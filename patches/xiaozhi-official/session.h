#ifndef C208_OFFICIAL_SESSION_H
#define C208_OFFICIAL_SESSION_H
#include <stdint.h>
#include "voice.h"
enum xs_state {XS_IDLE,XS_WAIT_WIFI,XS_PREPARING,XS_TLS,XS_ACTIVATION,XS_CONFIGURED,XS_FAILED,XS_MQTT_CONFIGURED,XS_TIME_SYNC,XS_WS_CONNECTING,XS_WS_READY,XS_CLOSED};
enum xs_error {XS_OK,XS_USB,XS_MEMORY,XS_CLOCK,XS_IDENTITY,XS_SOCKET,XS_TIMEOUT,XS_TLS_ERROR,XS_HTTP,XS_CONFIGURATION,XS_MQTT,XS_THREAD,XS_WEBSOCKET,XS_HELLO,XS_AUDIO};
int xs_start(void);
int xs_quiescent(void);
void xs_cancel(void);
void xs_status(char *out,unsigned capacity);
unsigned xs_state_value(void);
unsigned xs_error_value(void);
void xs_activation_code(char *out,unsigned capacity);
unsigned xs_caption_page(char *out,unsigned capacity,unsigned page,unsigned *pages,unsigned *kind);
int xs_voice_request(unsigned action,unsigned generation,unsigned automatic);
void xs_voice_status(struct xv_status *out);
void xs_worker(void *);
void ai_official_notify(void);
#endif
