#include "input.h"
static void reset(struct ai_effects *e){e->count=0;}
static void emit(struct ai_input *s,struct ai_effects *e,unsigned a){
 if(e->count<3)e->items[e->count++]=(struct ai_event){s->generation,a,s->automatic};
}
static void cancel(struct ai_input *s,struct ai_effects *e){
 if(s->phase!=AI_IDLE)emit(s,e,AI_CANCEL);
 s->generation++;s->phase=AI_IDLE;s->automatic=0;s->tap=0;
}
static void start(struct ai_input *s,struct ai_effects *e){s->generation++;s->phase=AI_LISTENING;emit(s,e,AI_START);}
void ai_init(struct ai_input *s,unsigned mode){*s=(struct ai_input){0};s->mode=mode<=AI_CONTINUOUS?mode:AI_HOLD;}
void ai_enter(struct ai_input *s,int online,struct ai_effects *e){reset(e);cancel(s,e);s->down=s->owned=0;s->active=1;s->online=!!online;}
void ai_leave(struct ai_input *s,struct ai_effects *e){reset(e);cancel(s,e);s->down=s->owned=0;s->active=0;}
void ai_set_mode(struct ai_input *s,unsigned mode,struct ai_effects *e){
 reset(e);if(mode>AI_CONTINUOUS||mode==s->mode)return;cancel(s,e);s->down=s->owned=0;s->mode=mode;
}
void ai_online(struct ai_input *s,int online,struct ai_effects *e){reset(e);if(!online){cancel(s,e);s->down=s->owned=0;}s->online=!!online;}
int ai_key(struct ai_input *s,unsigned key,unsigned type,uint32_t now,struct ai_effects *e){
 reset(e);if(key!=2)return 0;
 /* Retire local record actions on every page; only the AI home may capture. */
 if(type==1){
  if(s->owned)return 1;s->owned=1;s->down=1;s->down_at=now;
  if(!s->active)return 1;
  if(!s->online){emit(s,e,AI_NOT_READY);return 1;}
  if(s->mode==AI_HOLD){if(s->phase!=AI_IDLE)cancel(s,e);start(s,e);return 1;}
  if(s->mode==AI_CONTINUOUS){
   unsigned was_auto=s->automatic;cancel(s,e);
   if(!was_auto){s->automatic=1;emit(s,e,AI_AUTO);start(s,e);}return 1;
  }
  if(s->tap&&(uint32_t)(now-s->last_release)<=300u){
   unsigned was_auto=s->automatic;cancel(s,e);
   if(!was_auto){s->automatic=1;emit(s,e,AI_AUTO);start(s,e);}return 1;
  }
  s->tap=0;
  if(s->automatic)return 1;
  if(s->phase==AI_LISTENING){s->phase=AI_WAITING;emit(s,e,AI_SEND);}
  else{if(s->phase!=AI_IDLE)cancel(s,e);start(s,e);}
 }else if(type==3||type==4){
  unsigned was_down=s->down;s->down=0;s->owned=0;
  if(!was_down||!s->active||!s->online)return 1;
  if(s->mode==AI_HOLD){if(s->phase==AI_LISTENING){s->phase=AI_WAITING;emit(s,e,AI_SEND);}}
  else if(s->mode==AI_TOGGLE){s->tap=type==3&&(uint32_t)(now-s->down_at)<350u;s->last_release=now;}
 }
 return 1;
}
void ai_reply(struct ai_input *s,uint32_t generation,int speaking,struct ai_effects *e){
 reset(e);if(!s->active||!s->online||s->generation!=generation)return;
 if(speaking){if(s->phase==AI_WAITING||(s->automatic&&s->phase==AI_LISTENING))s->phase=AI_SPEAKING;}
 else if(s->phase==AI_SPEAKING){s->phase=AI_IDLE;if(s->automatic)start(s,e);}
}
