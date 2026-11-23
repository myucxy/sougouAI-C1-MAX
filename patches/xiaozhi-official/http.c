/* Incremental HTTP/1.1 response decoder for bounded official JSON replies. */
#include "http.h"
#include <string.h>
enum {HEADERS=0,FIXED,CHUNK_SIZE,CHUNK_DATA,CHUNK_CR,CHUNK_LF,TRAILER_CR,TRAILER_LF,DONE,FAILED};
void xo_http_init(struct xo_http_response *r){memset(r,0,sizeof(*r));}
static int equal(const char *p,unsigned n,const char *s){unsigned length=0;while(s[length])length++;if(length!=n)return 0;for(unsigned i=0;i<n;i++){unsigned a=(unsigned char)p[i],b=(unsigned char)s[i];if(a>='A'&&a<='Z')a+=32;if(a!=b)return 0;}return 1;}
static int decimal(const char *p,unsigned n,unsigned *v){
 if(!n)return 0;unsigned result=0;for(unsigned i=0;i<n;i++){unsigned c=(unsigned char)p[i];if(c<'0'||c>'9'||result>(16384u-(c-'0'))/10)return 0;result=result*10+c-'0';}*v=result;return 1;
}
static int parse_headers(struct xo_http_response *r){
 char *p=r->headers;unsigned n=r->header_bytes;
 if(n<19||(memcmp(p,"HTTP/1.1 ",9)&&memcmp(p,"HTTP/1.0 ",9))||p[9]<'1'||p[9]>'5'||p[10]<'0'||p[10]>'9'||p[11]<'0'||p[11]>'9'||p[12]!=' ')return 0;
 r->status=(unsigned)(p[9]-'0')*100+(unsigned)(p[10]-'0')*10+(unsigned)(p[11]-'0');
 unsigned at=12;while(at+1<n&&(p[at]!='\r'||p[at+1]!='\n'))at++;at+=2;unsigned seen_length=0,seen_transfer=0,seen_encoding=0;
 while(at+1<n){
  if(p[at]=='\r'&&p[at+1]=='\n')break;unsigned start=at,colon=0;
  while(at+1<n&&(p[at]!='\r'||p[at+1]!='\n')){if(!colon&&p[at]==':')colon=at;at++;}
  if(!colon||colon==start||at+1>=n||p[start]==' '||p[start]=='\t')return 0;
  unsigned value=colon+1,end=at;while(value<end&&(p[value]==' '||p[value]=='\t'))value++;while(end>value&&(p[end-1]==' '||p[end-1]=='\t'))end--;
  if(equal(p+start,colon-start,"content-length")){if(seen_length++||!decimal(p+value,end-value,&r->expected))return 0;}
  else if(equal(p+start,colon-start,"transfer-encoding")){if(seen_transfer++||!equal(p+value,end-value,"chunked"))return 0;r->chunked=1;}
  else if(equal(p+start,colon-start,"content-encoding")){if(seen_encoding++||!equal(p+value,end-value,"identity"))return 0;}
  at+=2;
 }
 if(seen_length&&seen_transfer)return 0;
 if(r->status==204){if(r->chunked||r->expected)return 0;r->state=DONE;return 1;}
 if(!seen_length&&!r->chunked)return 0;
 r->remaining=r->expected;r->state=r->chunked?CHUNK_SIZE:r->expected?FIXED:DONE;return 1;
}
static int failure(struct xo_http_response *r){r->state=FAILED;return -1;}
int xo_http_feed(struct xo_http_response *r,const unsigned char *input,unsigned n,char *body,unsigned cap){
 if(!r||(!input&&n)||!body||!cap)return -1;
 for(unsigned i=0;i<n;i++){
  unsigned c=input[i];if(r->state==FAILED)return -1;if(r->state==DONE)return failure(r);
  if(r->state==HEADERS){
   if(r->header_bytes+1>=sizeof(r->headers)||c==0||c>0x7e||(c<0x20&&c!='\r'&&c!='\n'&&c!='\t'))return failure(r);
   r->headers[r->header_bytes++]=(char)c;
   if(r->header_bytes>=4&&!memcmp(r->headers+r->header_bytes-4,"\r\n\r\n",4)){
    if(!parse_headers(r)||r->expected>=cap)return failure(r);if(r->state==DONE)body[0]=0;
   }
  }else if(r->state==FIXED||r->state==CHUNK_DATA){
   if(r->body_bytes+1>=cap||r->body_bytes>=16384)return failure(r);body[r->body_bytes++]=(char)c;
   if(!--r->remaining){r->state=r->chunked?CHUNK_CR:DONE;body[r->body_bytes]=0;}
  }else if(r->state==CHUNK_SIZE){
   if(c=='\n'){
    if(!r->line_bytes||r->line[r->line_bytes-1]!='\r')return failure(r);unsigned value=0,digits=0;
    for(unsigned j=0;j+1<r->line_bytes;j++){
     unsigned x=(unsigned char)r->line[j];if(x>='0'&&x<='9')x-='0';else if(x>='a'&&x<='f')x=x-'a'+10;else if(x>='A'&&x<='F')x=x-'A'+10;else return failure(r);
     if(++digits>8||value>(16384u-x)/16)return failure(r);value=value*16+x;
    }
    if(!digits||value>16384-r->body_bytes||value>=cap-r->body_bytes)return failure(r);
    r->line_bytes=0;r->remaining=value;r->state=value?CHUNK_DATA:TRAILER_CR;
   }else{if(r->line_bytes+1>=sizeof(r->line))return failure(r);r->line[r->line_bytes++]=(char)c;}
  }else if(r->state==CHUNK_CR){if(c!='\r')return failure(r);r->state=CHUNK_LF;}
  else if(r->state==CHUNK_LF){if(c!='\n')return failure(r);r->state=CHUNK_SIZE;}
  else if(r->state==TRAILER_CR){if(c!='\r')return failure(r);r->state=TRAILER_LF;}
  else if(r->state==TRAILER_LF){if(c!='\n')return failure(r);r->state=DONE;body[r->body_bytes]=0;}
 }
 return r->state==FAILED?-1:r->state==DONE?1:0;
}
int xo_http_eof(struct xo_http_response *r){if(!r)return -1;return r->state==DONE?1:failure(r);}
