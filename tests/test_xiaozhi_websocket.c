#include "../patches/xiaozhi-official/websocket.h"
#include "../patches/xiaozhi-official/official.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct xw_rx r;
static struct xo_event e;
int main(void){
 struct xw_url u;assert(xw_url_parse("wss://reply.test:8443/xiaozhi/v1/?a=1",&u));
 assert(!strcmp(u.host,"reply.test")&&u.port==8443&&!strcmp(u.authority,"reply.test:8443")&&!strcmp(u.path,"/xiaozhi/v1/?a=1"));
 assert(xw_url_parse("wss://reply.test",&u)&&!strcmp(u.path,"/")&&u.port==443);
 assert(!xw_url_parse("ws://reply.test/",&u)&&!xw_url_parse("wss://user@reply.test/",&u)&&!xw_url_parse("wss://reply.test/x\r\nX:y",&u));
 char key[25],accept[29];xw_key((const uint8_t *)"the sample nonce",key,accept);
 assert(!strcmp(key,"dGhlIHNhbXBsZSBub25jZQ==")&&!strcmp(accept,"s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
 const char *good="HTTP/1.1 101 Switching Protocols\r\nUpgrade: WebSocket\r\nConnection: keep-alive, Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";unsigned consumed;
 for(unsigned i=0;i<strlen(good);i++)assert(xw_upgrade(good,i,accept,&consumed)==0);
 assert(xw_upgrade(good,(unsigned)strlen(good),accept,&consumed)==1&&consumed==strlen(good));
 char bad[512];strcpy(bad,good);bad[0]='X';assert(xw_upgrade(bad,(unsigned)strlen(bad),accept,&consumed)==-1);
 strcpy(bad,good);bad[111]^=1;assert(xw_upgrade(bad,(unsigned)strlen(bad),accept,&consumed)==-1);
 const char *dup="HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
 assert(xw_upgrade(dup,(unsigned)strlen(dup),accept,&consumed)==-1);
 uint8_t out[512],mask[4]={1,2,3,4},payload[130];memset(payload,42,sizeof(payload));
 assert(xw_mask(1,payload,sizeof(payload),mask,out,sizeof(out))==138&&out[0]==0x81&&out[1]==0xfe);
 for(unsigned i=0;i<130;i++)assert((out[8+i]^mask[i&3])==42);
 assert(xw_mask(9,payload,126,mask,out,sizeof(out))<0);
 const uint8_t parts[]={0x01,2,'a','b',0x89,1,'p',0x80,2,'c','d'};unsigned events=0;
 for(unsigned i=0;i<sizeof(parts);i++){int op=xw_feed(&r,parts[i]);assert(op>=0);if(op){if(op==9)assert(r.control[0]=='p'&&r.total==2);else assert(op==1&&r.total==4&&!memcmp(r.payload,"abcd",4));events++;xw_next(&r);}}
 assert(events==2);
 memset(&r,0,sizeof(r));assert(xw_feed(&r,0x81)==0&&xw_feed(&r,126)==0&&xw_feed(&r,16)==0&&xw_feed(&r,0)==0);
 for(unsigned i=0;i<4096;i++)assert(xw_feed(&r,'a')==(i==4095?1:0));xw_next(&r);
 assert(xw_feed(&r,0x81)==0&&xw_feed(&r,126)==0&&xw_feed(&r,16)==0&&xw_feed(&r,1)==-1);
 const uint8_t invalid[][4]={{0x81,0x80,0,0},{0x41,0,0,0},{0x80,0,0,0},{0x09,0,0,0},{0x89,126,0,126},{0x81,126,0,1}};
 for(unsigned j=0;j<sizeof(invalid)/sizeof(*invalid);j++){memset(&r,0,sizeof(r));int result=0;for(unsigned i=0;i<4&&result>=0;i++)result=xw_feed(&r,invalid[j][i]);assert(result==-1);}
 const char *stt="{\"type\":\"stt\",\"session_id\":\"test\",\"text\":\"你好\\n小智\"}";
 assert(xo_parse_event(stt,(unsigned)strlen(stt),&e)&&e.kind==XO_EVENT_STT&&!strcmp(e.text,"你好 小智"));
 const char *tts="{\"type\":\"tts\",\"state\":\"sentence_start\",\"text\":\"这是小智的回复\"}";
 assert(xo_parse_event(tts,(unsigned)strlen(tts),&e)&&e.kind==XO_EVENT_SENTENCE&&!strcmp(e.text,"这是小智的回复"));
 const char *badjson[]={"{\"type\":\"tts\",\"state\":\"sentence_start\"}","{\"type\":\"stt\",\"text\":1}","{\"type\":\"stt\",\"text\":\"a\",\"text\":\"b\"}","{\"type\":\"hello\",\"transport\":\"mqtt\"}"};
 for(unsigned i=0;i<sizeof(badjson)/sizeof(*badjson);i++)assert(!xo_parse_event(badjson[i],(unsigned)strlen(badjson[i]),&e));
 char longtext[3000];strcpy(longtext,"{\"type\":\"tts\",\"state\":\"sentence_start\",\"text\":\"");
 for(unsigned i=0;i<600;i++)strcat(longtext,"中");strcat(longtext,"\"}");
 assert(xo_parse_event(longtext,(unsigned)strlen(longtext),&e)&&strlen(e.text)==1536);
 unsigned rng=91;for(unsigned i=0;i<100000;i++){memset(&r,0,sizeof(r));for(unsigned n=0;n<128;n++){rng=rng*1664525u+1013904223u;int op=xw_feed(&r,(uint8_t)(rng>>24));if(op<0)break;if(op)xw_next(&r);}}
 puts("PASS: RFC WebSocket key/upgrade, masking, fragments+ping, malformed frames, server subtitle JSON and 100000 streams.");
}
