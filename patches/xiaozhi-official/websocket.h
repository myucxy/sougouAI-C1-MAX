#ifndef C208_WEBSOCKET_H
#define C208_WEBSOCKET_H
#include <stdint.h>
#define XW_LIMIT 4096u
struct xw_url {char host[254],authority[261],path[513];unsigned port;};
struct xw_rx {unsigned header_bytes,needed,length,received,opcode,final,fragment,total;uint8_t header[10],control[125],payload[XW_LIMIT+1];};
int xw_url_parse(const char *,struct xw_url *);
void xw_key(const uint8_t random[16],char key[25],char expected_accept[29]);
/* Return 1 at header completion, 0 pending, -1 rejected; consumed retains following frame bytes. */
int xw_upgrade(const char *bytes,unsigned n,const char *expected,unsigned *consumed);
int xw_mask(unsigned opcode,const uint8_t *,unsigned,const uint8_t mask[4],uint8_t *,unsigned);
/* Return opcode 1/2/8/9/10 when complete, 0 pending, -1 rejected. */
int xw_feed(struct xw_rx *,uint8_t byte);
void xw_next(struct xw_rx *);
#endif
