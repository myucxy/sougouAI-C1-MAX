/* Supplied C208 image only. Native APIs reuse existing drivers. */
#include "platform.h"
#include "identity.h"
#include "addresses.h"
#include <string.h>
#define FN(ret,a,...) ((ret(*)(__VA_ARGS__))((uintptr_t)(a)|1u))
#define U8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define U32(a) (*(volatile uint32_t *)(uintptr_t)(a))
unsigned xp_lock(void){return FN(unsigned,0x1802b134,void)();}
void xp_unlock(unsigned key){FN(void,0x1802b13c,unsigned)(key);}
unsigned xp_ticks(void){return FN(unsigned,0x1806161c,void)();}
void xp_delay(unsigned ms){FN(int,0x1806485c,unsigned)(ms);}
int xp_usb(void){return FN(int,0x18051294,void)();}
int xp_network_ready(void){return !xp_usb()&&U32(0x20020930)&&(U8(0x20020992)&4);}
void *xp_alloc(unsigned bytes){return FN(void *,0x18063b1c,unsigned,unsigned)(1,bytes);}
void xp_free(void *p){FN(void,0x18063b38,void *)(p);}
/* Exact original heap counters, sampled atomically; free is not largest block. */
void xp_memory_report(unsigned stage,unsigned requested){
 if(xp_usb())return;
 unsigned key=xp_lock(),total=U32(0x20012f2c),used=U32(0x20012f38),peak=U32(0x20012f28);xp_unlock(key);
 const char *names[]={"ALLOC_STAGE","REQUEST_BYTES","HEAP_TOTAL","HEAP_USED","HEAP_FREE","HEAP_PEAK"};
 unsigned values[]={stage,requested,total,used,used<=total?total-used:0,peak};
 char out[256];unsigned n=0;const char *p="BOOT_" C208_AI_VERSION "\n";while(*p)out[n++]=*p++;
 for(unsigned i=0;i<6;i++){p=names[i];while(*p)out[n++]=*p++;out[n++]='=';for(int shift=28;shift>=0;shift-=4)out[n++]="0123456789abcdef"[(values[i]>>shift)&15];out[n++]='\n';}
 int fd=FN(int,0x1806a0cc,const char *,int,int)("/XZMEM.TXT",0x602,0);if(fd<0)return;
 FN(int,0x1806a1a4,int,const void *,unsigned)(fd,out,n);FN(int,0x1806a2f0,int)(fd);FN(int,0x1806a120,int)(fd);
}
int xp_mac(uint8_t mac[6]){
 if(!xp_network_ready())return 0;memset(mac,0,6);
 if(FN(int,0x180d73f0,void *,unsigned)(mac,0))return 0;
 unsigned bits=0;for(unsigned i=0;i<6;i++)bits|=mac[i];return bits&&!(mac[0]&1);
}
static int rtc_control(unsigned command,uint64_t *time){
 void *rtc=FN(void *,0x1806175c,const char *)("rtc");if(!rtc)return 0;
 if(FN(int,0x18061860,void *,unsigned)(rtc,0))return 0;
 int result=FN(int,0x18061a68,void *,unsigned,void *)(rtc,command,time);
 int closed=FN(int,0x18061920,void *)(rtc);return result==0&&closed==0;
}
uint64_t xp_time(void){
 /* Original time() and RTC control use an eight-byte time_t on this image. */
 uint64_t t=0;if(!rtc_control(0x10,&t))return 0;
 return t>=1735689600ULL&&t<=4102444800ULL?t:0;
}
int xp_set_time(uint64_t utc){
 if(utc<1735689600ULL||utc>4102444800ULL||xp_usb())return 0;
 if(!rtc_control(0x11,&utc))return 0;uint64_t actual=xp_time();
 return actual>=utc&&actual-utc<=2;
}
static const char *path(unsigned bank){return bank?"/XZAUTH1.CFG":"/XZAUTH0.CFG";}
static int read_identity(void *ctx,unsigned bank,uint8_t out[XO_ID_BYTES]){
 (void)ctx;if(xp_usb())return 0;int fd=FN(int,0x1806a0cc,const char *,int,int)(path(bank),0,0);if(fd<0)return 0;
 uint8_t buffer[XO_ID_BYTES+1];int n=FN(int,0x1806a160,int,void *,unsigned)(fd,buffer,sizeof(buffer));int close=FN(int,0x1806a120,int)(fd);
 int ok=n==(int)XO_ID_BYTES&&close>=0;if(ok)memcpy(out,buffer,XO_ID_BYTES);volatile uint8_t *wipe=buffer;for(unsigned i=0;i<sizeof(buffer);i++)wipe[i]=0;return ok;
}
static int commit_identity(void *ctx,unsigned bank,const uint8_t record[XO_ID_BYTES]){
 (void)ctx;if(xp_usb())return 0;int fd=FN(int,0x1806a0cc,const char *,int,int)(path(bank),0x602,0);if(fd<0)return 0;
 int n=FN(int,0x1806a1a4,int,const void *,unsigned)(fd,record,XO_ID_BYTES);
 /* Actual dfs fsync wrapper. Refuse TLS if durable flush/readback is unavailable. */
 int sync=n==(int)XO_ID_BYTES?FN(int,0x1806a2f0,int)(fd):-1;int close=FN(int,0x1806a120,int)(fd);
 return n==(int)XO_ID_BYTES&&sync==0&&close==0&&!xp_usb();
}
int xp_identity(uint8_t mac[6],char uuid[37],uint8_t entropy[32],uint64_t now){
 struct xo_identity_io io={read_identity,commit_identity,0};return xp_mac(mac)&&xo_identity_reserve(&io,mac,now,uuid,entropy);
}
struct address4 {uint8_t len,family;uint16_t port;uint32_t ip;uint8_t zeros[8];};
static int connect_kind(const char *host,unsigned port,int type){
 if(!xp_network_ready()||!port||port>65535)return -1;
 /* hostent h_addr_list at +16 on the original ARM lwIP; copy before another DNS call. */
 uintptr_t answer=(uintptr_t)FN(void *,0x18070494,const char *)(host);if(!answer)return -1;
 uintptr_t list=U32(answer+16);if(!list||!U32(list))return -1;uint32_t ip=U32(U32(list));
 int fd=FN(int,0x18070e04,int,int,int)(2,type,0);if(fd<0)return -1;
 if(FN(int,0x1807107c,int,int,int)(fd,4,0x4000)){xp_close(fd);return -1;}
 struct address4 address={16,2,(uint16_t)((port<<8)|(port>>8)),ip,{0}};
 int result=FN(int,0x180709bc,int,const void *,unsigned)(fd,&address,sizeof(address));
 if(result<0){int error=*FN(int *,0x180db230,void)();if(error!=11&&error!=119&&error!=120){xp_close(fd);return -1;}}
 return fd;
}
int xp_connect(const char *host,unsigned port){return connect_kind(host,port,1);}
/* Connected UDP restricts incoming datagrams to the selected NTP address/port. */
int xp_ntp_open(const char *host){return connect_kind(host,123,2);}
static int would_block(void){int error=*FN(int *,0x180db230,void)();return error==11||error==119||error==120;}
int xp_read(int fd,void *buffer,unsigned n){
 int result=FN(int,0x18070c54,int,void *,unsigned,int)(fd,buffer,n,8);if(result>0)return result;return result<0&&would_block()?0:-1;
}
int xp_write(int fd,const void *buffer,unsigned n){
 int result=FN(int,0x18070d74,int,const void *,unsigned,int)(fd,buffer,n,8);if(result>0)return result;return result<0&&would_block()?0:-1;
}
void xp_close(int fd){if(fd>=0)FN(int,0x18070944,int)(fd);}
void xp_log(unsigned state,unsigned error,unsigned tls_error,unsigned http_status){
 if(xp_usb())return;char out[128];const char *prefix="BOOT_" C208_AI_VERSION "\n";unsigned n=0;while(*prefix)out[n++]=*prefix++;
 const char *names[]={"OFFICIAL_STATE","ERROR","TLS_ERROR","HTTP_STATUS"};unsigned values[]={state,error,tls_error,http_status};
 for(unsigned i=0;i<4;i++){const char *p=names[i];while(*p)out[n++]=*p++;out[n++]='=';for(unsigned j=0;j<8;j++)out[n++]="0123456789abcdef"[(values[i]>>(28-j*4))&15];out[n++]='\n';}
 int fd=FN(int,0x1806a0cc,const char *,int,int)("/XZDBG.TXT",0x602,0);if(fd>=0){FN(int,0x1806a1a4,int,const void *,unsigned)(fd,out,n);FN(int,0x1806a120,int)(fd);}
}

void xp_time_log(unsigned state,unsigned error,uint64_t utc){
 if(xp_usb())return;char out[160];unsigned n=0;const char *prefix="BOOT_" C208_AI_VERSION "\n";while(*prefix)out[n++]=*prefix++;
 const char *names[]={"TIME_STATE","ERROR","UTC_HIGH","UTC_LOW","TICKS"};unsigned values[]={state,error,(unsigned)(utc>>32),(unsigned)utc,xp_ticks()};
 for(unsigned i=0;i<5;i++){const char *p=names[i];while(*p)out[n++]=*p++;out[n++]='=';for(int shift=28;shift>=0;shift-=4)out[n++]="0123456789abcdef"[(values[i]>>shift)&15];out[n++]='\n';}
 int fd=FN(int,0x1806a0cc,const char *,int,int)("/XZTIME.TXT",0x602,0);if(fd<0)return;
 FN(int,0x1806a1a4,int,const void *,unsigned)(fd,out,n);FN(int,0x1806a2f0,int)(fd);FN(int,0x1806a120,int)(fd);
}
