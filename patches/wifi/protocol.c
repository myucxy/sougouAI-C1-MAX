#include "protocol.h"
#ifdef C208_XIAOZHI_INPUT
#include "../xiaozhi/settings.h"
#endif
#ifdef C208_WIFI_V93
#include "portal.h"
#endif
static unsigned slen(const char *s) { unsigned n=0; while(s[n]) ++n; return n; }
static int same(const char *p, unsigned n, const char *s) {
    unsigned k=slen(s); if(n!=k) return 0; for(unsigned i=0;i<n;i++) if(p[i]!=s[i]) return 0; return 1;
}
static int lower(int c) { return c>='A'&&c<='Z'?c+32:c; }
static int ieq(const char *p, unsigned n, const char *s) {
    if(n!=slen(s)) return 0; for(unsigned i=0;i<n;i++) if(lower((unsigned char)p[i])!=s[i]) return 0; return 1;
}
static int hex(int c) {
    if(c>='0'&&c<='9') return c-'0'; c=lower(c); return c>='a'&&c<='f'?c-'a'+10:-1;
}
static int decode(const char *p, unsigned n, char *out, unsigned cap) {
    unsigned k=0; for(unsigned i=0;i<n;i++) {
        int c=(unsigned char)p[i];
        if(c=='%') { if(i+2>=n) return 0; int a=hex(p[i+1]), b=hex(p[i+2]); if(a<0||b<0) return 0; c=a*16+b; i+=2; }
        else if(c=='+') c=' ';
        if(c<32||c==127||k+1>=cap) return 0;
        out[k++]=(char)c;
    } out[k]=0; return 1;
}
int credentials_valid(const struct credentials *n) {
    unsigned s=0,p=0;
    while(s<sizeof(n->ssid)&&n->ssid[s]) { unsigned c=(unsigned char)n->ssid[s++]; if(c<32||c==127) return 0; }
    while(p<sizeof(n->password)&&n->password[p]) { unsigned c=(unsigned char)n->password[p++]; if(c<32||c>126) return 0; }
    return s>0&&s<=32&&p<=WIFI_PASS_MAX&&(p==0||p>=8);
}
int http_request(const char *b, unsigned n, struct credentials *out) {
    if(n>HTTP_CAP) return -1;
    unsigned line=0; while(line+1<n&&(b[line]!='\r'||b[line+1]!='\n')) line++;
    if(line+1>=n) return n==HTTP_CAP?-1:0;
    int get=same(b,line,"GET / HTTP/1.1")||same(b,line,"GET / HTTP/1.0");
    int post=same(b,line,"POST /configure HTTP/1.1")||same(b,line,"POST /configure HTTP/1.0");
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V95)
    int networks=same(b,line,"GET /networks HTTP/1.1")||same(b,line,"GET /networks HTTP/1.0");
    int rescan=same(b,line,"POST /scan HTTP/1.1")||same(b,line,"POST /scan HTTP/1.0");
#ifdef C208_WIFI_V95
    int status=same(b,line,"GET /status HTTP/1.1")||same(b,line,"GET /status HTTP/1.0");
    int provision=same(b,line,"POST /provision HTTP/1.1")||same(b,line,"POST /provision HTTP/1.0");
#ifdef C208_XIAOZHI_INPUT
    int xz_get=same(b,line,"GET /xiaozhi/settings HTTP/1.1")||same(b,line,"GET /xiaozhi/settings HTTP/1.0");
    int xz_post=same(b,line,"POST /xiaozhi/settings HTTP/1.1")||same(b,line,"POST /xiaozhi/settings HTTP/1.0");
    if(!get&&!post&&!networks&&!rescan&&!status&&!provision&&!xz_get&&!xz_post)return -1;
#else
    if(!get&&!post&&!networks&&!rescan&&!status&&!provision)return -1;
#endif
#elif defined(C208_WIFI_V93)
    int portal_read=portal_get_line(b,line);
    if(!get&&!post&&!networks&&!rescan&&!portal_read)return -1;
#else
    if(!get&&!post&&!networks&&!rescan)return -1;
