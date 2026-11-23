#include "../patches/xiaozhi-official/official.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct xo_config c;
static int parse(const char *p){return xo_parse_bootstrap(p,(unsigned)strlen(p),&c);}
int main(void){
 assert(parse("{\"activation\":{\"code\":\"123456\",\"message\":\"请在控制台添加设备\",\"challenge\":\"abc\",\"timeout_ms\":30000}}"));
 assert(parse("{\"activation\":{\"code\":\"123456\",\"message\":\"xiaozhi.me\\n123456\",\"challenge\":\"abc\"}}"));
 assert(!strcmp(c.activation_message,"xiaozhi.me 123456"));
 assert(c.status==XO_ACTIVATION_REQUIRED&&!strcmp(c.activation_code,"123456")&&c.has_challenge);
 assert(parse("{\"websocket\":{\"url\":\"wss://example.test/xiaozhi/v1/\",\"token\":\"abc\",\"version\":3},\"firmware\":{\"url\":\"https://example.test/esp32.bin\",\"force\":1},\"server_time\":{\"timestamp\":1794900000000}}"));
 assert(c.status==XO_CONFIGURED&&c.version==3&&c.ignored_firmware&&c.server_timestamp_ms==1794900000000ULL);
 assert(parse("{\"mqtt\":{\"endpoint\":\"broker.test\"}}")&&c.status==XO_MQTT_ONLY);
 assert(parse("{}")&&c.status==XO_UNCONFIGURED);
 const char *bad[]={"", "[]", "{", "{}x", "{\"websocket\":{\"url\":\"ws://example.test/\"}}", "{\"websocket\":{\"url\":\"wss://user:pass@example.test/\"}}",
 "{\"activation\":{\"code\":\"x\\u0000y\"}}", "{\"activation\":{\"code\":\"a\\rb\"}}", "{\"a\":1,\"a\":2}", "{\"a\":1,\"\\u0061\":2}",
 "{\"x\":truee}", "{\"x\":01}", "{\"x\":NaN}", "{\"x\":\"\\ud800\"}", "{\"x\":\"\\udc00\"}", "{\"websocket\":{\"url\":\"wss://example.test/\",\"version\":4}}",
 "{\"activation\":{\"timeout_ms\":-1}}", "{\"activation\":{\"timeout_ms\":30000.0}}", "{\"server_time\":{\"timestamp\":18446744073709551616}}", "{\"x\":[1,]}"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){if(parse(bad[i])){fprintf(stderr,"accepted invalid fixture %u\n",i);return 1;}assert(c.status==XO_INVALID&&!c.websocket_token[0]);}
 assert(parse("{\"activation\":{\"message\":\"hello \\ud83d\\ude00\"}}")&&!strcmp(c.activation_message,"hello \xf0\x9f\x98\x80"));
 char out[2048];const char *id="02:01:02:03:04:05",*uuid="00112233-4455-4677-8899-aabbccddeeff";
 assert(xo_bootstrap_body(out,sizeof(out),id,uuid,"V0105")>0&&strstr(out,"rk2108")&&!strstr(out,"esp32"));
 assert(xo_headers(out,sizeof(out),id,uuid,"",0)>0&&strstr(out,"Activation-Version: 1"));
 assert(xo_headers(out,sizeof(out),id,uuid,"token",1)>0&&strstr(out,"Authorization: Bearer token\r\n"));
 assert(xo_headers(out,sizeof(out),id,uuid,"Bearer token",2)>0&&!strstr(out,"Bearer Bearer"));
 assert(xo_headers(out,sizeof(out),id,uuid,"x\r\nX:y",1)<0&&!out[0]);
 assert(xo_hello(out,sizeof(out),1,20)>0&&strstr(out,"\"mcp\":false"));
 assert(xo_listen(out,sizeof(out),"session\"\\",1,0)>0&&strstr(out,"manual"));
 assert(xo_listen(out,sizeof(out),"session",1,1)>0&&strstr(out,"auto"));
 assert(xo_listen(out,sizeof(out),"session",0,0)>0&&strstr(out,"stop")&&!strstr(out,"mode"));
 assert(xo_abort(out,sizeof(out),"session")>0&&strstr(out,"abort"));
 assert(xo_hello(out,5,1,20)<0&&!out[0]);assert(!xo_secure_url("wss://host.test:0/",1));assert(!xo_secure_url("wss://host.test/x#frag",1));
 uint8_t packet[]={0xf8,0xff,0xfe},binary[64];const uint8_t *decoded;unsigned decoded_bytes;uint32_t timestamp;
 for(unsigned version=1;version<=3;version++){
  int n=xo_audio_pack(version,1234,packet,sizeof(packet),binary,sizeof(binary));assert(n>0);
  assert(xo_audio_unpack(version,binary,(unsigned)n,&decoded,&decoded_bytes,&timestamp));
  assert(decoded_bytes==sizeof(packet)&&!memcmp(decoded,packet,sizeof(packet))&&timestamp==(version==2?1234u:0u));
  if(version>1){binary[version==2?15:3]++;assert(!xo_audio_unpack(version,binary,(unsigned)n,&decoded,&decoded_bytes,&timestamp));}
 }
 unsigned rng=17;char fuzz[1024];
 for(unsigned i=0;i<100000;i++){rng=rng*1664525u+1013904223u;unsigned n=(rng>>16)%sizeof(fuzz);for(unsigned k=0;k<n;k++){rng=rng*1664525u+1013904223u;fuzz[k]=(char)(rng>>24);}xo_parse_bootstrap(fuzz,n,&c);assert(c.status<=XO_INVALID);}
 puts("PASS: official bootstrap, activation, WSS/token/version, ESP32 OTA ignored, bounded JSON/Unicode, header injection, commands, 100000 malformed inputs.");
}
