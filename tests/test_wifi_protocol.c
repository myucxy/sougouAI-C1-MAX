/* Compile with AddressSanitizer + UBSan; tests run only the portable protocol. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../patches/wifi/protocol.h"
static int post(const char *body,struct credentials *out) {
    char buf[HTTP_CAP+1];int n=snprintf(buf,sizeof(buf),"POST /configure HTTP/1.1\r\nHost: 192.168.84.1\r\nOrigin: http://192.168.84.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body);
    assert(n>0&&n<HTTP_CAP);return http_request(buf,(unsigned)n,out);
}
int main(void) {
    struct credentials c={0};
    assert(post("ssid=Home+WiFi&password=12345678",&c)==2);assert(!strcmp(c.ssid,"Home WiFi")&&!strcmp(c.password,"12345678"));
    assert(post("ssid=%E6%B8%85%E9%A3%8E&password=",&c)==2);assert(!strcmp(c.ssid,"清风"));
    assert(post("password=A%26B%3DC%2B12&ssid=x",&c)==2);assert(!strcmp(c.password,"A&B=C+12"));
    assert(post("ssid=x&password=1234567",&c)==-1);
    assert(post("ssid=x&password=12345678901234567890123456789012",&c)==2);
    assert(post("ssid=x&password=123456789012345678901234567890123",&c)==-1);
    assert(post("ssid=&password=",&c)==-1);
    assert(post("ssid=x%00y&password=12345678",&c)==-1);
    assert(post("ssid=x%0Dy&password=12345678",&c)==-1);
    assert(post("ssid=x%GG&password=12345678",&c)==-1);
    assert(post("ssid=x%&password=12345678",&c)==-1);
    assert(post("ssid=x&password=12345678&ssid=y",&c)==-1);
    assert(post("ssid=x&password=12345678&unknown=x",&c)==-1);
    assert(post("ssid=x&password=12345678&",&c)==-1);
    char ssid32[100];snprintf(ssid32,sizeof(ssid32),"ssid=12345678901234567890123456789012&password=");assert(post(ssid32,&c)==2);
    assert(post("ssid=123456789012345678901234567890123&password=",&c)==-1);
    const char *get="GET / HTTP/1.1\r\nHost: 192.168.84.1\r\n\r\n";
    for(unsigned i=0;i<strlen(get);i++)assert(http_request(get,i,&c)==0);
    assert(http_request(get,(unsigned)strlen(get),&c)==1);
    const char *sample="POST /configure HTTP/1.1\r\ncontent-length: 16\r\ncontent-type: application/x-www-form-urlencoded\r\n\r\nssid=x&password=";
    assert(strlen("ssid=x&password=")==16);
    for(unsigned i=0;i<strlen(sample);i++)assert(http_request(sample,i,&c)==0);
    assert(http_request(sample,(unsigned)strlen(sample),&c)==2);
    const char *bad[]={
      "POST /configure HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx",
      "POST /configure HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
      "POST /configure HTTP/1.1\r\nContent-Length: 4294967296\r\n\r\n",
      "POST /configure HTTP/1.1\r\nContent-Length: -1\r\n\r\n",
      "POST /configure HTTP/1.1\r\nOrigin: http://evil.invalid\r\n\r\n",
      "GET / HTTP/1.1\r\n\r\nGET / HTTP/1.1\r\n\r\n",
      "GET / HTTP/1.1\r\n folded: x\r\n\r\n",
      #ifndef C208_WIFI_V93
      "GET /favicon.ico HTTP/1.1\r\n\r\n"
#endif
      };
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(http_request(bad[i],(unsigned)strlen(bad[i]),&c)==-1);
    struct config_record r;assert(post("ssid=Home&password=12345678",&c)==2);record_make(&r,42,&c);assert(record_valid(&r));
    for(unsigned i=0;i<sizeof(r);i++){((uint8_t *)&r)[i]^=1;assert(!record_valid(&r));((uint8_t *)&r)[i]^=1;}
    memset(&c,0,sizeof(c));record_make(&r,43,&c);assert(record_valid(&r));
    uint8_t req[576]={0},out[320];req[0]=1;req[1]=1;req[2]=6;req[4]=0x12;req[7]=0x34;req[28]=0xaa;
    req[236]=99;req[237]=130;req[238]=83;req[239]=99;req[240]=53;req[241]=1;req[242]=1;req[243]=255;
    memset(out,0x5a,sizeof(out));assert(dhcp_reply(req,244,out,100)==300);assert(out[0]==2&&out[16]==192&&out[19]==100&&out[242]==2&&out[4]==0x12&&out[28]==0xaa);
    for(unsigned i=300;i<sizeof(out);i++)assert(out[i]==0x5a);
    req[242]=3;assert(dhcp_reply(req,244,out,100)==300&&out[242]==5);
    req[243]=50;req[244]=4;req[245]=192;req[246]=168;req[247]=84;req[248]=101;req[249]=255;
    assert(dhcp_reply(req,250,out,100)==300&&out[242]==6&&out[16]==0);
    req[248]=100;assert(dhcp_reply(req,250,out,100)==300&&out[242]==5);
    req[243]=54;req[248]=2;assert(dhcp_reply(req,250,out,100)==0);
    req[243]=50;req[244]=200;assert(dhcp_reply(req,250,out,100)==0);
    for(unsigned i=0;i<240;i++)assert(dhcp_reply(req,i,out,100)==0);
    /* Deterministic malformed input sweep exercises bounds under sanitizers. */
    uint32_t rng=0xC208;char fuzz[HTTP_CAP+1];
    for(unsigned iter=0;iter<100000;iter++) {
        rng=rng*1664525+1013904223;unsigned n=rng%(HTTP_CAP+1);
        for(unsigned i=0;i<n;i++){rng=rng*1664525+1013904223;fuzz[i]=(char)(rng>>24);}
        http_request(fuzz,n,&c);
        if(n>=240&&n<=576){fuzz[0]=1;fuzz[1]=1;fuzz[2]=6;fuzz[3]=0;memset(fuzz+24,0,4);fuzz[236]=99;fuzz[237]=(char)130;fuzz[238]=83;fuzz[239]=99;dhcp_reply((uint8_t *)fuzz,n,out,100);}
    }
    puts("PASS: HTTP framing/forms, byte limits, record corruption, DHCP offer/ACK/NAK, 100000 malformed inputs (ASan/UBSan)");
}