#endif
#else
    if(!get&&!post) return -1;
#endif
    unsigned at=line+2, length=0, lengths=0, type=0;
#ifdef C208_WIFI_V93
    unsigned hosts=0,canonical_host=0;
#endif
    while(1) {
        unsigned end=at; while(end+1<n&&(b[end]!='\r'||b[end+1]!='\n')) end++;
        if(end+1>=n) return n==HTTP_CAP?-1:0;
        if(end==at) { at+=2; break; }
        unsigned colon=at; while(colon<end&&b[colon]!=':') colon++;
        if(colon==at||colon==end||b[at]==' '||b[at]=='\t') return -1;
        unsigned val=colon+1, last=end;
        while(val<last&&(b[val]==' '||b[val]=='\t')) val++;
        while(last>val&&(b[last-1]==' '||b[last-1]=='\t')) last--;
        for(unsigned j=at;j<end;j++) if((unsigned char)b[j]<32&&b[j]!='\t') return -1;
#ifdef C208_WIFI_V93
        if(ieq(b+at,colon-at,"host")) {
            if(++hosts>1||val==last)return -1;
            canonical_host=same(b+val,last-val,"192.168.84.1")||same(b+val,last-val,"192.168.84.1:80");
        } else
#endif
        if(ieq(b+at,colon-at,"content-length")) {
            if(++lengths>1||val==last) return -1;
            for(unsigned j=val;j<last;j++) { if(b[j]<'0'||b[j]>'9'||length>HTTP_CAP/10) return -1; length=length*10+(unsigned)(b[j]-'0'); }
            if(length>512) return -1;
        } else if(ieq(b+at,colon-at,"transfer-encoding")) return -1;
        else if(ieq(b+at,colon-at,"content-type")) {
            if(type++) return -1;
            if(!same(b+val,last-val,"application/x-www-form-urlencoded")) return -1;
        } else if(ieq(b+at,colon-at,"origin")) {
            if(!same(b+val,last-val,"http://192.168.84.1")
#ifdef C208_WIFI_V95
                &&!provision&&!status&&!get
#ifdef C208_XIAOZHI_INPUT
                &&!xz_get&&!xz_post
#endif
#endif
            ) return -1;
        }
        at=end+2;
    }
#ifdef C208_XIAOZHI_INPUT
    if(at+length>HTTP_CAP||(!post&&!xz_post&&length)) return -1;
#else
    if(at+length>HTTP_CAP||(!post&&length)) return -1;
#endif
    if(n<at+length) return 0;
    if(n!=at+length) return -1; /* one request per connection */
#ifdef C208_WIFI_V93
    /* All read probes canonicalize to the local IP before page JS posts data.
     * Only GETs redirect; an off-origin form submission is never redirected. */
    if(portal_read&&((hosts&&!canonical_host)||(!get&&!networks)))return 6;
    if(!portal_read&&hosts&&!canonical_host)return -1;
#endif
#ifdef C208_WIFI_V95
#ifdef C208_XIAOZHI_INPUT
    if(xz_get)return 9;
    if(xz_post){enum xz_mode mode;return lengths==1&&type==1&&xz_settings_parse_form(b+at,length,&mode)?10:-1;}
#endif
    if(status)return 7;
    if(provision)return lengths==1&&length==0?8:-1;
#endif
    if(get) return 1;
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V95)
    if(networks)return 4;
    if(rescan)return lengths==1&&length==0?5:-1;
