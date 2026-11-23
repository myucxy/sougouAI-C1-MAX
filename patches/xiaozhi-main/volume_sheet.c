/* Created only on a foreground swipe; styles belong to the native page pool. */
#include "volume_sheet.h"
#include "record_mode.h"
#include "music.h"
#include "../xiaozhi-official/platform.h"
#include <stdint.h>
#include <string.h>
#define FN(r,a,...) ((r(*)(__VA_ARGS__))((uintptr_t)(a)|1u))
#define U8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define U16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define U32(a) (*(volatile uint32_t *)(uintptr_t)(a))
static struct {
 void *root,*cover,*panel,*bar,*value,*minus,*plus,*touch,*clear,*body,*handle_style;
 unsigned open,percent,opened_at;
} W;
/* Touch callbacks only post requests. All widgets are owned by the display task. */
static struct {
 void *root;void (*previous)(int,int);
 int16_t x,y;unsigned tracking,pending;
} P;
#define VOLUME_MESSAGE 250u
/* Stock rectangle drawing reads body opacity at +8; +15 is border opacity. */
#define STYLE_BODY_OPA 8u
#define STYLE_BORDER_OPA 15u

static int foreground(void *root){
 unsigned page=U16(0x20011dda);
 return root&&U32(0x20011dc8)==(uint32_t)(uintptr_t)root&&(page==AI_HOME_PAGE||page==MUSIC_PAGE)&&U8(0x20011c2d)&&!xp_usb()&&!record_mode_pending();
}
static int eligible(void *root){return foreground(root)&&(U16(0x20011dda)!=MUSIC_PAGE||music_page_volume_allowed());}
static int request(void *root,unsigned action){
 unsigned key=xp_lock();
 if(P.root!=root||!eligible(root)){xp_unlock(key);return 0;}
 if(P.pending){xp_unlock(key);return 1;}
 P.pending=action;xp_unlock(key);
 if(!FN(int,0x1805b5e8,unsigned,uintptr_t,unsigned)(VOLUME_MESSAGE,(uintptr_t)root,0)){
  key=xp_lock();P.pending=0;xp_unlock(key);return 0;
 }
 return 1;
}
void volume_sheet_request(void *root){request(root,1);}
static void touch_point(int x,int y){
 void (*previous)(int,int)=P.previous;if(previous)previous(x,y);
 if(!eligible(P.root)){P.tracking=0;return;}
 unsigned down=U32(0x200205c4);
 if(!P.tracking){if(down){P.x=(int16_t)x;P.y=(int16_t)y;P.tracking=1;}return;}
 /* A fast swipe may contain just press and release; use the release coordinates too. */
 if(P.tracking==1){
  int dx=x-P.x,dy=y-P.y;if(dx<0)dx=-dx;
  if(dy<=-48&&-dy>dx*2){P.tracking=2;if(!W.open)request(P.root,1);}
  else if(dy>=48&&dy>dx*2){P.tracking=2;if(W.open)request(P.root,2);}
 }
 if(!down){if(P.tracking==2&&W.open)W.opened_at=xp_ticks();P.tracking=0;}
}
void volume_sheet_bind(void *root){
 uintptr_t context=U32(0x200205ac);if(!root||!context)return;
 P.root=root;
 if(U32(context+8)==(uint32_t)(uintptr_t)touch_point)return;
 P.previous=(void (*)(int,int))(uintptr_t)U32(context+8);
 FN(void,0x180869c0,void *)(touch_point);
}
static void bounds(void *object,int x,int y,unsigned width,unsigned height){
 FN(void,0x18089b70,void *,unsigned,unsigned)(object,width,height);
 FN(void,0x18088eb0,void *,int,int)(object,x,y);
}
static void *copy_style(void *source){return FN(void *,0x1805b83c,unsigned)(FN(unsigned,0x1805b7f4,void *)(source));}
static void close_sheet(void){if(W.cover)FN(void,0x1808970c,void *,unsigned)(W.cover,1);W.open=0;}
static int interactive(void){return W.open&&foreground(W.root)&&(unsigned)(xp_ticks()-W.opened_at)>=150u;}
static void outside(void *object,unsigned event){if(object==W.cover&&event==6&&interactive())close_sheet();}
static void refresh(void){
 if(!W.open||!W.bar||!W.value)return;
 unsigned percent=FN(unsigned,0x18041f44,void)();if(percent>100)percent=100;
 if(percent==W.percent)return;W.percent=percent;
 FN(void,0x18093ab0,void *,int,unsigned)(W.bar,(int)percent,0);
 char text[5];unsigned n=0;if(percent==100)text[n++]='1';if(percent>=10)text[n++]=(char)('0'+percent/10%10);text[n++]=(char)('0'+percent%10);text[n++]='%';text[n]=0;
 FN(void,0x18098084,void *,const char *)(W.value,text);
}
static void apply(unsigned percent){
 if(!interactive())return;if(percent>100)percent=100;
 if(percent!=FN(unsigned,0x18041f44,void)())FN(int,0x18041f50,int)((int)percent);
 refresh();
}
static void adjust(void *object,unsigned event){
 if(event!=6||(object!=W.minus&&object!=W.plus)||!interactive())return;
 unsigned value=FN(unsigned,0x18041f44,void)();if(value>100)value=100;
 if(object==W.minus)value=value>10?value-10:0;else value=value<90?value+10:100;
 apply(value);
}
static void track(void *object,unsigned event){
 if(object!=W.touch||(event!=0&&event!=1)||!interactive())return;
 int x=(int16_t)U16(0x200205c0);unsigned value=x<=55?0:x>=184?100:(unsigned)(x-55)*100u/129u;
 apply((value+5u)/10u*10u);
}
static void *object(void *parent,void *style,int x,int y,unsigned width,unsigned height){
 void *p=FN(void *,0x180897c0,void *,void *)(parent,0);if(!p)return 0;
 FN(void,0x1808969c,void *,void *)(p,style);bounds(p,x,y,width,height);FN(void,0x180885bc,void *,unsigned)(p,1);return p;
}
static void *label(const char *value,int x,int y,unsigned width){
 void *p=FN(void *,0x18098300,void *,void *)(W.panel,0);if(!p)return 0;
 FN(void,0x1808969c,void *,void *)(p,W.clear);
 FN(void,0x180981f0,void *,unsigned)(p,2);FN(void,0x18097548,void *,unsigned)(p,0);FN(void,0x180885bc,void *,unsigned)(p,0);
 bounds(p,x,y,width,30);FN(void,0x18098084,void *,const char *)(p,value);return p;
}
static int button(int x,unsigned minus,uintptr_t released,uintptr_t pressed){
 void *p=FN(void *,0x18097188,void *,void *)(W.panel,0);if(!p)return 0;
 FN(void,0x1805b9d4,void *,uintptr_t,uintptr_t)(p,released,pressed);bounds(p,x,50,48,48);
 if(minus)W.minus=p;else W.plus=p;
 FN(void,0x180885bc,void *,unsigned)(p,1);FN(void,0x18088758,void *,void *)(p,adjust);return 1;
}
int volume_sheet_open(void *root){
 if(!eligible(root)||(W.root&&W.root!=root))return 0;
 if(!W.handle_style){
  unsigned free=0;for(unsigned i=0;i<10;i++)free+=!U8(0x20011e6cu+i);
  void *base=FN(void *,0x1808a218,void *)(root);
  if(free<3||!base||!FN(int,0x18086b9c,void *)(base))return 0;
  W.root=root;W.clear=copy_style(base);W.body=copy_style(base);W.handle_style=copy_style(base);
  U8((uintptr_t)W.clear+STYLE_BODY_OPA)=0;U8((uintptr_t)W.clear+STYLE_BORDER_OPA)=0;
  U16((uintptr_t)W.body+2)=U16((uintptr_t)W.body+4)=0;U8((uintptr_t)W.body+STYLE_BODY_OPA)=255;U8((uintptr_t)W.body+STYLE_BORDER_OPA)=0;
  U16((uintptr_t)W.handle_style+2)=U16((uintptr_t)W.handle_style+4)=0x8410;U8((uintptr_t)W.handle_style+STYLE_BODY_OPA)=255;U8((uintptr_t)W.handle_style+STYLE_BORDER_OPA)=0;
 }
 if(!W.cover){
  W.cover=object(root,W.clear,0,0,240,240);if(!W.cover)goto failed;
  FN(void,0x18088758,void *,void *)(W.cover,outside);
  W.panel=object(W.cover,W.body,0,120,240,120);if(!W.panel)goto failed;
  if(!label("音量大小",8,14,160))goto failed;
  W.value=label("",172,14,64);if(!W.value)goto failed;
  W.bar=FN(void *,0x18093cb4,void *,void *)(W.panel,0);if(!W.bar)goto failed;
  bounds(W.bar,55,71,129,6);FN(void,0x18093b94,void *,int,int)(W.bar,0,100);
  FN(void,0x1805ba10,void *,unsigned,unsigned)(W.bar,U16(0x180e0f0c),U16(0x180e0f10));
  if(!button(0,1,0x1819e6fc,0x1819f908)||!button(192,0,0x1819c2e4,0x1819d4f0))goto failed;
  W.touch=object(W.panel,W.clear,52,50,136,48);if(!W.touch)goto failed;
  FN(void,0x18088758,void *,void *)(W.touch,track);
  /* This hint reuses the third style; allocation failure leaves volume controls usable. */
  void *handle=object(W.panel,W.handle_style,98,5,44,3);
  if(handle)FN(void,0x180885bc,void *,unsigned)(handle,0);
 }
 W.open=1;W.percent=~0u;W.opened_at=xp_ticks();refresh();FN(void,0x1808970c,void *,unsigned)(W.cover,0);return 1;
failed:
 /* Keep page-owned styles for a retry; destroy every control before clearing refs. */
 if(W.cover)FN(void,0x1808a0d8,void *)(W.cover);
 W.cover=W.panel=W.bar=W.value=W.minus=W.plus=W.touch=0;W.open=0;return 0;
}
unsigned volume_sheet_active(void){return W.open||P.pending;}
int volume_sheet_gesture(unsigned gesture){
 if(!W.open&&!P.pending)return P.tracking==2||(gesture==6&&P.tracking);
 if(W.open&&gesture==7&&interactive()&&P.tracking!=2)request(P.root,2);
 return 1;
}
void volume_sheet_message(const unsigned char *message){
 if(!message)return;
 if(message[0]==VOLUME_MESSAGE){
  uintptr_t root=(uintptr_t)message[4]|((uintptr_t)message[5]<<8)|((uintptr_t)message[6]<<16)|((uintptr_t)message[7]<<24);
  if(root!=(uintptr_t)P.root)return;
  unsigned key=xp_lock(),action=P.pending;P.pending=0;xp_unlock(key);
  if(!eligible(P.root))return;
  if(action==1)volume_sheet_open(P.root);else if(action==2)close_sheet();
 }else if(message[0]==21&&W.open)refresh();
}
void volume_sheet_destroy(void){
 uintptr_t context=U32(0x200205ac);
 if(context&&U32(context+8)==(uint32_t)(uintptr_t)touch_point)FN(void,0x180869c0,void *)(P.previous);
 memset(&P,0,sizeof(P));
 W.open=0;
 if(W.cover)FN(void,0x1808a0d8,void *)(W.cover);
 /* Style bytes remain valid until the original dispatcher resets its page pool. */
 memset(&W,0,sizeof(W));
}
