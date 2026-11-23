#ifndef C208_PROTOCOL_H
#define C208_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#define HTTP_CAP 1536
#define WIFI_PASS_MAX 32 /* This image's WLAN management key buffer, not the chip limit. */
struct credentials { char ssid[33], password[33]; };
struct config_record { uint32_t magic, sequence; struct credentials net; uint8_t pad[2]; uint32_t hash; };
int http_request(const char *buf, unsigned size, struct credentials *out);
/* 0 incomplete; 1 page; 2 configure; 4 networks; 5 scan; 6 portal redirect; 7 status; 8 provision; -1 invalid. */
int credentials_valid(const struct credentials *net);
uint32_t record_hash(const void *buf, unsigned size);
int record_valid(const struct config_record *record);
void record_make(struct config_record *record, uint32_t sequence, const struct credentials *net);
unsigned dhcp_reply(const uint8_t *request, unsigned size, uint8_t *reply, uint8_t host);
#endif
