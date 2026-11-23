#ifndef C208_VOICE_CODEC_H
#define C208_VOICE_CODEC_H
#include <stdint.h>
struct xc_codec {void *encoder,*decoder;unsigned encoder_bytes,decoder_bytes;};
int xc_open(struct xc_codec *);
void xc_close(struct xc_codec *);
int xc_reset_encoder(struct xc_codec *);
int xc_reset_decoder(struct xc_codec *);
int xc_encode(struct xc_codec *,const int16_t pcm[320],uint8_t *packet,unsigned cap);
int xc_decode(struct xc_codec *,const uint8_t *,unsigned,int16_t *,unsigned cap);
#endif
