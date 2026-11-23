/* Reuse shipped pdmc capture driver and original 16k/16-bit/4-channel ABI. */
#include "audio.h"
#include "platform.h"
#include <stddef.h>
#include <string.h>
#include "addresses.h"
#define FN(ret,a,...) ((ret(*)(__VA_ARGS__))((uintptr_t)(a)|1u))
#define U8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define U16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define U32(a) (*(volatile uint32_t *)(uintptr_t)(a))
static struct {void *device,*dma;unsigned opened,prepared;} A;
static struct {unsigned step,started,reads,frames,empty;} D;
struct params {uint32_t rate;uint8_t bits,channels;uint16_t reserved;};
struct audio_buffer {void *buffer;uint32_t frames,period;};
static uintptr_t pcm_state(void *device){
 uintptr_t dev=(uintptr_t)device;if(!dev||U32(dev+0x24)!=0x18113544u||U32(dev+0x38)!=dev)return 0;
 uintptr_t pcm=U32(dev+0x40);return pcm>=0x20000000u&&pcm<0x20200000u?pcm:0;
}
static int buffer_available(void *device,int playback,unsigned *contiguous,unsigned *capacity){
 uintptr_t pcm=pcm_state(device);if(!pcm)return -1;
 unsigned key=xp_lock(),hw=U32(pcm+0x1c),app=U32(pcm+0x20),boundary=U32(pcm+0x24),size=U32(pcm+0x14);xp_unlock(key);
 /* rk_pcm_init doubles the ring size up to INT32_MAX: 1280 -> 0x50000000. */
 if(!size||boundary<size||boundary>0x7fffffffu||size>4096||boundary%size||hw>=boundary||app>=boundary)return -1;
 unsigned n=hw>=app?hw-app:boundary-(app-hw);
 if(playback){n+=size;if(n>=boundary)n-=boundary;}
 *contiguous=size-app%size;if(capacity)*capacity=size;return n<=size?(int)n:-1;
}
static unsigned available(void *device,int playback,unsigned *contiguous){int n=buffer_available(device,playback,contiguous,0);return n>0?(unsigned)n:0;}
static int control(unsigned command,void *argument){return FN(int,0x18061a68,void *,unsigned,void *)(A.device,command,argument);}
int xa_capture_close(void){
 int ok=1;
 if(A.opened&&control(0x1007,0))return 0;
 if(A.prepared){if(control(0x101f,0))ok=0;else A.prepared=0;}
 if(A.opened){if(FN(int,0x18061920,void *)(A.device))ok=0;else A.opened=0;}
 /* An unconfirmed DMA release must not free memory still visible to the driver. */
 if(!A.prepared&&!A.opened){if(A.dma)FN(void,0x18036a10,void *)(A.dma);A.device=A.dma=0;}
 return ok;
}
int xa_capture_open(void){
 D=(__typeof__(D)){0};D.started=xp_ticks();D.step=1;
 if(xp_usb()||A.device||U8(0x2001185c))return 0;
 void *device=FN(void *,0x1806175c,const char *)("pdmc");if(!device||U8((uintptr_t)device+0x1a))return 0;
 D.step=2;A.device=device;A.dma=FN(void *,0x18036a04,unsigned)(320u*4u*4u*2u);if(!A.dma){A.device=0;return 0;}
 if(FN(int,0x18061860,void *,unsigned)(device,1)){xa_capture_close();return 0;}A.opened=1;D.step=3;
 struct audio_buffer buffer={A.dma,1280,320};
 A.prepared=1;if(control(0x101e,&buffer)){xa_capture_close();return 0;}
 D.step=4;struct params cfg={16000,16,4,0};
 if(control(0x1001,&cfg)){xa_capture_close();return 0;}D.step=5;
 if(!pcm_state(device)||control(0x1006,0)){xa_capture_close();return 0;}D.step=6;
 return 1;
}
int xa_capture_read(int16_t *mono,unsigned cap){
 if(!A.device||!A.opened||!A.prepared||!mono||cap<320||xp_usb())return -1;
 D.reads++;unsigned contiguous=0;if(available(A.device,0,&contiguous)<320||contiguous<320){D.empty++;return 0;}
 int16_t input[320*4];
 int frames=FN(int,0x18061988,void *,unsigned,void *,unsigned)(A.device,0,input,320);
 if(frames<=0||frames>320)return -1;
 /* Initial bring-up uses the first physical microphone channel, no unverified DSP layout. */
 for(unsigned i=0;i<(unsigned)frames;i++)mono[i]=input[i*4];D.frames+=(unsigned)frames;return frames;
}

