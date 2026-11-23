#ifndef C208_PORTAL_H
#define C208_PORTAL_H
#include <stdint.h>
/* One uncompressed IN question, bounded to ordinary 512-byte UDP DNS. */
unsigned portal_dns_reply(const uint8_t *query,unsigned size,uint8_t *reply,unsigned capacity);
int portal_get_line(const char *line,unsigned size);
int portal_client_ip(uint32_t network_order_ip);
#endif
