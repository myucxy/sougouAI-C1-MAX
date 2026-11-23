#ifndef C208_SCAN_H
#define C208_SCAN_H
#include <stdint.h>
#define WIFI_SCAN_MAX 16
#define WIFI_SCAN_JSON_CAP 5120
struct wifi_network { char ssid[33]; int16_t rssi,channel; uint32_t security; };
struct wifi_scan { unsigned count, status; struct wifi_network networks[WIFI_SCAN_MAX]; };
/* status: 0 unavailable, 1 scan completed (possibly empty), 2 scan failed. */
int wifi_scan_add(struct wifi_scan *scan,const uint8_t record[60]);
unsigned wifi_scan_json(const struct wifi_scan *scan,char *out,unsigned cap);
int wifi_security_supported(uint32_t security);
#endif
