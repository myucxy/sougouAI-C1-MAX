/* Three exclusive audio owners, serviced by V0118's existing clock worker. */
#include "record_mode.h"
#include "music.h"
#include "../xiaozhi-official/platform.h"
#include "../xiaozhi-official/session.h"
#include "../xiaozhi-official/audio.h"
#include "../xiaozhi-official/clock.h"
#define FN(r,a,...) ((r(*)(__VA_ARGS__))((uintptr_t)(a)|1u))
#define U8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define U32(a) (*(volatile uint32_t *)(uintptr_t)(a))
static struct {volatile unsigned ready,active,requested,stage,since,error,page,retries,retry_at,destination;} R;
unsigned record_mode_active(void){return R.ready?R.active:FN(unsigned,0x1803ed68,void)()?RECORD_MODE:XIAOZHI_MODE;}
unsigned record_mode_pending(void){return R.requested!=0;}
unsigned record_mode_error(void){return R.error;}
int record_ai_allowed(void){return record_mode_active()==XIAOZHI_MODE&&!R.requested;}
unsigned record_home_page(void){unsigned mode=record_mode_active();return mode==RECORD_MODE?RECORD_PAGE:mode==MUSIC_MODE?MUSIC_PAGE:AI_HOME_PAGE;}
void record_page_ready(unsigned mode){unsigned k=xp_lock();if(!R.ready){R.active=mode;R.ready=1;}if(!R.stage&&R.requested==mode)R.requested=0;xp_unlock(k);}
void record_page_init(void){FN(void,0x180567fc,void)();record_page_ready(RECORD_MODE);}
int record_navigate(unsigned page){
 if(page!=AI_HOME_PAGE&&page!=AI_MENU_PAGE&&page!=MUSIC_PAGE)return 0;
 unsigned k=xp_lock();
 if(!R.ready||R.requested||FN(unsigned,0x1803ed68,void)()||xp_usb()){xp_unlock(k);return 0;}
 R.requested=page==MUSIC_PAGE?MUSIC_MODE:XIAOZHI_MODE;R.destination=page;R.error=0;xp_unlock(k);
 if(xt_start())return 1;
 k=xp_lock();R.requested=R.destination=0;R.error=2;xp_unlock(k);return 0;
}
int record_feature_request(unsigned feature){
 if(feature!=XIAOZHI_MODE&&feature!=MUSIC_MODE)return 0;
 return record_navigate(feature==MUSIC_MODE?MUSIC_PAGE:AI_HOME_PAGE);
}
void record_mode_request(unsigned raw){
 if(raw>2)return;
 unsigned k=xp_lock();R.requested=raw==1?RECORD_MODE:XIAOZHI_MODE;R.destination=0;R.error=0;xp_unlock(k);
 if(!xt_start()){k=xp_lock();R.requested=0;R.error=2;xp_unlock(k);}
}
static void send_page(void){
 unsigned k=xp_lock(),page=R.page;xp_unlock(k);
 if(!page||!U32(0x20011de0)||FN(int,0x1805b53c,unsigned)(page)!=1)return;
 k=xp_lock();if(R.page==page)R.page=0;xp_unlock(k);
}
void record_mode_poll(void){
 /* Original selector also defers handoff while application startup is pending. */
 if(!R.ready||U8(0x2000f338))return;
 if(xp_usb()){if(R.active==MUSIC_MODE)music_close();return;}
 unsigned k=xp_lock(),target=R.requested;
 if(!target){unsigned active=R.active;xp_unlock(k);if(active==MUSIC_MODE)music_poll();send_page();return;}
 if(!R.stage&&target==R.active){if(R.destination)R.page=R.destination;R.requested=R.destination=0;xp_unlock(k);send_page();return;}
 unsigned first=!R.stage;
 if(first){R.stage=1;R.since=xp_ticks();R.retries=0;}
 xp_unlock(k);
 if(first){
  if(R.active==RECORD_MODE){
   if(U8(0x2001185c))FN(void,0x18045758,void)();
   FN(void,0x1804684c,void)();
  }else if(R.active==XIAOZHI_MODE)xs_cancel();
 }
 int music_released=R.active!=MUSIC_MODE||music_close();
 int released=music_released&&xs_quiescent()&&!U8(0x2001185c);
 uintptr_t player=U32(0x200118c8);
 if(released&&player&&FN(unsigned,0x180a4464,void *)((void *)player))released=0;
 if(released&&!xa_quiescent()){
  unsigned now=xp_ticks();
  if(R.retries<3&&(!R.retries||(int32_t)(now-R.retry_at)>=0)){
   R.retries++;R.retry_at=now+250u;xa_capture_close();xa_playback_close();
  }
  released=xa_quiescent();
 }
 k=xp_lock();
 if(released){R.active=R.requested;R.requested=R.stage=R.error=0;R.page=R.destination?R.destination:R.active==RECORD_MODE?RECORD_PAGE:R.active==MUSIC_MODE?MUSIC_PAGE:AI_HOME_PAGE;R.destination=0;}
 else if((unsigned)(xp_ticks()-R.since)>=15000u){R.error=1;R.requested=R.stage=R.destination=0;}
 xp_unlock(k);send_page();
}
const void *record_mode_state(void){return &R;}
