#include "session.h"
#include "../xiaozhi-main/record_mode.h"
#include "platform.h"
#include "clock.h"
#include "tls.h"
#include "official.h"
#include "http.h"
#include "websocket.h"
#include "voice.h"
#include "official_root.h"
#include "addresses.h"
#include <string.h>
#define FN(ret,a,...) ((ret(*)(__VA_ARGS__))((uintptr_t)(a)|1u))
/* Bootstrap response storage and voice queues are used in separate phases. */
union workspace_phase {
 struct {struct xo_http_response http;char body[XO_BODY_LIMIT+1];} bootstrap;
 struct xv_voice voice;
};
struct workspace {
 struct xo_tls *tls;union workspace_phase *phase;unsigned voice_live;struct xo_config config;
 br_x509_decoder_context decoder;br_x509_trust_anchor anchor;
 char request[4096],headers[1536],payload[1024];
 char mac[18],uuid[37];uint8_t entropy[32];int socket;
 struct xw_url url;struct xw_rx websocket;struct xo_event event;
 uint8_t tx[XW_LIMIT+16];unsigned tx_bytes,tx_sent;
 char key[25],accept[29],session[129];
};
static struct {volatile unsigned busy,cancel,state,error,tls_error,http_status;char activation[33];volatile unsigned caption_revision,caption_kind;char caption[XO_TEXT_LIMIT+1];struct xv_status voice;struct ai_event commands[8];unsigned command_head,command_count,command_overflow,retry_after,retry_delayed; } S;
static void state(unsigned next,unsigned error){
 /* Keep the issued code visible while polling activation over TLS. */
 if(next==XS_TLS&&S.state==XS_ACTIVATION&&S.activation[0])next=XS_ACTIVATION;
 S.state=next;S.error=error;xp_log(next,error,S.tls_error,S.http_status);ai_official_notify();
}
unsigned xs_state_value(void){return S.state;}
unsigned xs_error_value(void){return S.error;}
void xs_activation_code(char *out,unsigned cap){
 if(!cap)return;unsigned n=0;if(S.state==XS_ACTIVATION)while(S.activation[n]&&n+1<cap){out[n]=S.activation[n];n++;}out[n]=0;
}
void xs_status(char *out,unsigned cap){
 if(!cap)return;const char *text="小智官方 / 等待连接";
 if(S.state==XS_WAIT_WIFI)text="请先连接 Wi-Fi";
 else if(S.state==XS_TIME_SYNC)text="正在联网自动校时";
 else if(S.state==XS_PREPARING)text="正在准备官方连接";
 else if(S.state==XS_TLS)text="正在连接小智官方";
 else if(S.state==XS_WS_CONNECTING)text="正在连接对话通道";
 else if(S.state==XS_WS_READY){
  if(S.voice.error)text="音频设备初始化失败\n请重连后重试";
  else if(S.voice.phase==XV_CAPTURE)text="正在收音";
  else if(S.voice.phase==XV_WAITING)text="已发送\n等待小智回复";
  else if(S.voice.phase==XV_PLAYING)text="小智正在回复";
  else text=S.voice.ready?"小智已就绪\n按录音键说话":"正在准备音频设备";
 }
 else if(S.state==XS_CLOSED)text="对话通道已断开\n请点击重试";
 else if(S.state==XS_ACTIVATION)text="官方激活码：";
 else if(S.state==XS_CONFIGURED)text="官方配置已获取\n正在建立对话通道";
 else if(S.state==XS_MQTT_CONFIGURED)text="官方配置已获取\nMQTT待适配";
 else if(S.state==XS_FAILED){
  switch(S.error){
  case XS_USB:text="请断开 USB 后重试";break;
  case XS_MEMORY:text="小智连接内存不足";break;
  case XS_CLOCK:text="自动校时失败\n请检查网络或时间";break;
  case XS_IDENTITY:text="缺少小智身份文件\n请用电脑初始化";break;
  case XS_SOCKET:text="官方网络连接失败";break;
  case XS_TIMEOUT:text="官方连接超时\n请点击重试";break;
  case XS_TLS_ERROR:text="官方TLS连接失败\n请检查时间或网络";break;
  case XS_HTTP:text="官方接口响应异常";break;
  case XS_MQTT:text="官方仅提供MQTT\n语音通道待适配";break;
  case XS_WEBSOCKET:text="对话协议连接失败\n请点击重试";break;
  case XS_HELLO:text="对话握手超时\n请点击重试";break;
  case XS_AUDIO:
   text=S.voice.error==2?"麦克风启动失败":S.voice.error==3?"扬声器播放失败":S.voice.error==4?"音频发送拥堵":S.voice.error==5?"回复音频解码失败":S.voice.error==7?"语音编码失败":S.voice.error==8?"未收到采音数据":S.voice.error==9?"回复等待超时":"音频链路异常\n请重连后重试";break;
  case XS_THREAD:text="小智线程启动失败";break;
  default:text="官方配置格式异常";break;
  }
 }
 unsigned n=0;while(*text&&n+1<cap)out[n++]=*text++;
 if(S.state==XS_ACTIVATION)for(unsigned i=0;S.activation[i]&&n+1<cap;i++)out[n++]=S.activation[i];out[n]=0;
}
void xs_cancel(void){S.cancel=1;}
static void caption(const struct xo_event *event){
 unsigned key=xp_lock();memcpy(S.caption,event->text,sizeof(S.caption));S.caption_kind=event->kind;
 S.caption_revision++;if(!S.caption_revision)S.caption_revision=1;xp_unlock(key);ai_official_notify();
}
unsigned xs_caption_page(char *out,unsigned cap,unsigned page,unsigned *pages,unsigned *kind){
 if(!out||!cap||!pages||!kind)return 0;
 unsigned key=xp_lock(),count=0;
 for(unsigned i=0;S.caption[i];i++)if(((unsigned char)S.caption[i]&0xc0)!=0x80)count++;
 *pages=(count+17)/18;*kind=S.caption_kind;unsigned revision=S.caption_revision,n=0,character=0;
 if(*pages)page%=*pages;
 for(unsigned i=0;S.caption[i];){unsigned c=(unsigned char)S.caption[i],bytes=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
  if(character>=page*18&&character<page*18+18){
   if(character==page*18+9&&n+1<cap)out[n++]='\n';
   if(n+bytes>=cap)break;for(unsigned j=0;j<bytes;j++)out[n++]=S.caption[i+j];
  }i+=bytes;character++;
 }out[n]=0;xp_unlock(key);return count?revision:0;
}
int xs_voice_request(unsigned action,unsigned generation,unsigned automatic){
 unsigned key=xp_lock();if(!S.voice.ready||S.command_count==8){if(S.voice.ready)S.command_overflow=1;xp_unlock(key);return 0;}
 S.commands[(S.command_head+S.command_count)%8]=(struct ai_event){generation,action,automatic};S.command_count++;xp_unlock(key);return 1;
}
void xs_voice_status(struct xv_status *out){if(out){unsigned key=xp_lock();*out=S.voice;xp_unlock(key);}}
static void voice_publish(struct workspace *w){unsigned key=xp_lock();S.voice=w->phase->voice.status;xp_unlock(key);}
static int voice_send(void *ctx,unsigned opcode,const uint8_t *p,unsigned bytes);
static __attribute__((noinline)) int voice_tick(struct workspace *w){
 unsigned key=xp_lock();struct ai_event event={0};if(S.command_count){event=S.commands[S.command_head];S.command_head=(S.command_head+1)%8;S.command_count--;}xp_unlock(key);
 if(S.command_overflow){w->phase->voice.status.error=4;w->phase->voice.status.ready=0;voice_publish(w);return 0;}
 if(event.action==AI_START){key=xp_lock();S.caption[0]=0;S.caption_revision++;xp_unlock(key);}
 int ok=1;if(event.action)ok=xv_action(&w->phase->voice,event.action,event.generation,event.automatic,w->session,voice_send,w);
 /* Native capture setup may advance ticks; never compare its new timestamp to the pre-command tick. */
 if(ok)ok=xv_tick(&w->phase->voice,xp_ticks(),w->session,w->config.version,voice_send,w);voice_publish(w);return ok;
}
static int cancelled(void){return S.cancel||xp_usb()||!xp_network_ready();}
static void wipe(void *ptr,unsigned n){volatile uint8_t *p=ptr;while(n--)*p++=0;}
static void mac_text(char out[18],const uint8_t mac[6]){
 unsigned n=0;for(unsigned i=0;i<6;i++){if(i)out[n++]=':';out[n++]="0123456789abcdef"[mac[i]>>4];out[n++]="0123456789abcdef"[mac[i]&15];}out[n]=0;
}
static unsigned append(char *out,unsigned at,unsigned cap,const char *text){
 while(*text){if(at+1>=cap)return cap;out[at++]=*text++;}if(at<cap)out[at]=0;return at;
}
static unsigned decimal(char *out,unsigned at,unsigned cap,unsigned value){
 char digits[11];unsigned n=0;do{digits[n++]='0'+value%10;value/=10;}while(value);while(n){char b[2]={digits[--n],0};at=append(out,at,cap,b);}return at;
}
static __attribute__((noinline)) int root(struct workspace *w){
 br_x509_decoder_init(&w->decoder,0,0);br_x509_decoder_push(&w->decoder,C208_ROOT_DER,sizeof(C208_ROOT_DER));
 const br_x509_pkey *key=br_x509_decoder_get_pkey(&w->decoder);if(!key||!br_x509_decoder_isCA(&w->decoder))return 0;
 w->anchor.dn.data=(unsigned char *)C208_ROOT_DN;w->anchor.dn.len=sizeof(C208_ROOT_DN);w->anchor.flags=BR_X509_TA_CA;w->anchor.pkey=*key;return 1;
}
static int request(struct workspace *w,int activate){
 S.tls_error=S.http_status=0;uint64_t now=xp_time();if(!now){state(XS_FAILED,XS_CLOCK);return 0;}
 uint8_t mac[6];if(!xp_identity(mac,w->uuid,w->entropy,now)){state(XS_FAILED,XS_IDENTITY);return 0;}mac_text(w->mac,mac);
 int payload=activate?2:xo_bootstrap_body(w->payload,sizeof(w->payload),w->mac,w->uuid,C208_AI_VERSION);
 if(activate)memcpy(w->payload,"{}",3);
 int headers=xo_headers(w->headers,sizeof(w->headers),w->mac,w->uuid,"",0);
 if(payload<0||headers<0||!root(w)){state(XS_FAILED,XS_CONFIGURATION);return 0;}
 unsigned n=0;n=append(w->request,n,sizeof(w->request),activate?"POST /xiaozhi/ota/activate HTTP/1.1\r\n":"POST /xiaozhi/ota/ HTTP/1.1\r\n");
 n=append(w->request,n,sizeof(w->request),"Host: api.tenclass.net\r\nConnection: close\r\nAccept-Encoding: identity\r\n");
 n=append(w->request,n,sizeof(w->request),w->headers);n=append(w->request,n,sizeof(w->request),"Content-Length: ");
 n=decimal(w->request,n,sizeof(w->request),(unsigned)payload);n=append(w->request,n,sizeof(w->request),"\r\n\r\n");n=append(w->request,n,sizeof(w->request),w->payload);
 if(n>=sizeof(w->request)){state(XS_FAILED,XS_CONFIGURATION);return 0;}
 if(!xo_tls_init(w->tls,"api.tenclass.net",&w->anchor,1,now,w->entropy,sizeof(w->entropy))){state(XS_FAILED,XS_TLS_ERROR);return 0;}wipe(w->entropy,sizeof(w->entropy));
 state(XS_TLS,XS_OK);w->socket=xp_connect("api.tenclass.net",443);if(w->socket<0){state(XS_FAILED,XS_SOCKET);return 0;}
 xo_http_init(&w->phase->bootstrap.http);unsigned started=xp_ticks(),sent=0;int ok=0;
 while(!cancelled()&&(unsigned)(xp_ticks()-started)<30000u){
  unsigned flags=br_ssl_engine_current_state(&w->tls->client.eng),progress=0;size_t bytes;unsigned char *buffer;
  if(flags&BR_SSL_CLOSED){S.tls_error=(unsigned)br_ssl_engine_last_error(&w->tls->client.eng);state(XS_FAILED,XS_TLS_ERROR);break;}
  if(flags&BR_SSL_SENDREC){buffer=br_ssl_engine_sendrec_buf(&w->tls->client.eng,&bytes);int r=xp_write(w->socket,buffer,(unsigned)bytes);if(r<0){state(XS_FAILED,XS_SOCKET);break;}if(r){br_ssl_engine_sendrec_ack(&w->tls->client.eng,(unsigned)r);progress=1;}}
  flags=br_ssl_engine_current_state(&w->tls->client.eng);if(flags&BR_SSL_RECVREC){buffer=br_ssl_engine_recvrec_buf(&w->tls->client.eng,&bytes);int r=xp_read(w->socket,buffer,(unsigned)bytes);if(r<0){state(XS_FAILED,XS_SOCKET);break;}if(r){br_ssl_engine_recvrec_ack(&w->tls->client.eng,(unsigned)r);progress=1;}}
  flags=br_ssl_engine_current_state(&w->tls->client.eng);if((flags&BR_SSL_SENDAPP)&&sent<n){buffer=br_ssl_engine_sendapp_buf(&w->tls->client.eng,&bytes);unsigned take=n-sent;if(take>bytes)take=(unsigned)bytes;memcpy(buffer,w->request+sent,take);sent+=take;br_ssl_engine_sendapp_ack(&w->tls->client.eng,take);br_ssl_engine_flush(&w->tls->client.eng,1);progress=1;}
  flags=br_ssl_engine_current_state(&w->tls->client.eng);if(flags&BR_SSL_RECVAPP){buffer=br_ssl_engine_recvapp_buf(&w->tls->client.eng,&bytes);int r=xo_http_feed(&w->phase->bootstrap.http,buffer,(unsigned)bytes,w->phase->bootstrap.body,sizeof(w->phase->bootstrap.body));br_ssl_engine_recvapp_ack(&w->tls->client.eng,bytes);progress=1;if(r<0){state(XS_FAILED,XS_HTTP);break;}if(r){S.http_status=w->phase->bootstrap.http.status;ok=1;break;}}
  if(!progress)xp_delay(5);
 }
 xp_close(w->socket);w->socket=-1;
 if(!ok&&S.state!=XS_FAILED)state(XS_FAILED,xp_usb()?XS_USB:XS_TIMEOUT);
 return ok;
}
static int pump(struct workspace *w){
 unsigned flags=br_ssl_engine_current_state(&w->tls->client.eng);size_t bytes;unsigned char *p;
 if(flags&BR_SSL_CLOSED){S.tls_error=(unsigned)br_ssl_engine_last_error(&w->tls->client.eng);return -1;}
 if(flags&BR_SSL_SENDREC){p=br_ssl_engine_sendrec_buf(&w->tls->client.eng,&bytes);int n=xp_write(w->socket,p,(unsigned)bytes);if(n<0)return -1;if(n)br_ssl_engine_sendrec_ack(&w->tls->client.eng,(unsigned)n);}
 flags=br_ssl_engine_current_state(&w->tls->client.eng);
 if(flags&BR_SSL_RECVREC){p=br_ssl_engine_recvrec_buf(&w->tls->client.eng,&bytes);int n=xp_read(w->socket,p,(unsigned)bytes);if(n<0)return -1;if(n)br_ssl_engine_recvrec_ack(&w->tls->client.eng,(unsigned)n);}
 flags=br_ssl_engine_current_state(&w->tls->client.eng);
 if((flags&BR_SSL_SENDAPP)&&w->tx_sent<w->tx_bytes){p=br_ssl_engine_sendapp_buf(&w->tls->client.eng,&bytes);unsigned n=w->tx_bytes-w->tx_sent;if(n>bytes)n=(unsigned)bytes;memcpy(p,w->tx+w->tx_sent,n);w->tx_sent+=n;br_ssl_engine_sendapp_ack(&w->tls->client.eng,n);br_ssl_engine_flush(&w->tls->client.eng,1);}
 return 1;
}
static int frame(struct workspace *w,unsigned opcode,const uint8_t *data,unsigned n){
 if(w->tx_sent==w->tx_bytes)w->tx_sent=w->tx_bytes=0;uint8_t mask[4];br_hmac_drbg_generate(&w->tls->client.eng.rng,mask,sizeof(mask));
 int bytes=xw_mask(opcode,data,n,mask,w->tx+w->tx_bytes,sizeof(w->tx)-w->tx_bytes);if(bytes<0)return 0;w->tx_bytes+=(unsigned)bytes;return 1;
}
static int voice_send(void *ctx,unsigned opcode,const uint8_t *p,unsigned bytes){return frame(ctx,opcode,p,bytes);}
static int message(struct workspace *w,int opcode,unsigned *ready){
 if(opcode==8)return frame(w,8,w->websocket.control,w->websocket.length)?-2:0;
 if(opcode==9)return frame(w,10,w->websocket.control,w->websocket.length)?1:0;
 if(opcode==10)return 1;
 if(opcode==2){
  if(!*ready)return 0;const uint8_t *packet;unsigned n;uint32_t timestamp;
  if(!xo_audio_unpack(w->config.version,w->websocket.payload,w->websocket.total,&packet,&n,&timestamp))return 0;
  return xv_receive(&w->phase->voice,packet,n);
 }
 if(opcode!=1||!xo_parse_event((const char *)w->websocket.payload,w->websocket.total,&w->event))return 0;
 if(w->event.kind==XO_EVENT_HELLO){if(*ready)return 0;memcpy(w->session,w->event.session,sizeof(w->session));*ready=1;if(!w->session[0]||!xv_open(&w->phase->voice)){voice_publish(w);state(XS_FAILED,XS_AUDIO);return 0;}voice_publish(w);state(XS_WS_READY,XS_OK);return 1;}
 if(!*ready)return 0;
 if(w->event.session[0]&&(!w->session[0]||strcmp(w->event.session,w->session)))return 0;
 if(w->event.kind==XO_EVENT_TTS_START)xv_tts(&w->phase->voice,1);
 if(w->event.kind==XO_EVENT_TTS_STOP)xv_tts(&w->phase->voice,0);
 if(w->event.kind==XO_EVENT_STT||w->event.kind==XO_EVENT_SENTENCE)caption(&w->event);
 return 1;
}
static int consume(struct workspace *w,const uint8_t *p,unsigned n,unsigned *ready,unsigned *used){
 *used=0;for(unsigned i=0;i<n;i++){if(w->phase->voice.count==XV_QUEUE)break;*used=i+1;int op=xw_feed(&w->websocket,p[i]);if(op<0)return 0;if(op){int result=message(w,op,ready);xw_next(&w->websocket);if(result<=0)return result;}}return 1;
}
static void conversation(struct workspace *w){
 if(!xw_url_parse(w->config.websocket_url,&w->url)){state(XS_FAILED,XS_WEBSOCKET);return;}
 uint64_t now=xp_time();uint8_t mac[6];if(!now){state(XS_FAILED,XS_CLOCK);return;}
 if(!xp_identity(mac,w->uuid,w->entropy,now)||!root(w)){state(XS_FAILED,XS_IDENTITY);return;}mac_text(w->mac,mac);
 if(!xo_tls_init(w->tls,w->url.host,&w->anchor,1,now,w->entropy,sizeof(w->entropy))){state(XS_FAILED,XS_TLS_ERROR);return;}wipe(w->entropy,sizeof(w->entropy));
 uint8_t random[16];br_hmac_drbg_generate(&w->tls->client.eng.rng,random,sizeof(random));xw_key(random,w->key,w->accept);
 int headers=xo_headers(w->headers,sizeof(w->headers),w->mac,w->uuid,w->config.websocket_token,w->config.version);
 unsigned n=0;n=append(w->request,n,sizeof(w->request),"GET ");n=append(w->request,n,sizeof(w->request),w->url.path);n=append(w->request,n,sizeof(w->request)," HTTP/1.1\r\nHost: ");n=append(w->request,n,sizeof(w->request),w->url.authority);
 n=append(w->request,n,sizeof(w->request),"\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: ");n=append(w->request,n,sizeof(w->request),w->key);n=append(w->request,n,sizeof(w->request),"\r\n");
 n=append(w->request,n,sizeof(w->request),w->headers);n=append(w->request,n,sizeof(w->request),"\r\n");
 if(headers<0||n>=sizeof(w->request)||n>sizeof(w->tx)){state(XS_FAILED,XS_WEBSOCKET);return;}
 memcpy(w->tx,w->request,n);w->tx_bytes=n;w->tx_sent=0;memset(&w->websocket,0,sizeof(w->websocket));
 w->socket=xp_connect(w->url.host,w->url.port);if(w->socket<0){state(XS_FAILED,XS_SOCKET);return;}
 state(XS_WS_CONNECTING,XS_OK);unsigned start=xp_ticks(),last=start,ping=start,upgraded=0,ready=0,used=0;
 while(!cancelled()){
  if(pump(w)<0){state(ready?XS_CLOSED:XS_FAILED,ready?XS_OK:XS_TLS_ERROR);return;}
  size_t bytes;unsigned char *p=br_ssl_engine_recvapp_buf(&w->tls->client.eng,&bytes);
  if(p&&bytes){last=xp_ticks();int result=1;unsigned accepted=(unsigned)bytes;
   if(!upgraded){
    for(unsigned i=0;i<(unsigned)bytes;i++){
     if(used==sizeof(w->request)){result=0;break;}w->request[used++]=(char)p[i];unsigned consumed=0;int check=xw_upgrade(w->request,used,w->accept,&consumed);
     if(check<0){result=0;break;}
     if(check==1){if(w->tx_sent<w->tx_bytes){result=0;break;}upgraded=1;
      int hello=xo_hello(w->payload,sizeof(w->payload),w->config.version,20);if(hello<0||!frame(w,1,(const uint8_t *)w->payload,(unsigned)hello)){result=0;break;}
      if(i+1<bytes){unsigned rest=0;result=consume(w,p+i+1,(unsigned)bytes-i-1,&ready,&rest);accepted=i+1+rest;}break;
     }
    }
   }else result=consume(w,p,(unsigned)bytes,&ready,&accepted);
   if(accepted)br_ssl_engine_recvapp_ack(&w->tls->client.eng,accepted);
   if(result==-2){
    unsigned closing=xp_ticks();while(!cancelled()&&(unsigned)(xp_ticks()-closing)<500u){if(pump(w)<0)break;if(w->tx_sent==w->tx_bytes&&!(br_ssl_engine_current_state(&w->tls->client.eng)&BR_SSL_SENDREC))break;xp_delay(5);}
    state(XS_CLOSED,XS_OK);return;
   }if(!result){if(S.state!=XS_FAILED)state(XS_FAILED,XS_WEBSOCKET);return;}
  }
  unsigned tick=xp_ticks();if(ready&&!voice_tick(w)){state(XS_FAILED,XS_AUDIO);return;}
  if(!ready&&(unsigned)(tick-start)>30000u){state(XS_FAILED,XS_HELLO);return;}
  if(ready&&(unsigned)(tick-last)>90000u){state(XS_CLOSED,XS_TIMEOUT);return;}
  if(ready&&(unsigned)(tick-ping)>25000u&&w->tx_sent==w->tx_bytes){if(!frame(w,9,(const uint8_t *)"",0)){state(XS_FAILED,XS_WEBSOCKET);return;}ping=tick;}
  xp_delay(5);
 }
 state(XS_CLOSED,xp_usb()?XS_USB:XS_TIMEOUT);
}
void xs_worker(void *argument){
 (void)argument;S.activation[0]=0;S.caption[0]=0;S.caption_kind=0;S.caption_revision++;memset(&S.voice,0,sizeof(S.voice));S.command_count=S.command_head=S.command_overflow=0;struct workspace *w=0;state(XS_WAIT_WIFI,XS_OK);unsigned start=xp_ticks();
 while(!xp_network_ready()&&!S.cancel&&!xp_usb()&&(unsigned)(xp_ticks()-start)<300000u)xp_delay(100);
 if(xp_usb()||S.cancel){state(XS_FAILED,XS_USB);goto finished;}
 if(!xp_network_ready()){state(XS_FAILED,XS_TIMEOUT);goto finished;}
 state(XS_TIME_SYNC,XS_OK);if(!xt_wait()){state(XS_FAILED,xp_usb()?XS_USB:XS_CLOCK);goto finished;}
 state(XS_PREPARING,XS_OK);w=xp_alloc(sizeof(*w));if(!w){xp_memory_report(1,sizeof(*w));state(XS_FAILED,XS_MEMORY);goto finished;}w->socket=-1;
 w->tls=xp_alloc(sizeof(*w->tls));if(!w->tls){xp_memory_report(2,sizeof(*w->tls));state(XS_FAILED,XS_MEMORY);goto finished;}
 w->phase=xp_alloc(sizeof(*w->phase));if(!w->phase){xp_memory_report(3,sizeof(*w->phase));state(XS_FAILED,XS_MEMORY);goto finished;}
 if(!request(w,0))goto finished;
 if(S.http_status!=200||!xo_parse_bootstrap(w->phase->bootstrap.body,w->phase->bootstrap.http.body_bytes,&w->config)){state(XS_FAILED,XS_CONFIGURATION);goto finished;}
 start=xp_ticks();
 while(w->config.status==XO_ACTIVATION_REQUIRED&&!cancelled()&&(unsigned)(xp_ticks()-start)<300000u){
  memcpy(S.activation,w->config.activation_code,sizeof(S.activation));state(XS_ACTIVATION,XS_OK);
  for(unsigned i=0;i<40&&!cancelled();i++)xp_delay(100);if(cancelled())break;
  if(!request(w,w->config.has_challenge?1:0))goto finished;
  if(S.http_status==202)continue;
  if(S.http_status!=200){state(XS_FAILED,XS_HTTP);goto finished;}
  if(w->config.has_challenge&&!request(w,0))goto finished;
  if(S.http_status!=200||!xo_parse_bootstrap(w->phase->bootstrap.body,w->phase->bootstrap.http.body_bytes,&w->config)){state(XS_FAILED,XS_CONFIGURATION);goto finished;}
 }
 if(cancelled()){state(XS_FAILED,xp_usb()?XS_USB:XS_TIMEOUT);goto finished;}
 if(w->config.status==XO_CONFIGURED){memset(w->phase,0,sizeof(*w->phase));w->voice_live=1;state(XS_CONFIGURED,XS_OK);conversation(w);}
 else if(w->config.status==XO_MQTT_ONLY)state(XS_MQTT_CONFIGURED,XS_MQTT);
 else state(XS_FAILED,XS_CONFIGURATION);
finished:
 if(w){
  if(w->phase&&w->voice_live)xv_close(&w->phase->voice);
  unsigned key=xp_lock();S.voice.ready=0;S.voice.phase=XV_IDLE;xl_reset(&S.voice.meter);xp_unlock(key);
  if(w->socket>=0)xp_close(w->socket);
  if(w->phase){wipe(w->phase,sizeof(*w->phase));xp_free(w->phase);}
  if(w->tls){wipe(w->tls,sizeof(*w->tls));xp_free(w->tls);}
  wipe(w,sizeof(*w));xp_free(w);
 }
 /* Returning dynamic threads release their stack later in the idle thread. */
 unsigned key=xp_lock();S.retry_after=xp_ticks()+250u;S.retry_delayed=1;S.busy=0;xp_unlock(key);
}
int xs_quiescent(void){return !S.busy&&(!S.retry_delayed||(int32_t)(xp_ticks()-S.retry_after)>=0);}
int xs_start(void){
 if(!record_ai_allowed())return 0;
 unsigned key=FN(unsigned,0x1802b134,void)();if(S.busy||(S.retry_delayed&&(int32_t)(xp_ticks()-S.retry_after)<0)){FN(void,0x1802b13c,unsigned)(key);return 1;}S.retry_delayed=0;S.busy=1;S.cancel=0;FN(void,0x1802b13c,unsigned)(key);
 void *thread=FN(void *,0x18064604,const char *,void *,void *,unsigned,unsigned,unsigned)("xiaozhi",xs_worker,0,65536,23,10);
 if(!thread){S.busy=0;state(XS_FAILED,XS_THREAD);return 0;}
 if(FN(int,0x180648f8,void *)(thread)){FN(int,0x18064654,void *)(thread);S.busy=0;state(XS_FAILED,XS_THREAD);return 0;}return 1;
}
const unsigned xo_session_working_bytes=sizeof(struct workspace)+sizeof(struct xo_tls)+sizeof(union workspace_phase);
const unsigned xo_session_allocation_sizes[]={sizeof(struct workspace),sizeof(struct xo_tls),sizeof(union workspace_phase)};

/* Analysis ABI: TLS and phase/voice offsets now identify pointer fields. */
const unsigned xo_session_layout[]={sizeof(struct workspace),offsetof(struct workspace,tls),offsetof(struct workspace,phase),offsetof(struct workspace,anchor),sizeof(br_x509_trust_anchor),sizeof(br_x509_pkey),offsetof(struct workspace,phase),offsetof(struct workspace,session),offsetof(struct workspace,config)+offsetof(struct xo_config,version)};
