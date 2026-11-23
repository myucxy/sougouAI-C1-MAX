/* Small SNTP bootstrap. The existing RTC and timezone remain the system clock. */
#include "clock.h"
#include "../xiaozhi-main/record_mode.h"
#include "platform.h"
#include <string.h>
#define FN(ret,a,...) ((ret(*)(__VA_ARGS__))((uintptr_t)(a)|1u))
#define RETRY_MS 60000u
#define REFRESH_MS 21600000u
static struct {volatile unsigned started,state,attempted;unsigned linked,last_attempt,last_success,sequence;} T;
static uint32_t be32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint64_t epoch(uint32_t seconds){
 uint64_t n=seconds;if(n<2208988800ULL)n+=4294967296ULL;
 n-=2208988800ULL;return n>=1735689600ULL&&n<=4102444800ULL?n:0;
}
void xt_request(uint8_t packet[48],const uint8_t cookie[8]){memset(packet,0,48);packet[0]=0x23;packet[2]=6;memcpy(packet+40,cookie,8);}
uint64_t xt_response(const uint8_t *p,unsigned n,const uint8_t cookie[8],unsigned elapsed){
 if(!p||!cookie||n!=48||elapsed>4000u||(p[0]>>6)==3||(p[0]&7)!=4||((p[0]>>3)&7)<3||((p[0]>>3)&7)>4||!p[1]||p[1]>15||memcmp(p+24,cookie,8))return 0;
 uint64_t receive=epoch(be32(p+32)),transmit=epoch(be32(p+40));
 if(!receive||!transmit||transmit<receive||transmit-receive>2||(!be32(p+40)&&!be32(p+44))||be32(p+8)>16u*65536u)return 0;
 /* Round down to the RTC's seconds; compensate at most half the measured RTT. */
 uint64_t now=transmit+(((uint64_t)be32(p+44)*1000u>>32)+elapsed/2u)/1000u;
 return now<=4102444800ULL?now:0;
}
unsigned xt_state_value(void){return T.state;}
int xt_sync(void){
 static const char *const hosts[]={"ntp.aliyun.com","time.cloudflare.com"};
 if(!xp_network_ready()||xp_usb())return 0;
 T.state=XT_SYNCING;uint8_t mac[6]={0};xp_mac(mac);
 for(unsigned host=0;host<2&&xp_network_ready()&&!xp_usb();host++){
  int fd=xp_ntp_open(hosts[host]);if(fd<0)continue;
  uint8_t cookie[8],request[48],response[64];unsigned tick=xp_ticks(),seq=++T.sequence;
  for(unsigned i=0;i<8;i++)cookie[i]=(uint8_t)((i<4?tick:seq)>>(8*(i&3)))^(i<6?mac[i]:0x5a);cookie[7]|=1;
  xt_request(request,cookie);unsigned start=xp_ticks();int sent=0,ok=0;
  while(xp_network_ready()&&!xp_usb()&&(unsigned)(xp_ticks()-start)<4000u){
   if(!sent){int n=xp_write(fd,request,sizeof(request));if(n<0||(n>0&&n!=48))break;sent=n==48;}
   if(sent){int n=xp_read(fd,response,sizeof(response));if(n<0)break;
    if(n>0){uint64_t now=xt_response(response,(unsigned)n,cookie,xp_ticks()-start);if(now&&xp_set_time(now)){ok=1;break;}}
   }
   record_mode_poll();xp_delay(20);
  }
  xp_close(fd);
  if(ok){T.state=XT_SYNCED;T.attempted=1;T.last_success=xp_ticks();xp_time_log(XT_SYNCED,0,xp_time());return 1;}
 }
 T.state=XT_FAILED;T.attempted=1;xp_time_log(XT_FAILED,1,xp_time());return 0;
}
void xt_poll(void){
 record_mode_poll();
 if(!xp_network_ready()||xp_usb()){T.linked=0;return;}
 unsigned now=xp_ticks();
 if(!T.linked||(!T.attempted)||(T.state==XT_FAILED&&(unsigned)(now-T.last_attempt)>=RETRY_MS)||(T.state==XT_SYNCED&&(unsigned)(now-T.last_success)>=REFRESH_MS)){
  T.linked=1;T.last_attempt=now;xt_sync();
 }
}
void xt_worker(void *argument){(void)argument;for(;;){xt_poll();xp_delay(record_mode_active()==MUSIC_MODE?40:500);}}
int xt_start(void){
 unsigned key=FN(unsigned,0x1802b134,void)();if(T.started){FN(void,0x1802b13c,unsigned)(key);return 1;}T.started=1;FN(void,0x1802b13c,unsigned)(key);
 void *thread=FN(void *,0x18064604,const char *,void *,void *,unsigned,unsigned,unsigned)("xztime",xt_worker,0,3072,25,10);
 if(thread&&!FN(int,0x180648f8,void *)(thread))return 1;
 if(thread)FN(int,0x18064654,void *)(thread);T.started=0;T.state=XT_FAILED;T.attempted=1;xp_time_log(XT_FAILED,2,xp_time());return 0;
}
int xt_wait(void){
 if(!xt_start())return xp_time()!=0;
 unsigned start=xp_ticks();
 while(xp_network_ready()&&!xp_usb()&&(!T.linked||!T.attempted||T.state==XT_SYNCING)&&(unsigned)(xp_ticks()-start)<20000u)xp_delay(100);
 /* A blocked NTP service must not discard a valid battery-backed RTC. */
 return !xp_usb()&&xp_network_ready()&&xp_time()!=0;
}