#endif
    if(lengths!=1||type!=1||!length) return -1;
    struct credentials tmp={{0},{0}}; unsigned seen=0,end=at+length;
    while(at<end) {
        unsigned next=at; while(next<end&&b[next]!='&') next++;
        unsigned eq=at; while(eq<next&&b[eq]!='=') eq++;
        if(eq==next) return -1;
        if(same(b+at,eq-at,"ssid")) { if(seen&1||!decode(b+eq+1,next-eq-1,tmp.ssid,sizeof(tmp.ssid))) return -1; seen|=1; }
        else if(same(b+at,eq-at,"password")) { if(seen&2||!decode(b+eq+1,next-eq-1,tmp.password,sizeof(tmp.password))) return -1; seen|=2; }
        else return -1;
        if(next<end&&next+1==end) return -1;
        at=next+1;
    }
    if(seen!=3||!credentials_valid(&tmp)) return -1;
    *out=tmp; return 2;
}
uint32_t record_hash(const void *p,unsigned n) {
    const uint8_t *b=p; uint32_t h=0; while(n--) h^=(h<<5)+*b+++(h>>2); return h;
}
int record_valid(const struct config_record *r) {
    if(r->magic!=0x46573243u||r->hash!=record_hash(r,offsetof(struct config_record,hash))) return 0;
    /* An empty SSID is a durable tombstone, so forgetting survives a torn second write. */
    if(!r->net.ssid[0]) { for(unsigned i=0;i<sizeof(r->net);i++) if(((const uint8_t *)&r->net)[i]) return 0; return 1; }
    return credentials_valid(&r->net);
}
void record_make(struct config_record *r,uint32_t seq,const struct credentials *n) {
    for(unsigned i=0;i<sizeof(*r);i++) ((uint8_t *)r)[i]=0;
    r->magic=0x46573243u; r->sequence=seq; r->net=*n;
    r->hash=record_hash(r,offsetof(struct config_record,hash));
}
unsigned dhcp_reply(const uint8_t *p,unsigned n,uint8_t *q,uint8_t host) {
    if(n<240||n>576||p[0]!=1||p[1]!=1||p[2]!=6||p[3]||p[24]||p[25]||p[26]||p[27]) return 0;
    if(p[236]!=99||p[237]!=130||p[238]!=83||p[239]!=99) return 0;
    unsigned type=0, requested=0, server=0;
    for(unsigned at=240;at<n;) {
        unsigned key=p[at++]; if(key==255) break; if(key==0) continue;
        if(at>=n) return 0; unsigned len=p[at++]; if(at+len>n) return 0;
        if(key==53) { if(type||len!=1) return 0; type=p[at]; }
        if(key==50) { if(requested||len!=4) return 0; requested=1; if(p[at]!=192||p[at+1]!=168||p[at+2]!=84||p[at+3]!=host) requested=2; }
        if(key==54) { if(server||len!=4) return 0; server=1; if(p[at]!=192||p[at+1]!=168||p[at+2]!=84||p[at+3]!=1) return 0; }
        at+=len;
    }
    if(type!=1&&type!=3) return 0;
    unsigned nak=type==3&&requested==2;
    /* A renewal must refer to this client's assigned address. */
    if(type==3&&!requested&&(p[12]||p[13]||p[14]||p[15])&&
       (p[12]!=192||p[13]!=168||p[14]!=84||p[15]!=host)) nak=1;
    for(unsigned i=0;i<300;i++) q[i]=0;
    for(unsigned i=0;i<8;i++) q[i]=p[i]; q[0]=2; q[10]=0x80; q[11]=0;
    for(unsigned i=0;i<16;i++) q[28+i]=p[28+i];
    if(!nak) { q[16]=192;q[17]=168;q[18]=84;q[19]=host; }
    q[20]=192;q[21]=168;q[22]=84;q[23]=1;
    q[236]=99;q[237]=130;q[238]=83;q[239]=99;
    unsigned k=240;
    q[k++]=53;q[k++]=1;q[k++]=nak?6:type==1?2:5;
    q[k++]=54;q[k++]=4;q[k++]=192;q[k++]=168;q[k++]=84;q[k++]=1;
    if(!nak) {
        q[k++]=51;q[k++]=4;q[k++]=0;q[k++]=0;q[k++]=1;q[k++]=44;
        q[k++]=1;q[k++]=4;q[k++]=255;q[k++]=255;q[k++]=255;q[k++]=0;
        q[k++]=3;q[k++]=4;q[k++]=192;q[k++]=168;q[k++]=84;q[k++]=1;
#ifdef C208_WIFI_V93
        q[k++]=6;q[k++]=4;q[k++]=192;q[k++]=168;q[k++]=84;q[k++]=1;
#endif
    }
    q[k++]=255; return k<300?300:k;
}
