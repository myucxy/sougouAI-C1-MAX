#include "codec.h"
#include "platform.h"
#include "opus.h"
#include <string.h>
static void release(void *p,unsigned n){if(p){volatile uint8_t *q=p;while(n--)*q++=0;xp_free(p);}}
void xc_close(struct xc_codec *c){if(!c)return;release(c->encoder,c->encoder_bytes);release(c->decoder,c->decoder_bytes);memset(c,0,sizeof(*c));}
int xc_open(struct xc_codec *c){
 if(!c)return 0;memset(c,0,sizeof(*c));c->encoder_bytes=(unsigned)opus_encoder_get_size(1);c->decoder_bytes=(unsigned)opus_decoder_get_size(1);
 if(!c->encoder_bytes||c->encoder_bytes>65536||!c->decoder_bytes||c->decoder_bytes>65536)return 0;
 c->encoder=xp_alloc(c->encoder_bytes);c->decoder=xp_alloc(c->decoder_bytes);
 if(!c->encoder||!c->decoder||opus_encoder_init(c->encoder,16000,1,OPUS_APPLICATION_VOIP)||opus_decoder_init(c->decoder,16000,1)){xc_close(c);return 0;}
 if(opus_encoder_ctl(c->encoder,OPUS_SET_COMPLEXITY(0))||opus_encoder_ctl(c->encoder,OPUS_SET_BITRATE(24000))||opus_encoder_ctl(c->encoder,OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE))||opus_encoder_ctl(c->encoder,OPUS_SET_DTX(0))){xc_close(c);return 0;}return 1;
}
int xc_reset_encoder(struct xc_codec *c){return c&&c->encoder&&opus_encoder_ctl(c->encoder,OPUS_RESET_STATE)==OPUS_OK;}
int xc_reset_decoder(struct xc_codec *c){return c&&c->decoder&&opus_decoder_ctl(c->decoder,OPUS_RESET_STATE)==OPUS_OK;}
int xc_encode(struct xc_codec *c,const int16_t pcm[320],uint8_t *out,unsigned cap){if(!c||!c->encoder||!pcm||!out||cap<64||cap>4096)return -1;return opus_encode(c->encoder,pcm,320,out,(opus_int32)cap);}
int xc_decode(struct xc_codec *c,const uint8_t *p,unsigned n,int16_t *out,unsigned cap){
 if(!c||!c->decoder||!p||!n||n>4096||!out||cap<1920)return -1;
 int samples=opus_packet_get_nb_samples(p,(opus_int32)n,16000);if(samples<=0||samples>1920||opus_packet_get_nb_channels(p)>2)return -1;
 return opus_decode(c->decoder,p,(opus_int32)n,out,1920,0);
}
