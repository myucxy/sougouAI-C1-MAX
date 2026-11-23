/* Bounded official bootstrap/wire adapter; no flash-write or network API. */
#include "official.h"
#include <string.h>
struct token {unsigned start,end,first,next,type;};
struct json {const unsigned char *s;unsigned n,at,count;struct token t[192];};
enum {OBJECT=1,ARRAY,STRING,NUMBER,TRUE_VALUE,FALSE_VALUE,NULL_VALUE};
static int utf8(const unsigned char *s,unsigned n){
 for(unsigned i=0;i<n;){unsigned c=s[i++],need,lower=0x80,upper=0xbf;
  if(c<0x80)continue;if(c>=0xc2&&c<=0xdf)need=1;
  else if(c>=0xe0&&c<=0xef){need=2;if(c==0xe0)lower=0xa0;if(c==0xed)upper=0x9f;}
  else if(c>=0xf0&&c<=0xf4){need=3;if(c==0xf0)lower=0x90;if(c==0xf4)upper=0x8f;}else return 0;
  if(i+need>n||s[i]<lower||s[i]>upper)return 0;i++;while(--need)if(s[i]<0x80||s[i++]>0xbf)return 0;
 }return 1;
}
static int hex(unsigned c){return c>='0'&&c<='9'?(int)(c-'0'):c>='a'&&c<='f'?(int)(c-'a'+10):c>='A'&&c<='F'?(int)(c-'A'+10):-1;}
static int codeunit(struct json *j,unsigned *value){
 if(j->at+4>j->n)return 0;unsigned v=0;for(unsigned i=0;i<4;i++){int x=hex(j->s[j->at++]);if(x<0)return 0;v=(v<<4)|(unsigned)x;}*value=v;return 1;
}
static void ws(struct json *j){while(j->at<j->n&&(j->s[j->at]==' '||j->s[j->at]=='\r'||j->s[j->at]=='\n'||j->s[j->at]=='\t'))j->at++;}
static int string_end(struct json *j){
 while(j->at<j->n){unsigned c=j->s[j->at++];if(c=='"')return 1;if(c<0x20)return 0;
  if(c=='\\'){if(j->at==j->n)return 0;c=j->s[j->at++];if(c=='u'){
    unsigned u;if(!codeunit(j,&u))return 0;
    if(u>=0xd800&&u<=0xdbff){unsigned low;if(j->at+2>j->n||j->s[j->at++]!='\\'||j->s[j->at++]!='u'||!codeunit(j,&low)||low<0xdc00||low>0xdfff)return 0;}
    else if(u>=0xdc00&&u<=0xdfff)return 0;
   }else if(c!='"'&&c!='\\'&&c!='/'&&c!='b'&&c!='f'&&c!='n'&&c!='r'&&c!='t')return 0;
  }
 }return 0;
}
static int decode_kind(const struct json *j,unsigned index,char *out,unsigned capacity,int display){
 if(!index||j->t[index].type!=STRING||!capacity)return 0;unsigned at=j->t[index].start+1,end=j->t[index].end-1,n=0;
 while(at<end){unsigned c=j->s[at++];if(c=='\\'){
  c=j->s[at++];if(c=='u'){
   unsigned cp=0;for(unsigned i=0;i<4;i++)cp=(cp<<4)|(unsigned)hex(j->s[at++]);
   if(cp>=0xd800&&cp<=0xdbff){at+=2;unsigned low=0;for(unsigned i=0;i<4;i++)low=(low<<4)|(unsigned)hex(j->s[at++]);cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);}
   /* Embedded NUL/control text cannot enter headers, status labels or URLs. */
   if(display&&(cp==9||cp==10||cp==13))cp=' ';else if(cp<0x20||cp==0x7f)return 0;
   unsigned bytes=cp<0x80?1:cp<0x800?2:cp<0x10000?3:4;if(n+bytes>=capacity){if(display==2){out[n]=0;return 1;}return 0;}
   if(bytes==1)out[n++]=(char)cp;
   else{out[n++]=(char)((bytes==2?0xc0:bytes==3?0xe0:0xf0)|(cp>>(6*(bytes-1))));for(unsigned i=bytes-1;i;i--)out[n++]=(char)(0x80|((cp>>(6*(i-1)))&63));}continue;
  }
  if(c=='b'||c=='f'||c=='n'||c=='r'||c=='t'){if(display&&(c=='n'||c=='r'||c=='t'))c=' ';else return 0;}
 }
 if(display==2&&c>=0xc2){unsigned bytes=c<0xe0?2:c<0xf0?3:4;if(n+bytes>=capacity){out[n]=0;return 1;}out[n++]=(char)c;while(--bytes)out[n++]=(char)j->s[at++];continue;}
 if(c<0x20||c==0x7f)return 0;if(n+1>=capacity){if(display==2){out[n]=0;return 1;}return 0;}out[n++]=(char)c;
 }out[n]=0;return 1;
}
static int decode(const struct json *j,unsigned i,char *out,unsigned n){return decode_kind(j,i,out,n,0);}
static int keyeq(const struct json *j,unsigned a,unsigned b){char x[129],y[129];return decode(j,a,x,sizeof(x))&&decode(j,b,y,sizeof(y))&&!strcmp(x,y);}
static unsigned parse(struct json *j,unsigned depth){
 ws(j);if(depth>8||j->at==j->n||j->count+1>=192)return 0;
 unsigned index=++j->count;struct token *t=&j->t[index];t->start=j->at;unsigned c=j->s[j->at++];
 if(c=='{'||c=='['){
  t->type=c=='{'?OBJECT:ARRAY;ws(j);unsigned last=0,close=c=='{'?'}':']';
  if(j->at<j->n&&j->s[j->at]==close)j->at++;
  else for(;;){
   if(t->type==OBJECT){
    ws(j);if(j->at==j->n||j->s[j->at]!='"')return 0;unsigned k=parse(j,depth+1);if(!k)return 0;
    /* Reject duplicate decoded keys, including escaped spellings. */
    char key[129];if(!decode(j,k,key,sizeof(key)))return 0;
    for(unsigned p=t->first;p;p=j->t[j->t[p].next].next)if(keyeq(j,p,k))return 0;
    if(last)j->t[last].next=k;else t->first=k;ws(j);if(j->at==j->n||j->s[j->at++]!=':')return 0;
    unsigned v=parse(j,depth+1);if(!v)return 0;j->t[k].next=v;last=v;
   }else{unsigned v=parse(j,depth+1);if(!v)return 0;if(last)j->t[last].next=v;else t->first=v;last=v;}
   ws(j);if(j->at==j->n)return 0;c=j->s[j->at++];if(c==close)break;if(c!=',')return 0;
  }
 }else if(c=='"'){t->type=STRING;if(!string_end(j))return 0;}
 else if(c=='t'||c=='f'||c=='n'){
  const char *literal=c=='t'?"true":c=='f'?"false":"null";unsigned n=c=='f'?5:4;
  if(t->start+n>j->n||memcmp(j->s+t->start,literal,n))return 0;j->at=t->start+n;t->type=c=='t'?TRUE_VALUE:c=='f'?FALSE_VALUE:NULL_VALUE;
 }else{
  t->type=NUMBER;j->at=t->start;if(j->s[j->at]=='-')j->at++;if(j->at==j->n)return 0;
  if(j->s[j->at]=='0')j->at++;else{if(j->s[j->at]<'1'||j->s[j->at]>'9')return 0;while(j->at<j->n&&j->s[j->at]>='0'&&j->s[j->at]<='9')j->at++;}
  if(j->at<j->n&&j->s[j->at]=='.'){j->at++;unsigned begin=j->at;while(j->at<j->n&&j->s[j->at]>='0'&&j->s[j->at]<='9')j->at++;if(begin==j->at)return 0;}
  if(j->at<j->n&&(j->s[j->at]=='e'||j->s[j->at]=='E')){j->at++;if(j->at<j->n&&(j->s[j->at]=='+'||j->s[j->at]=='-'))j->at++;unsigned begin=j->at;while(j->at<j->n&&j->s[j->at]>='0'&&j->s[j->at]<='9')j->at++;if(begin==j->at)return 0;}
 }
 t->end=j->at;return index;
}
static unsigned field(const struct json *j,unsigned obj,const char *name){
 if(!obj||j->t[obj].type!=OBJECT)return 0;for(unsigned p=j->t[obj].first;p;p=j->t[j->t[p].next].next){char k[129];if(decode(j,p,k,sizeof(k))&&!strcmp(k,name))return j->t[p].next;}return 0;
}
static int number(const struct json *j,unsigned token,uint64_t *value){
 if(!token||j->t[token].type!=NUMBER)return 0;uint64_t v=0;
 for(unsigned p=j->t[token].start;p<j->t[token].end;p++){unsigned c=j->s[p];if(c<'0'||c>'9'||v>(UINT64_MAX-(c-'0'))/10)return 0;v=v*10+c-'0';}*value=v;return 1;
}
static int optional_string(const struct json *j,unsigned obj,const char *name,char *out,unsigned n){unsigned t=field(j,obj,name);return !t||decode(j,t,out,n);}
int xo_secure_url(const char *url,int websocket){
 const char *prefix=websocket?"wss://":"https://";unsigned n=websocket?6:8;if(strncmp(url,prefix,n))return 0;
 unsigned at=n,label=0,host=0;for(;url[at]&&url[at]!='/'&&url[at]!=':';at++){
  unsigned c=(unsigned char)url[at];if(c=='.'){if(!label||url[at-1]=='-')return 0;label=0;}else if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'){if(!label&&c=='-')return 0;label++;if(label>63)return 0;}else return 0;host++;
 }if(!label||host>253||url[at-1]=='-')return 0;
 if(url[at]==':'){unsigned port=0,count=0;at++;while(url[at]>='0'&&url[at]<='9'){if(++count>5)return 0;port=port*10+(unsigned)(url[at++]-'0');}if(!port||port>65535)return 0;}
 if(url[at]&&url[at]!='/')return 0;
 for(;url[at];at++){unsigned c=(unsigned char)url[at];if(c<=0x20||c>=0x7f||c=='#'||c=='\\')return 0;}
 return 1;
}
int xo_parse_bootstrap(const char *body,unsigned bytes,struct xo_config *out){
 if(!out)return 0;memset(out,0,sizeof(*out));out->status=XO_INVALID;
 if(!body||!bytes||bytes>XO_BODY_LIMIT||!utf8((const unsigned char *)body,bytes))return 0;
 struct json j={0};j.s=(const unsigned char *)body;j.n=bytes;unsigned root=parse(&j,0);ws(&j);
 if(!root||j.at!=bytes||j.t[root].type!=OBJECT)return 0;
 struct xo_config next={0};next.version=1;next.activation_timeout_ms=30000;
 unsigned act=field(&j,root,"activation");if(act){
  if(j.t[act].type!=OBJECT||!optional_string(&j,act,"code",next.activation_code,sizeof(next.activation_code)))return 0;
  unsigned message=field(&j,act,"message");if(message&&!decode_kind(&j,message,next.activation_message,sizeof(next.activation_message),1))return 0;
  unsigned challenge=field(&j,act,"challenge");if(challenge){char value[513];if(!decode(&j,challenge,value,sizeof(value)))return 0;next.has_challenge=!!value[0];}
  unsigned timeout=field(&j,act,"timeout_ms");uint64_t value;if(timeout){if(!number(&j,timeout,&value)||value<1000||value>300000)return 0;next.activation_timeout_ms=(unsigned)value;}
 }
 unsigned w=field(&j,root,"websocket");if(w){
  if(j.t[w].type!=OBJECT||!decode(&j,field(&j,w,"url"),next.websocket_url,sizeof(next.websocket_url))||!xo_secure_url(next.websocket_url,1)||!optional_string(&j,w,"token",next.websocket_token,sizeof(next.websocket_token)))return 0;
  unsigned v=field(&j,w,"version");uint64_t value;if(v){if(!number(&j,v,&value)||value<1||value>3)return 0;next.version=(unsigned)value;}
 }
 unsigned clock=field(&j,root,"server_time");if(clock){unsigned t=field(&j,clock,"timestamp");if(j.t[clock].type!=OBJECT||(t&&!number(&j,t,&next.server_timestamp_ms)))return 0;}
 unsigned mqtt=field(&j,root,"mqtt");if(mqtt&&j.t[mqtt].type!=OBJECT)return 0;
 next.ignored_firmware=!!field(&j,root,"firmware");
 next.status=next.activation_code[0]||next.has_challenge?XO_ACTIVATION_REQUIRED:w?XO_CONFIGURED:field(&j,root,"mqtt")?XO_MQTT_ONLY:XO_UNCONFIGURED;
 *out=next;return 1;
}
struct writer {char *p;unsigned n,cap;int failed;};
static void add(struct writer *w,const char *s){while(*s){if(w->n+1>=w->cap){w->failed=1;return;}w->p[w->n++]=*s++;}}
static void digits(struct writer *w,unsigned n){char b[11];unsigned k=0;do{b[k++]=(char)('0'+n%10);n/=10;}while(n);while(k){char s[2]={b[--k],0};add(w,s);}}
static void quoted(struct writer *w,const char *s){
 add(w,"\"");for(;*s;s++){unsigned c=(unsigned char)*s;if(c<0x20||c==0x7f){w->failed=1;break;}if(c=='"'||c=='\\')add(w,"\\");char b[2]={*s,0};add(w,b);}add(w,"\"");
}
static int done(struct writer *w){if(!w->cap)return -1;if(w->failed){w->p[0]=0;return -1;}w->p[w->n]=0;return (int)w->n;}
static int device(const char *s){if(!s)return 0;for(unsigned i=0;i<17;i++){if(!s[i])return 0;if(i%3==2){if(s[i]!=':')return 0;}else if(hex((unsigned char)s[i])<0)return 0;}return !s[17];}
static int client(const char *s){if(!s)return 0;for(unsigned i=0;i<36;i++){if(!s[i])return 0;if(i==8||i==13||i==18||i==23){if(s[i]!='-')return 0;}else if(hex((unsigned char)s[i])<0)return 0;}return !s[36];}
static int text(const char *s,unsigned max){if(!s)return 0;unsigned n=0;for(;*s;s++){unsigned c=(unsigned char)*s;if(c<0x20||c==0x7f||++n>max)return 0;}return 1;}
int xo_bootstrap_body(char *out,unsigned cap,const char *id,const char *uuid,const char *version){
 struct writer w={out,0,cap,0};if(!device(id)||!client(uuid)||!text(version,32)){if(cap)out[0]=0;return -1;}
 add(&w,"{\"version\":2,\"language\":\"zh-CN\",\"flash_size\":8388608,\"mac_address\":");quoted(&w,id);add(&w,",\"uuid\":");quoted(&w,uuid);
 add(&w,",\"chip_model_name\":\"rk2108\",\"application\":{\"name\":\"c208-xiaozhi\",\"version\":");quoted(&w,version);
 add(&w,"},\"board\":{\"type\":\"c208-rk2108\",\"name\":\"C208\"},\"protocol\":{\"features\":{\"mcp\":false,\"aec\":false,\"glyph_push\":false}}}");return done(&w);
}
int xo_headers(char *out,unsigned cap,const char *id,const char *uuid,const char *token,unsigned version){
 struct writer w={out,0,cap,0};if(!device(id)||!client(uuid)||!text(token,1024)||version>3){if(cap)out[0]=0;return -1;}
 for(const char *p=token;*p;p++)if((unsigned char)*p>0x7e){if(cap)out[0]=0;return -1;}
 add(&w,"Device-Id: ");add(&w,id);add(&w,"\r\nClient-Id: ");add(&w,uuid);
 if(!version)add(&w,"\r\nActivation-Version: 1\r\nUser-Agent: c208-rk2108/0.1\r\nAccept-Language: zh-CN\r\nContent-Type: application/json\r\n");
 else{add(&w,"\r\nProtocol-Version: ");digits(&w,version);add(&w,"\r\n");if(*token){add(&w,"Authorization: ");if(!strchr(token,' '))add(&w,"Bearer ");add(&w,token);add(&w,"\r\n");}}return done(&w);
}
int xo_hello(char *out,unsigned cap,unsigned version,unsigned ms){
 struct writer w={out,0,cap,0};if(version<1||version>3||(ms!=20&&ms!=40&&ms!=60)){if(cap)out[0]=0;return -1;}
 add(&w,"{\"type\":\"hello\",\"version\":");digits(&w,version);add(&w,",\"transport\":\"websocket\",\"features\":{\"mcp\":false,\"aec\":false,\"glyph_push\":false},\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,\"channels\":1,\"frame_duration\":");digits(&w,ms);add(&w,"}}");return done(&w);
}
int xo_listen(char *out,unsigned cap,const char *session,int start,int automatic){
 struct writer w={out,0,cap,0};if(!text(session,128)||!*session){if(cap)out[0]=0;return -1;}
 add(&w,"{\"session_id\":");quoted(&w,session);add(&w,",\"type\":\"listen\",\"state\":");quoted(&w,start?"start":"stop");if(start){add(&w,",\"mode\":");quoted(&w,automatic?"auto":"manual");}add(&w,"}");return done(&w);
}
int xo_abort(char *out,unsigned cap,const char *session){
 struct writer w={out,0,cap,0};if(!text(session,128)||!*session){if(cap)out[0]=0;return -1;}
 add(&w,"{\"session_id\":");quoted(&w,session);add(&w,",\"type\":\"abort\"}");return done(&w);
}

