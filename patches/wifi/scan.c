#include "scan.h"
static int valid_utf8(const uint8_t *s,unsigned n) {
    for(unsigned i=0;i<n;) {
        unsigned c=s[i++],extra,min=0;
        if(c<0x80) { if(c<32||c==127)return 0;continue; }
        if(c>=0xc2&&c<=0xdf) {extra=1;c&=31;min=0x80;}
        else if(c>=0xe0&&c<=0xef) {extra=2;c&=15;min=0x800;}
        else if(c>=0xf0&&c<=0xf4) {extra=3;c&=7;min=0x10000;}
        else return 0;
        if(i+extra>n)return 0;
        while(extra--) {unsigned t=s[i++];if((t&0xc0)!=0x80)return 0;c=(c<<6)|(t&63);}
        if(c<min||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return 0;
    }return 1;
}
static uint32_t u32(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int equal(const char *a,const char *b) {for(unsigned i=0;;i++){if(a[i]!=b[i])return 0;if(!a[i])return 1;}}
int wifi_security_supported(uint32_t s) {
    return s==0||s==0x200002||s==0x200004||s==0x400002||s==0x400004||s==0x400006;
}
int wifi_scan_add(struct wifi_scan *s,const uint8_t raw[60]) {
    unsigned n=raw[16];if(!n||n>32||!valid_utf8(raw+17,n))return 0;
    struct wifi_network item={{0},0,0,0};
    for(unsigned i=0;i<n;i++)item.ssid[i]=(char)raw[17+i];
    item.security=u32(raw);item.channel=(int16_t)(raw[12]|raw[13]<<8);item.rssi=(int16_t)(raw[14]|raw[15]<<8);
    if(item.rssi>0||item.rssi<-127||item.channel<1||item.channel>196)return 0;
    unsigned at=s->count;
    for(unsigned i=0;i<s->count;i++)if(s->networks[i].security==item.security&&equal(s->networks[i].ssid,item.ssid)){
        if(s->networks[i].rssi>=item.rssi)return 1;at=i;break;
    }
    if(at==s->count) {
        if(s->count<WIFI_SCAN_MAX)s->count++;
        else {at=WIFI_SCAN_MAX-1;if(s->networks[at].rssi>=item.rssi)return 1;}
    }
    while(at&&s->networks[at-1].rssi<item.rssi){s->networks[at]=s->networks[at-1];at--;}
    s->networks[at]=item;return 1;
}
struct writer {char *out;unsigned n,cap;int ok;};
static void ch(struct writer *w,char c){if(w->n+1>=w->cap){w->ok=0;return;}w->out[w->n++]=c;}
static void text(struct writer *w,const char *s){while(*s)ch(w,*s++);}
static void number(struct writer *w,int n){char d[10];unsigned at=0;if(n<0){ch(w,'-');n=-n;}do{d[at++]=(char)('0'+n%10);n/=10;}while(n);while(at)ch(w,d[--at]);}
static void quoted(struct writer *w,const char *s){
    ch(w,'"');for(unsigned i=0;s[i];i++){
        unsigned c=(uint8_t)s[i];
        if(c=='"'||c=='\\'){ch(w,'\\');ch(w,(char)c);}
        else if(c=='<'||c=='>'||c=='&'){text(w,"\\u00");ch(w,"0123456789abcdef"[c>>4]);ch(w,"0123456789abcdef"[c&15]);}
        else ch(w,(char)c);
    }ch(w,'"');
}
unsigned wifi_scan_json(const struct wifi_scan *s,char *out,unsigned cap){
    if(!cap)return 0;struct writer w={out,0,cap,1};
    text(&w,"HTTP/1.0 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\n\r\n{\"status\":");
    quoted(&w,s->status==1?"ready":s->status==2?"failed":"unavailable");text(&w,",\"networks\":[");
    for(unsigned i=0;i<s->count&&i<WIFI_SCAN_MAX;i++){
        const struct wifi_network *n=&s->networks[i];if(i)ch(&w,',');
        text(&w,"{\"ssid\":");quoted(&w,n->ssid);text(&w,",\"rssi\":");number(&w,n->rssi);
        text(&w,",\"channel\":");number(&w,n->channel);text(&w,",\"open\":");text(&w,n->security?"false":"true");
        text(&w,",\"supported\":");text(&w,wifi_security_supported(n->security)?"true":"false");ch(&w,'}');
    }text(&w,"]}");
    if(!w.ok){out[0]=0;return 0;}out[w.n]=0;return w.n;
}