#define PLAYBACK_FADE_FRAMES 160u
#define PLAYBACK_QUIET_FRAMES 512u
static struct {void *device,*dma;unsigned opened,prepared,ending,final,tail_written,finish_started,fade_in,parked;uint8_t automute_saved,automute_before;int16_t last_sample;} P;
static int ram_pointer(uintptr_t address,unsigned bytes){return !(address&3u)&&address>=0x20000000u&&address<=0x20200000u-bytes;}
static int playback_automute(unsigned restore){
 uintptr_t device=(uintptr_t)P.device;if(!ram_pointer(device,0x6a))return 0;
 uintptr_t card=U32(device+0x3c);if(!ram_pointer(card,12))return 0;
 uintptr_t codec=U32(card+8);if(!ram_pointer(codec,0x18)||U32(codec+0x10)!=0x1811360cu)return 0;
 uintptr_t client=U32(codec+0x14);if(!ram_pointer(client,0x42))return 0;
 uintptr_t bus=U32(client+0x3c);unsigned address=U16(client+0x40);
 if(!ram_pointer(bus,4)||!address||address>0x7fu)return 0;
 struct message {uint16_t address,flags,length,reserved;void *data;};
 uint8_t reg=0x13,value=0;
 struct message read[2]={{address,0,1,0,&reg},{address,1,1,0,&value}};
 /* Native helpers treat a short transfer of zero messages as success. */
 if(FN(int,0x180a0fb0,uintptr_t,void *,unsigned)(bus,read,2)!=2)return 0;
 /* Remember before writing: a failed transfer may still have reached the codec. */
 if(!restore){P.automute_before=value&1u;P.automute_saved=1;}
 uint8_t before=value;value=(uint8_t)((value&~1u)|(restore?P.automute_before:0));
 uint8_t data[2]={reg,value};struct message write={address,0,2,0,data};
 if(value!=before&&FN(int,0x180a0fb0,uintptr_t,void *,unsigned)(bus,&write,1)!=1)return 0;
 if(restore)P.automute_saved=0;
 return 1;
}
int xa_playback_close(void){
 int ok=1;
 /* Native XRUN/STOP already stopped DMA, DAI and codec; avoid doing it twice. */
 if(P.opened&&U8((uintptr_t)P.device+0x69)!=4&&U8((uintptr_t)P.device+0x69)!=5&&
    FN(int,0x18061a68,void *,unsigned,void *)(P.device,0x1007,0))return 0;
 if(P.automute_saved&&!playback_automute(1))return 0;
 if(P.prepared){if(FN(int,0x18061a68,void *,unsigned,void *)(P.device,0x101f,0))ok=0;else P.prepared=0;}
 if(P.opened){if(FN(int,0x18061920,void *)(P.device))ok=0;else P.opened=0;}
 if(!P.prepared&&!P.opened){if(P.dma)FN(void,0x18036a10,void *)(P.dma);P=(__typeof__(P)){0};}return ok;
}
int xa_playback_open(void){
 if(xp_usb()||P.device)return 0;
 void *device=FN(void *,0x1806175c,const char *)("es8156p");if(!device||U8((uintptr_t)device+0x1a))return 0;
 P=(__typeof__(P)){0};P.device=device;P.dma=FN(void *,0x18036a04,unsigned)(2048u*2u*2u);if(!P.dma){P.device=0;return 0;}
 /* DMA may fetch a whole period beyond a short last write. */
 memset(P.dma,0,2048u*2u*2u);
 if(FN(int,0x18061860,void *,unsigned)(device,2)){xa_playback_close();return 0;}P.opened=1;
 struct audio_buffer buffer={P.dma,2048,256};P.prepared=1;
 if(FN(int,0x18061a68,void *,unsigned,void *)(device,0x101e,&buffer)){xa_playback_close();return 0;}
 struct params cfg={16000,16,2,0};
 if(FN(int,0x18061a68,void *,unsigned,void *)(device,0x1001,&cfg)||!pcm_state(device)||!playback_automute(0)){xa_playback_close();return 0;}
 return 1;
}
int xa_playback_write(const int16_t *mono,unsigned frames){
 if(!P.opened||!P.prepared||!mono||!frames||frames>1920||xp_usb())return -1;
 unsigned contiguous=0,avail=available(P.device,1,&contiguous);unsigned n=frames;if(n>256)n=256;if(n>contiguous)n=contiguous;if(n>avail)n=avail;if(!n)return 0;
 if(U8((uintptr_t)P.device+0x69)==4||U8((uintptr_t)P.device+0x69)==5)return -1;
 if(P.ending){P.ending=P.final=P.parked=P.tail_written=0;P.fade_in=PLAYBACK_FADE_FRAMES;}
 int16_t stereo[256*2];for(unsigned i=0;i<n;i++){
  int32_t sample=mono[i];if(i<P.fade_in)sample=sample*(int32_t)(PLAYBACK_FADE_FRAMES-P.fade_in+i+1u)/(int32_t)PLAYBACK_FADE_FRAMES;
  stereo[2*i]=stereo[2*i+1]=(int16_t)(sample/2);
 }
 unsigned wrote=FN(unsigned,0x180619f8,void *,unsigned,const void *,unsigned)(P.device,0,stereo,n);
 if(wrote!=n)return -1;P.last_sample=(int16_t)(stereo[2*(n-1)]*2);P.fade_in=n<P.fade_in?P.fade_in-n:0;return (int)n;
}