static void be16(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void be32(uint8_t *p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static unsigned read16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t read32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
int xo_audio_pack(unsigned version,uint32_t timestamp,const uint8_t *packet,unsigned bytes,uint8_t *out,unsigned capacity){
 unsigned header=version==2?16:version==3?4:0;
 if(version<1||version>3||!packet||!out||!bytes||bytes>8192||capacity<header+bytes)return -1;
 if(version==2){be16(out,2);be16(out+2,0);be32(out+4,0);be32(out+8,timestamp);be32(out+12,bytes);}
 else if(version==3){out[0]=out[1]=0;be16(out+2,bytes);}
 memcpy(out+header,packet,bytes);return (int)(header+bytes);
}
int xo_audio_unpack(unsigned version,const uint8_t *data,unsigned bytes,const uint8_t **packet,unsigned *packet_bytes,uint32_t *timestamp){
 if(!packet||!packet_bytes||!timestamp)return 0;*packet=0;*packet_bytes=0;*timestamp=0;
 unsigned header=version==2?16:version==3?4:0;
 if(version<1||version>3||!data||bytes<=header||bytes-header>8192)return 0;
 if(version==2){if(read16(data)!=2||read16(data+2)||read32(data+4)||read32(data+12)!=bytes-header)return 0;*timestamp=read32(data+8);}
 else if(version==3){if(data[0]||data[1]||read16(data+2)!=bytes-header)return 0;}
 *packet=data+header;*packet_bytes=bytes-header;return 1;
}

int xo_parse_event(const char *body,unsigned bytes,struct xo_event *out){
 if(!out)return 0;memset(out,0,sizeof(*out));if(!body||!bytes||bytes>4096||!utf8((const unsigned char *)body,bytes))return 0;
 struct json j={0};j.s=(const unsigned char *)body;j.n=bytes;unsigned root=parse(&j,0);ws(&j);
 if(!root||j.at!=bytes||j.t[root].type!=OBJECT)return 0;
 char type[33],state[33];unsigned t=field(&j,root,"type");if(!decode(&j,t,type,sizeof(type))||!optional_string(&j,root,"session_id",out->session,sizeof(out->session)))return 0;
 if(!strcmp(type,"hello")){
  char transport[33];if(!decode(&j,field(&j,root,"transport"),transport,sizeof(transport))||strcmp(transport,"websocket"))return 0;out->kind=XO_EVENT_HELLO;return 1;
 }
 if(!strcmp(type,"stt"))out->kind=XO_EVENT_STT;
 else if(!strcmp(type,"tts")){
  if(!decode(&j,field(&j,root,"state"),state,sizeof(state)))return 0;
  if(!strcmp(state,"start"))out->kind=XO_EVENT_TTS_START;else if(!strcmp(state,"stop"))out->kind=XO_EVENT_TTS_STOP;else if(!strcmp(state,"sentence_start"))out->kind=XO_EVENT_SENTENCE;
 }else if(!strcmp(type,"llm")){out->kind=XO_EVENT_EMOTION;if(!optional_string(&j,root,"emotion",out->emotion,sizeof(out->emotion)))return 0;}
 if(out->kind==XO_EVENT_STT||out->kind==XO_EVENT_SENTENCE){if(!decode_kind(&j,field(&j,root,"text"),out->text,sizeof(out->text),2))return 0;}
 return 1;
}
