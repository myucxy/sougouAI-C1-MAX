#ifndef C208_VOICE_H
#define C208_VOICE_H
#include "codec.h"
#include "level.h"
#include "../xiaozhi-main/input.h"
#define XV_PACKET_BYTES 1276u
#define XV_QUEUE 12u
enum xv_phase {XV_IDLE,XV_CAPTURE,XV_WAITING,XV_PLAYING,XV_ERROR};
struct xv_status {unsigned ready,phase,error,generation,automatic;struct xl_level meter;};
struct xv_packet {unsigned bytes;uint8_t data[XV_PACKET_BYTES];};
struct xv_voice {struct xc_codec codec;struct xv_status status;unsigned capture,playback,last_sample,last_output,waiting_since,started,silence,speech,stop_pending,tts_started,tts_stopped,head,count,decoded_samples,decoded_at;int16_t pcm[320],decoded[1920];struct xv_packet packets[XV_QUEUE];};
typedef int (*xv_send)(void *,unsigned,const uint8_t *,unsigned);
int xv_open(struct xv_voice *);
void xv_close(struct xv_voice *);
int xv_action(struct xv_voice *,unsigned action,unsigned generation,unsigned automatic,const char *session,xv_send,void *);
int xv_tick(struct xv_voice *,unsigned now,const char *session,unsigned version,xv_send,void *);
int xv_receive(struct xv_voice *,const uint8_t *,unsigned);
void xv_tts(struct xv_voice *,int start);
void xv_snapshot(struct xv_voice *,struct xv_status *);
#endif
