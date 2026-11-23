#ifndef C208_MANAGEMENT_H
#define C208_MANAGEMENT_H
#include <stdint.h>
struct management_status { const char *status,*ssid; uint32_t ip; unsigned battery; };
int management_allowed(const char *request,unsigned size,uint32_t ip,int write);
unsigned management_json(const struct management_status *status,char *out,unsigned cap);
void management_ip(char out[16],uint32_t ip);
#endif
