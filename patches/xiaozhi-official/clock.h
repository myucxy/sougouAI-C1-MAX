#ifndef C208_NETWORK_CLOCK_H
#define C208_NETWORK_CLOCK_H
#include <stdint.h>
enum xt_state {XT_WAIT_WIFI,XT_SYNCING,XT_SYNCED,XT_FAILED};
int xt_start(void);
void xt_worker(void *argument);
void xt_poll(void);
int xt_sync(void);
int xt_wait(void);
unsigned xt_state_value(void);
/* Packet helpers do not read or change the RTC. */
void xt_request(uint8_t packet[48],const uint8_t cookie[8]);
uint64_t xt_response(const uint8_t *packet,unsigned bytes,const uint8_t cookie[8],unsigned elapsed_ms);
#endif
