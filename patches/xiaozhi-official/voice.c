#include "voice.h"
#include "audio.h"
#include "platform.h"
#include "official.h"
#include <string.h>
static int fail(struct xv_voice *v,unsigned error){
 if(error==2||error==8)xa_capture_report(error);
 v->status.error=error;v->status.phase=XV_ERROR;v->status.ready=0;xl_reset(&v->status.meter);
 if(v->capture){xa_capture_close();v->capture=0;}if(v->playback){xa_playback_close();v->playback=0;}v->count=v->decoded_samples=0;return 0;
}
int xv_open(struct xv_voice *v){
 if(!v)return 0;memset(v,0,sizeof(*v));if(!xc_open(&v->codec))return fail(v,1);v->status.ready=1;return 1;
}
void xv_close(struct xv_voice *v){if(!v)return;if(v->capture)xa_capture_close();if(v->playback)xa_playback_close();xc_close(&v->codec);memset(v,0,sizeof(*v));}
static int control(struct xv_voice *v,const char *session,unsigned action,xv_send send,void *ctx){
 char out[256];int n=action==AI_CANCEL?xo_abort(out,sizeof(out),session):xo_listen(out,sizeof(out),session,action==AI_START,v->status.automatic);
 return n>0&&send(ctx,1,(const uint8_t *)out,(unsigned)n);
}
int xv_action(struct xv_voice *v,unsigned action,unsigned generation,unsigned automatic,const char *session,xv_send send,void *ctx){
 if(!v||!v->status.ready||!session||!*session)return 0;
 if(action==AI_CANCEL){
  if(v->capture&&!xa_capture_close())return fail(v,2);v->capture=0;
  if(v->playback&&!xa_playback_close())return fail(v,3);v->playback=0;
  v->count=v->decoded_samples=v->decoded_at=0;v->stop_pending=v->tts_started=v->tts_stopped=0;
  v->status.phase=XV_IDLE;v->status.automatic=0;v->status.generation=generation;xl_reset(&v->status.meter);
  return control(v,session,AI_CANCEL,send,ctx)||fail(v,4);
 }
 if(action==AI_AUTO)return 1;
 if(action==AI_START){
  if(v->capture&&!xa_capture_close())return fail(v,2);v->capture=0;
  if(v->playback&&!xa_playback_parked()){if(!xa_playback_close())return fail(v,3);v->playback=0;}
  v->count=v->decoded_samples=v->decoded_at=0;v->stop_pending=v->tts_started=v->tts_stopped=0;v->speech=v->silence=0;
  v->status.generation=generation;v->status.automatic=!!automatic;xl_reset(&v->status.meter);
  if(!xc_reset_encoder(&v->codec)||!xa_capture_open())return fail(v,2);v->capture=1;
  v->started=v->last_sample=xp_ticks();v->status.phase=XV_CAPTURE;
  return control(v,session,AI_START,send,ctx)||fail(v,4);
 }
 if(action==AI_SEND&&v->capture){if(!xa_capture_close())return fail(v,2);v->capture=0;v->stop_pending=1;v->status.phase=XV_WAITING;v->waiting_since=xp_ticks();xl_reset(&v->status.meter);}
 return 1;
}
int xv_receive(struct xv_voice *v,const uint8_t *p,unsigned n){
 if(!v||!v->status.ready||v->capture||v->status.phase==XV_IDLE)return 1;
 if(!p||!n||n>XV_PACKET_BYTES)return fail(v,5);if(v->count==XV_QUEUE)return fail(v,6);
 unsigned slot=(v->head+v->count)%XV_QUEUE;memcpy(v->packets[slot].data,p,n);v->packets[slot].bytes=n;v->count++;return 1;
}
void xv_tts(struct xv_voice *v,int start){
 if(!v||!v->status.ready||v->status.phase==XV_IDLE)return;
 if(v->capture&&!v->status.automatic)return;
 if(start){v->tts_started=1;v->tts_stopped=0;if(v->status.automatic&&v->capture){if(!xa_capture_close()){fail(v,2);return;}v->capture=0;v->status.phase=XV_WAITING;v->waiting_since=xp_ticks();xl_reset(&v->status.meter);}xc_reset_decoder(&v->codec);}
 else if(v->tts_started)v->tts_stopped=1;
}
int xv_tick(struct xv_voice *v,unsigned now,const char *session,unsigned version,xv_send send,void *ctx){
 if(!v||!v->status.ready)return 0;
 /* Refill retained output before microphone encoding or network control work. */
 if(v->playback&&!v->count&&v->decoded_at==v->decoded_samples&&xa_playback_parked()&&xa_playback_keepalive()<0)return fail(v,3);
 if(v->stop_pending){if(!control(v,session,AI_SEND,send,ctx))return 1;v->stop_pending=0;}
 if(v->status.phase==XV_WAITING&&!v->tts_started&&(unsigned)(now-v->waiting_since)>30000u)return fail(v,9);
 if(v->capture){
  int n=xa_capture_read(v->pcm,320);if(n<0||n>320)return fail(v,2);
  if(n&&n!=320)return fail(v,2);
  if(n){
   v->last_sample=now;
   /* Manual modes need no amplitude/envelope work; automatic turns retain VAD. */
   if(v->status.automatic){
    xl_update(&v->status.meter,v->pcm,(unsigned)n);
    if(v->status.meter.active){v->speech=1;v->silence=now;}else if(!v->speech)v->silence=now;
   }else v->status.meter.received=1;
   uint8_t packet[512],wire[528];int bytes=xc_encode(&v->codec,v->pcm,packet,sizeof(packet));if(bytes<=0)return fail(v,7);
   int framed=xo_audio_pack(version,now,packet,(unsigned)bytes,wire,sizeof(wire));if(framed<=0||!send(ctx,2,wire,(unsigned)framed))return fail(v,4);
   if(v->status.automatic&&v->speech&&(unsigned)(now-v->silence)>=900u){if(!xv_action(v,AI_SEND,v->status.generation,1,session,send,ctx))return 0;}
  }else if((unsigned)(now-v->last_sample)>1000u)return fail(v,8);
  else if((unsigned)(now-v->last_sample)>120u)xl_reset(&v->status.meter);
  if((unsigned)(now-v->started)>60000u)return xv_action(v,AI_SEND,v->status.generation,v->status.automatic,session,send,ctx);
 }
 if(v->decoded_at<v->decoded_samples){
  int n=xa_playback_write(v->decoded+v->decoded_at,v->decoded_samples-v->decoded_at);if(n<0)return fail(v,3);
  if(n){v->decoded_at+=(unsigned)n;v->last_output=now;}
  else if((unsigned)(now-v->last_output)>1500u)return fail(v,3);
 }
 if(v->decoded_at==v->decoded_samples&&v->count){
  struct xv_packet *packet=&v->packets[v->head];int n=xc_decode(&v->codec,packet->data,packet->bytes,v->decoded,1920);
  if(n<=0)return fail(v,5);v->head=(v->head+1)%XV_QUEUE;v->count--;v->decoded_samples=(unsigned)n;v->decoded_at=0;
  if(!v->playback){if(!xa_playback_open())return fail(v,3);v->playback=1;}v->last_output=now;v->status.phase=XV_PLAYING;
 }
 if(v->playback&&!v->tts_stopped&&!v->count&&v->decoded_at==v->decoded_samples){
  int quiet=xa_playback_keepalive();if(quiet<0)return fail(v,3);
 }
 if(v->tts_stopped&&!v->count&&v->decoded_at==v->decoded_samples){
  if(v->playback){
   int finished=xa_playback_finish();if(finished<0)return fail(v,3);if(!finished)return 1;
  }
  v->tts_stopped=v->tts_started=0;v->status.phase=XV_IDLE;
  if(v->status.automatic)return xv_action(v,AI_START,v->status.generation+1,1,session,send,ctx);
 }
 return 1;
}
void xv_snapshot(struct xv_voice *v,struct xv_status *out){if(v&&out)*out=v->status;}

const unsigned xv_working_bytes=sizeof(struct xv_voice);