/* Keep clocks and bias stable between replies; only explicit close powers down. */
static int playback_tail(unsigned final){
 if(!P.device)return 1;
 if(!P.opened||!P.prepared||xp_usb())return -1;
 unsigned driver=U8((uintptr_t)P.device+0x69);
 if(driver==4||driver==5)return -1;
 if(final&&!P.final){P.final=1;P.finish_started=xp_ticks();}
 if(P.final&&!P.parked&&(unsigned)(xp_ticks()-P.finish_started)>1500u)return -1;
 unsigned contiguous=0,size=0;int free_frames=buffer_available(P.device,1,&contiguous,&size);
 if(free_frames<0)return -1;
 unsigned pending=size-(unsigned)free_frames;
 /* A late network stop must not let the hardware reach XRUN first. */
 if(!P.ending){if(!final&&pending>=512u)return 0;P.ending=1;P.tail_written=0;}
 if(final&&P.tail_written>=PLAYBACK_FADE_FRAMES+PLAYBACK_QUIET_FRAMES&&
    pending<=P.tail_written-PLAYBACK_FADE_FRAMES-PLAYBACK_QUIET_FRAMES)P.parked=1;
 for(unsigned chunk=0;chunk<8;chunk++){
  free_frames=buffer_available(P.device,1,&contiguous,0);if(free_frames<0)return -1;
  unsigned n=(unsigned)free_frames;if(n>256)n=256;if(n>contiguous)n=contiguous;if(!n)break;
  int16_t stereo[256*2];
  for(unsigned i=0;i<n;i++){
   unsigned at=P.tail_written+i;int32_t sample=at<PLAYBACK_FADE_FRAMES?
    (int32_t)P.last_sample*(int32_t)(PLAYBACK_FADE_FRAMES-1u-at)/(int32_t)PLAYBACK_FADE_FRAMES:0;
   stereo[2*i]=stereo[2*i+1]=(int16_t)(sample/2);
  }
  if(FN(unsigned,0x180619f8,void *,unsigned,const void *,unsigned)(P.device,0,stereo,n)!=n)return -1;
  /* Saturate after a complete ring of silence, including days of idle time. */
  if(P.tail_written<PLAYBACK_FADE_FRAMES+PLAYBACK_QUIET_FRAMES+size)P.tail_written+=n;
 }
 return final&&P.parked?1:0;
}

int xa_playback_parked(void){return P.device&&P.opened&&P.prepared&&P.parked&&U8((uintptr_t)P.device+0x69)==3&&!xp_usb();}
int xa_playback_finish(void){return playback_tail(1);}
int xa_playback_keepalive(void){return playback_tail(0);}

void xa_capture_report(unsigned error){
 if(xp_usb())return;
 unsigned driver=0,hw=0,app=0,size=0,boundary=0;uintptr_t pcm=pcm_state(A.device);
 if(pcm){unsigned key=xp_lock();driver=U8((uintptr_t)A.device+0x69);hw=U32(pcm+0x1c);app=U32(pcm+0x20);size=U32(pcm+0x14);boundary=U32(pcm+0x24);xp_unlock(key);}
 const char *names[]={"AUDIO_ERROR","CAPTURE_STEP","ELAPSED_MS","READ_CALLS","PCM_FRAMES","EMPTY_READS","DRIVER_STATE","HW_PTR","APP_PTR","BUFFER_FRAMES","BOUNDARY"};
 unsigned values[]={error,D.step,xp_ticks()-D.started,D.reads,D.frames,D.empty,driver,hw,app,size,boundary};
 char out[384];unsigned n=0;const char *prefix="BOOT_" C208_AI_VERSION "\n";while(*prefix)out[n++]=*prefix++;
 for(unsigned i=0;i<11;i++){const char *p=names[i];while(*p)out[n++]=*p++;out[n++]='=';for(int shift=28;shift>=0;shift-=4)out[n++]="0123456789abcdef"[(values[i]>>shift)&15];out[n++]='\n';}
 int fd=FN(int,0x1806a0cc,const char *,int,int)("/XZAUDIO.TXT",0x602,0);if(fd<0)return;
 FN(int,0x1806a1a4,int,const void *,unsigned)(fd,out,n);FN(int,0x1806a2f0,int)(fd);FN(int,0x1806a120,int)(fd);
}

int xa_quiescent(void){return !A.device&&!P.device;}
