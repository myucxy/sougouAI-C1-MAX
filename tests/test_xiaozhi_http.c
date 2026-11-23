#include "../patches/xiaozhi-official/http.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct xo_http_response response;
static char body[16385];
static int feed(const char *s){xo_http_init(&response);return xo_http_feed(&response,(const unsigned char *)s,(unsigned)strlen(s),body,sizeof(body));}
int main(void){
 const char *fixed="HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Type: application/json\r\n\r\n{}";
 const char *chunked="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\n{\r\n1\r\n}\r\n0\r\n\r\n";
 for(unsigned kind=0;kind<2;kind++){
  const char *s=kind?chunked:fixed;unsigned n=(unsigned)strlen(s);
  for(unsigned split=0;split<=n;split++){
   xo_http_init(&response);assert(xo_http_feed(&response,(const unsigned char *)s,split,body,sizeof(body))>=0);
   assert(xo_http_feed(&response,(const unsigned char *)s+split,n-split,body,sizeof(body))==1);
   assert(response.status==200&&!strcmp(body,"{}")&&xo_http_eof(&response)==1);
  }
  xo_http_init(&response);for(unsigned i=0;i<n;i++)assert(xo_http_feed(&response,(const unsigned char *)s+i,1,body,sizeof(body))==(i+1==n));
  for(unsigned end=0;end<n;end++){xo_http_init(&response);assert(xo_http_feed(&response,(const unsigned char *)s,end,body,sizeof(body))>=0);assert(xo_http_eof(&response)==-1);}
 }
 assert(feed("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length-Extra-Long-Unknown: ignored\r\n\r\n{}")==1);
 assert(feed("HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\n\r\n")==1&&response.status==202);
 const char *bad[]={"HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}",
 "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n{}",
 "HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n", "HTTP/1.1 200 OK\r\nContent-Length: 999999\r\n\r\n",
 "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Encoding: gzip\r\n\r\n{}",
 "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n-1\r\nx\r\n0\r\n\r\n",
 "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4001\r\n", "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}x"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(feed(bad[i])==-1);
 puts("PASS: fragmented HTTP content-length/chunked, pending activation, truncation, ambiguous framing, compression and size rejection.");
}
