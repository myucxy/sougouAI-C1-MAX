#ifndef C208_OFFICIAL_HTTP_H
#define C208_OFFICIAL_HTTP_H
#define XO_HTTP_HEADER_CAP 4096u
struct xo_http_response {
 unsigned state,header_bytes,status,body_bytes,expected,remaining,line_bytes,chunked,chunk_digits,chunk_size,total;
 char headers[XO_HTTP_HEADER_CAP];char line[65];
};
void xo_http_init(struct xo_http_response *);
/* Feed raw decrypted HTTP bytes, body output owned by caller. 0 progress, 1 complete, -1 rejected. */
int xo_http_feed(struct xo_http_response *,const unsigned char *,unsigned,char *body,unsigned capacity);
int xo_http_eof(struct xo_http_response *);
#endif
