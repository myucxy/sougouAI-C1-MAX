#include "websocket.h"
#include "official.h"
#include "bearssl.h"
#include <string.h>
static const char base64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void b64(const uint8_t *p,unsigned n,char *out){
 unsigned j=0;for(unsigned i=0;i<n;i+=3){unsigned a=p[i],b=i+1<n?p[i+1]:0,c=i+2<n?p[i+2]:0;
 out[j++]=base64[a>>2];out[j++]=base64[((a&3)<<4)|(b>>4)];out[j++]=i+1<n?base64[((b&15)<<2)|(c>>6)]:'=';out[j++]=i+2<n?base64[c&63]:'=';}out[j]=0;
}
int xw_url_parse(const char *url,struct xw_url *u){
 if(!url||!u||!xo_secure_url(url,1))return 0;memset(u,0,sizeof(*u));unsigned at=6,n=0;
 while(url[at]&&url[at]!=':'&&url[at]!='/'){if(n+1>=sizeof(u->host))return 0;u->host[n++]=url[at++];}
 unsigned end=at;u->port=443;
 if(url[at]==':'){u->port=0;at++;while(url[at]>='0'&&url[at]<='9')u->port=u->port*10+(unsigned)(url[at++]-'0');end=at;}
 if(end-6>=sizeof(u->authority))return 0;memcpy(u->authority,url+6,end-6);
 const char *path=url[at]?url+at:"/";unsigned size=(unsigned)strlen(path);if(size>=sizeof(u->path))return 0;memcpy(u->path,path,size+1);return 1;
}
void xw_key(const uint8_t random[16],char key[25],char accept[29]){
 uint8_t digest[20];b64(random,16,key);br_sha1_context sha;br_sha1_init(&sha);br_sha1_update(&sha,key,24);
 br_sha1_update(&sha,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",36);br_sha1_out(&sha,digest);b64(digest,20,accept);
}
static unsigned lower(unsigned c){return c>='A'&&c<='Z'?c+32:c;}
static int equal(const char *a,unsigned n,const char *b){if(strlen(b)!=n)return 0;for(unsigned i=0;i<n;i++)if(lower((unsigned char)a[i])!=lower((unsigned char)b[i]))return 0;return 1;}
static int token(const char *p,unsigned n,const char *want){
 unsigned at=0;while(at<n){while(at<n&&(p[at]==' '||p[at]=='\t'||p[at]==','))at++;unsigned start=at;while(at<n&&p[at]!=',')at++;unsigned end=at;while(end>start&&(p[end-1]==' '||p[end-1]=='\t'))end--;if(equal(p+start,end-start,want))return 1;}return 0;
}
int xw_upgrade(const char *p,unsigned n,const char *accept,unsigned *consumed){
 *consumed=0;if(n>4096)return -1;unsigned end=0;
 for(unsigned i=0;i+3<n;i++)if(!memcmp(p+i,"\r\n\r\n",4)){end=i+4;break;}
 if(!end)return 0;for(unsigned i=0;i<end;i++)if((unsigned char)p[i]<0x20&&p[i]!='\r'&&p[i]!='\n'&&p[i]!='\t')return -1;
 if(end<16||memcmp(p,"HTTP/1.1 101",12)||p[12]!=' ')return -1;
 unsigned at=0;while(at+1<end&&memcmp(p+at,"\r\n",2))at++;at+=2;unsigned got=0;
 while(at+2<end){unsigned stop=at;while(stop+1<end&&memcmp(p+stop,"\r\n",2))stop++;if(stop==at)break;
  unsigned colon=at;while(colon<stop&&p[colon]!=':')colon++;if(colon==stop||p[at]==' '||p[at]=='\t')return -1;
  unsigned start=colon+1,last=stop;while(start<last&&(p[start]==' '||p[start]=='\t'))start++;while(last>start&&(p[last-1]==' '||p[last-1]=='\t'))last--;
  unsigned flag=0;
  if(equal(p+at,colon-at,"upgrade")){if(!equal(p+start,last-start,"websocket"))return -1;flag=1;}
  else if(equal(p+at,colon-at,"connection")){if(!token(p+start,last-start,"upgrade"))return -1;flag=2;}
  else if(equal(p+at,colon-at,"sec-websocket-accept")){if(last-start!=28||memcmp(p+start,accept,28))return -1;flag=4;}
  else if(equal(p+at,colon-at,"sec-websocket-extensions")||equal(p+at,colon-at,"sec-websocket-protocol"))return -1;
  if(flag&&(got&flag))return -1;got|=flag;at=stop+2;
 }
 if(got!=7)return -1;*consumed=end;return 1;
}
int xw_mask(unsigned op,const uint8_t *p,unsigned n,const uint8_t key[4],uint8_t *out,unsigned cap){
 unsigned head=n<126?6:8;if(!p||!key||!out||n>XW_LIMIT||cap<head+n||(op!=1&&op!=2&&op!=8&&op!=9&&op!=10)||(op>=8&&n>125))return -1;
 out[0]=(uint8_t)(0x80|op);out[1]=(uint8_t)(0x80|(n<126?n:126));unsigned at=2;if(n>=126){out[2]=(uint8_t)(n>>8);out[3]=(uint8_t)n;at=4;}memcpy(out+at,key,4);
 for(unsigned i=0;i<n;i++)out[at+4+i]=p[i]^key[i&3];return (int)(head+n);
}
void xw_next(struct xw_rx *r){r->header_bytes=r->needed=r->length=r->received=0;if(r->final&&r->opcode<8){r->fragment=r->total=0;}}
int xw_feed(struct xw_rx *r,uint8_t b){
 if(r->header_bytes<2){r->header[r->header_bytes++]=b;if(r->header_bytes<2)return 0;
  unsigned op=r->header[0]&15;r->final=r->header[0]>>7;r->opcode=op;
  if((r->header[0]&0x70)||(b&0x80)||op==3||op==4||op==5||op==6||op==7||op>10||(op>=8&&!r->final))return -1;
  if(op<8){if((op==0&&!r->fragment)||(op&&r->fragment))return -1;if(op)r->fragment=op;}
  unsigned n=b&127;if(op>=8&&n>125)return -1;r->needed=n==126?4:n==127?10:2;r->length=n;
 }
 else if(r->header_bytes<r->needed){r->header[r->header_bytes++]=b;if(r->header_bytes<r->needed)return 0;
  if(r->needed==10)return -1;r->length=((unsigned)r->header[2]<<8)|r->header[3];if(r->length<126)return -1;
 }else{
  unsigned base=r->opcode>=8?0:r->total;
  if(r->opcode>=8){if(r->received>=125)return -1;r->control[r->received++]=b;}
  else{if(base+r->received>=XW_LIMIT)return -1;r->payload[base+r->received++]=b;}
 }
 if(r->header_bytes<r->needed)return 0;
 unsigned base=r->opcode>=8?0:r->total;
 if(r->length>XW_LIMIT-base)return -1;if(r->received<r->length)return 0;
 if(r->opcode>=8){if(r->opcode==8&&r->length==1)return -1;return (int)r->opcode;}
 r->total+=r->length;r->payload[r->total]=0;
 if(r->final)return (int)r->fragment;xw_next(r);return 0;
}
