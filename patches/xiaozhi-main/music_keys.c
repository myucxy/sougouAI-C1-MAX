/* The physical REC key is key 2; touchscreen controls keep their own actions. */
#include "music.h"
#include "record_mode.h"
#include "../xiaozhi-official/platform.h"
#define DOUBLE_MS 320u
#define HOLD_MS 600u
static struct {unsigned down,down_at,released_at,pending,second,long_fired,queued;} K;
static int enabled(void){return record_mode_active()==MUSIC_MODE&&!record_mode_pending()&&!xp_usb()&&music_loaded()&&(music_page_keys_allowed()||music_settings_active());}
void music_keys_reset(void){unsigned key=xp_lock();K.down=K.down_at=K.released_at=K.pending=K.second=K.long_fired=K.queued=0;xp_unlock(key);}
void music_keys_event(unsigned type){
 if(!enabled()){music_keys_reset();return;}
 unsigned key=xp_lock(),now=xp_ticks();
 if(type==1){
  if(!K.down){
   if(K.pending&&(unsigned)(now-K.released_at)>=DOUBLE_MS){K.pending=0;K.queued=MUSIC_TOGGLE;}
   K.second=K.pending;K.down=1;K.down_at=now;K.long_fired=0;
  }
 }else if((type==3||type==4)&&K.down){
  K.down=0;
  if(K.long_fired){K.pending=K.second=0;}
  else if((unsigned)(now-K.down_at)>=HOLD_MS){K.pending=K.second=0;K.long_fired=1;K.queued=MUSIC_PREVIOUS_TRACK;}
  else if(K.second){K.pending=K.second=0;K.queued=MUSIC_NEXT_TRACK;}
  else{K.pending=1;K.released_at=now;}
 }
 xp_unlock(key);
}
void music_keys_poll(void){
 if(!enabled()){music_keys_reset();return;}
 unsigned key=xp_lock(),now=xp_ticks();
 if(K.down&&!K.long_fired&&(unsigned)(now-K.down_at)>=HOLD_MS){
  K.pending=K.second=0;K.long_fired=1;K.queued=MUSIC_PREVIOUS_TRACK;
 }else if(!K.down&&K.pending&&(unsigned)(now-K.released_at)>=DOUBLE_MS){K.pending=0;K.queued=MUSIC_TOGGLE;}
 /* Queue admission only touches bounded state; retain an action until accepted. */
 if(K.queued&&music_action(K.queued))K.queued=0;
 xp_unlock(key);
}
