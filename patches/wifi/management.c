#include "management.h"
#ifdef C208_XIAOZHI_INPUT
#define C208_VERSION "V0097"
#elif defined(C208_WIFI_V96)
#define C208_VERSION "V0096"
#else
#define C208_VERSION "V0095"
#endif
static unsigned length(const char *p){unsigned n=0;while(p[n])n++;return n;}
static int equal(const char *p,unsigned n,const char *s){if(n!=length(s))return 0;for(unsigned i=0;i<n;i++)if(p[i]!=s[i])return 0;return 1;}
static int header(const char *p,unsigned n,const char *s){if(n!=length(s))return 0;for(unsigned i=0;i<n;i++){int c=(unsigned char)p[i];if(c>='A'&&c<='Z')c+=32;if(c!=s[i])return 0;}return 1;}
void management_ip(char out[16],uint32_t ip){unsigned at=0;for(unsigned i=0;i<4;i++){unsigned n=(ip>>(8*i))&255;if(i)out[at++]='.';if(n>=100)out[at++]=(char)('0'+n/100);if(n>=10)out[at++]=(char)('0'+n/10%10);out[at++]=(char)('0'+n%10);}out[at]=0;}
int management_allowed(const char *p,unsigned n,uint32_t ip,int write){
    if(!ip||n>1536)return 0;char host[16],origin[26];management_ip(host,ip);
    const char *prefix="http://";unsigned k=0;while(prefix[k]){origin[k]=prefix[k];k++;}for(unsigned i=0;host[i];i++)origin[k++]=host[i];origin[k]=0;
    unsigned at=0;while(at+1<n&&(p[at]!='\r'||p[at+1]!='\n'))at++;if(at+1>=n)return 0;at+=2;
    unsigned hosts=0,origins=0,actions=0;
    while(at+1<n){
        unsigned end=at;while(end+1<n&&(p[end]!='\r'||p[end+1]!='\n'))end++;if(end+1>=n)return 0;if(end==at)return hosts==1&&(!write||(origins==1&&actions==1));
        unsigned colon=at;while(colon<end&&p[colon]!=':')colon++;if(colon==at||colon==end)return 0;
        unsigned first=colon+1,last=end;while(first<last&&(p[first]==' '||p[first]=='\t'))first++;while(last>first&&(p[last-1]==' '||p[last-1]=='\t'))last--;
        if(header(p+at,colon-at,"host")){
            if(++hosts>1)return 0;unsigned len=last-first;if(len>=3&&equal(p+last-3,3,":80"))len-=3;if(!equal(p+first,len,host))return 0;
        }else if(header(p+at,colon-at,"origin")){
            if(++origins>1)return 0;unsigned len=last-first;if(len>=3&&equal(p+last-3,3,":80"))len-=3;if(!equal(p+first,len,origin))return 0;
        }else if(header(p+at,colon-at,"x-c208-action")){
#ifdef C208_XIAOZHI_INPUT
            if(++actions>1||!equal(p+first,last-first,write==2?"xiaozhi-settings":"provision"))return 0;
#else
            if(++actions>1||!equal(p+first,last-first,"provision"))return 0;
#endif
        }
        at=end+2;
    }return 0;
}
struct writer{char *p;unsigned n,cap;int ok;};
static void ch(struct writer *w,char c){if(w->n+1>=w->cap){w->ok=0;return;}w->p[w->n++]=c;}
static void text(struct writer *w,const char *s){while(*s)ch(w,*s++);}
static void quoted(struct writer *w,const char *s){ch(w,'"');for(unsigned i=0;s[i];i++){unsigned c=(unsigned char)s[i];if(c=='"'||c=='\\'){ch(w,'\\');ch(w,(char)c);}else if(c<32||c=='<'||c=='>'||c=='&'){text(w,"\\u00");ch(w,"0123456789abcdef"[c>>4]);ch(w,"0123456789abcdef"[c&15]);}else ch(w,(char)c);}ch(w,'"');}
unsigned management_json(const struct management_status *s,char *out,unsigned cap){
    if(!cap)return 0;struct writer w={out,0,cap,1};char ip[16];management_ip(ip,s->ip);
    text(&w,"HTTP/1.0 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\n\r\n{\"version\":\"" C208_VERSION "\",\"author\":\"By清风惹尘丶\",\"connected\":true,\"provisioning\":false,\"status\":");quoted(&w,s->status);text(&w,",\"ssid\":");quoted(&w,s->ssid);text(&w,",\"ip\":");quoted(&w,ip);text(&w,",\"battery\":");
    unsigned b=s->battery>100?100:s->battery;if(b>=100)ch(&w,'1');if(b>=10)ch(&w,(char)('0'+b/10%10));ch(&w,(char)('0'+b%10));ch(&w,'}');
    if(!w.ok){out[0]=0;return 0;}out[w.n]=0;return w.n;
}
