#ifndef C208_XIAOZHI_OFFICIAL_H
#define C208_XIAOZHI_OFFICIAL_H
#include <stddef.h>
#include <stdint.h>
#define XO_BOOTSTRAP_URL "https://api.tenclass.net/xiaozhi/ota/"
#define XO_BODY_LIMIT 16384u
#define XO_TEXT_LIMIT 1536u
enum xo_event_kind {XO_EVENT_IGNORE,XO_EVENT_HELLO,XO_EVENT_STT,XO_EVENT_SENTENCE,XO_EVENT_TTS_START,XO_EVENT_TTS_STOP,XO_EVENT_EMOTION};
struct xo_event {unsigned kind;char session[129],text[XO_TEXT_LIMIT+1],emotion[33];};
int xo_parse_event(const char *,unsigned,struct xo_event *);
enum xo_status {XO_UNCONFIGURED,XO_ACTIVATION_REQUIRED,XO_CONFIGURED,XO_MQTT_ONLY,XO_INVALID};
struct xo_config {
 unsigned status,version,activation_timeout_ms,has_challenge,ignored_firmware;
 char activation_code[33],activation_message[257];
 char websocket_url[513],websocket_token[1025];
 uint64_t server_timestamp_ms;
};
int xo_parse_bootstrap(const char *body,unsigned bytes,struct xo_config *out);
int xo_bootstrap_body(char *out,unsigned capacity,const char *device_id,const char *client_id,const char *firmware_version);
int xo_headers(char *out,unsigned capacity,const char *device_id,const char *client_id,const char *token,unsigned version);
int xo_hello(char *out,unsigned capacity,unsigned version,unsigned frame_ms);
int xo_listen(char *out,unsigned capacity,const char *session,int start,int automatic);
int xo_abort(char *out,unsigned capacity,const char *session);
int xo_audio_pack(unsigned version,uint32_t timestamp,const uint8_t *packet,unsigned bytes,uint8_t *out,unsigned capacity);
int xo_audio_unpack(unsigned version,const uint8_t *data,unsigned bytes,const uint8_t **packet,unsigned *packet_bytes,uint32_t *timestamp);
int xo_secure_url(const char *url,int websocket);
#endif
