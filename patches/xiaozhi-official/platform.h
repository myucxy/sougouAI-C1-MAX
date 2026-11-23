#ifndef C208_OFFICIAL_PLATFORM_H
#define C208_OFFICIAL_PLATFORM_H
#include <stdint.h>
unsigned xp_ticks(void);
unsigned xp_lock(void);
void xp_unlock(unsigned key);
void xp_delay(unsigned ms);
int xp_usb(void);
int xp_network_ready(void);
int xp_mac(uint8_t mac[6]);
uint64_t xp_time(void);
int xp_set_time(uint64_t utc);
int xp_ntp_open(const char *host);
void xp_time_log(unsigned state,unsigned error,uint64_t utc);
void *xp_alloc(unsigned bytes);
void xp_memory_report(unsigned stage,unsigned requested);
void xp_free(void *p);
int xp_identity(uint8_t mac[6],char uuid[37],uint8_t entropy[32],uint64_t now);
int xp_connect(const char *host,unsigned port);
int xp_read(int socket,void *buffer,unsigned bytes);
int xp_write(int socket,const void *buffer,unsigned bytes);
void xp_close(int socket);
void xp_log(unsigned state,unsigned error,unsigned tls_error,unsigned http_status);
#endif
