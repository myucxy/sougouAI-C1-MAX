/* C208 V0086 experimental extension. Every firmware address is image-specific.
 * All mutable state lives in reserved BSS; no writable data is put in XIP flash.
 * lwIP PCB operations run exclusively on tcpip_thread via callback messages.
 */
#include "protocol.h"
#ifdef C208_XIAOZHI_INPUT
#include "../xiaozhi/native.h"
#endif
#ifdef C208_WIFI_V95
#include "management.h"
#include "management_page.h"
#endif
#ifdef C208_WIFI_V93
#include "portal.h"
#endif
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V95)
#include "scan.h"
#endif
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V94)
#include "setup_page.h"
#endif
#ifdef C208_WIFI_V89
#include "wifi_icon.h"
#endif
#define FN(ret,addr,...) ((ret (*)(__VA_ARGS__))((uintptr_t)(addr)|1u))
#define U8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define U32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define STATE_BASE 0x2002c99cu
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V95)
#define STATE_BYTES 12288u
#else
#define STATE_BYTES 4096u
#endif
#define AP_IP 0x0154a8c0u
#define PAGE 58
#define MSG_UPDATE 253
#define BARRIER() __asm__ volatile("dmb" ::: "memory")
typedef void (*task_fn)(void *);
typedef void (*event_fn)(void *, int);
struct pbuf { struct pbuf *next; void *payload; uint16_t total, len; };
struct sockaddr4 { uint8_t length, family; uint16_t port; uint32_t ip; uint8_t zero[8]; };
struct state {
    uint32_t magic;
    volatile unsigned command, job_done, job_op;
    volatile unsigned ap_owned, suppress_dhcp, legacy_busy, connecting;
    volatile char ap_name[3];
    void *listener, *client;
    unsigned rx_len, tx_len, tx_pos, tx_ack, client_ticks;
    volatile unsigned request_ready, handoff;
    const char *tx;
    char request[HTTP_CAP];
    struct credentials received, pending, saved;
    uint32_t sequence;
    int bank;
    unsigned have_saved, auto_connect, ap_ticks, retry_ticks, link_seen, usb_paused;
    unsigned leases;
    uint8_t mac[4][6];
    const char *status;
    void *page_list;
#ifdef C208_WIFI_V89
    unsigned netif_ready;
#ifdef C208_WIFI_V90
    void *status_label, *network_label, *ip_label;
    unsigned previous_sleep, sleep_held, trace_len;
    char trace[1536];
#ifdef C208_WIFI_V94
    void *primary_label;
    unsigned operation_busy,last_ui_tick;
#ifdef C208_WIFI_V95
    struct wifi_scan scan;
    char scan_json[WIFI_SCAN_JSON_CAP];
    char management_json[1024];
    unsigned web_ip,web_retry_tick;
#endif
#endif
#ifdef C208_WIFI_V92
    struct wifi_scan scan;
    char scan_json[WIFI_SCAN_JSON_CAP];
    void *primary_label,*saved_action_label,*forget_label,*countdown_label;
    unsigned forget_until, operation_busy, last_ui_tick, skip_scan;
#ifdef C208_WIFI_V93
    int portal_dns_fd;
#endif
#endif
#endif
#endif
};
#define S ((struct state *)(uintptr_t)STATE_BASE)
_Static_assert(sizeof(struct state)<=STATE_BYTES,"BSS reservation too small");
_Static_assert(sizeof(struct config_record)==80,"configuration layout");
void *memcpy(void *d,const void *s,size_t n) { uint8_t *q=d; const uint8_t *p=s; while(n--) *q++=*p++; return d; }
void *memset(void *d,int c,size_t n) { uint8_t *q=d; while(n--) *q++=(uint8_t)c; return d; }
void __aeabi_memcpy(void *d,const void *s,size_t n) { memcpy(d,s,n); }
void __aeabi_memcpy4(void *d,const void *s,size_t n) { memcpy(d,s,n); }
void __aeabi_memclr4(void *d,size_t n) { memset(d,0,n); }
const char wifi_page_name[]="WIFI SETUP";
static unsigned length(const char *s) { unsigned n=0;while(s[n])n++;return n; }
static void delay(unsigned ms) { FN(int,0x1806485c,int)((int)ms); }
static unsigned clock_ms(void) { return FN(unsigned,0x1806161c,void)(); }
static unsigned irq_off(void) { return FN(unsigned,0x1802b134,void)(); }
static void irq_on(unsigned v) { FN(void,0x1802b13c,unsigned)(v); }
static int usb_busy(void) { return FN(int,0x18051294,void)(); }
static int connected(void) { return U32(0x20020930)&&(U8(0x20020992)&1); }
static int ready(void) { return U32(0x20020930)&&(U8(0x20020992)&4); }
static int ap_active(void) { return U32(0x200208c8)&&(U8(0x2002092a)&1); }
static uint32_t sta_ip(void) {
    uintptr_t dev=U32(0x20020930); if(!dev) return 0;
    uintptr_t prot=U32(dev+0x120); if(!prot) return 0;
    uintptr_t netif=U32(prot+0x4c); if(!netif) return 0;
    return U32(netif+4);
}
static void notify(const char *s) {
    S->status=s; BARRIER();
    if(S->page_list) FN(int,0x1805b5e8,int,int,int)(MSG_UPDATE,0,0);
}
static void no_legacy_reconnect(void) {
    unsigned key=irq_off(); U8(0x20020993)&=(uint8_t)~1u; irq_on(key);
}
static void wlan_disconnect(void) {
    no_legacy_reconnect(); if(connected()||(U8(0x20020992)&2)) FN(int,0x1809d6c4,void)();
}
static int open_file(const char *p,int flags) { return FN(int,0x1806a0cc,const char *,int,int)(p,flags,0); }
static int close_file(int fd) { return FN(int,0x1806a120,int)(fd); }
#ifdef C208_WIFI_V90
static void trace_event(const char *event) {
    if(usb_busy()) return;
    unsigned n=length(event);
    if(S->trace_len+n+16>=sizeof(S->trace)) S->trace_len=0;
    char digits[10];unsigned count=0,tick=clock_ms();
    do { digits[count++]=(char)('0'+tick%10);tick/=10; } while(tick);
    while(count)S->trace[S->trace_len++]=digits[--count];
    S->trace[S->trace_len++]=' ';
    memcpy(S->trace+S->trace_len,event,n);S->trace_len+=n;S->trace[S->trace_len++]='\n';
    int fd=open_file("/WIFIDBG.TXT",0x602);
    if(fd>=0) {
        FN(int,0x1806a1a4,int,const void *,unsigned)(fd,S->trace,S->trace_len);
        close_file(fd);
    }
}
__attribute__((noinline)) static void trace_boot(void) {
    int fd=open_file("/WIFIDBG.TXT",0);
    if(fd>=0) {
        int n=FN(int,0x1806a160,int,void *,unsigned)(fd,S->trace,sizeof(S->trace)-32);
        close_file(fd);
        if(n>0) {
            S->trace_len=(unsigned)n;
            for(unsigned i=0;i<S->trace_len;i++) {
                unsigned c=(unsigned char)S->trace[i];
                if((c<32&&c!='\n')||c>126) { S->trace_len=0;break; }
            }
        }
    }
#ifdef C208_XIAOZHI_INPUT
    trace_event("BOOT_V0097");
#elif defined(C208_WIFI_V96)
    trace_event("BOOT_V0096");
#elif defined(C208_WIFI_V95)
    trace_event("BOOT_V0095");
#elif defined(C208_WIFI_V94)
    trace_event("BOOT_V0094");
#elif defined(C208_WIFI_V93)
    trace_event("BOOT_V0093");
#elif defined(C208_WIFI_V92)
    trace_event("BOOT_V0092");
#elif defined(C208_WIFI_V91)
    trace_event("BOOT_V0091");
#else
    trace_event("BOOT_V0090");
#endif
}
static int wireless_power(void) {
    /* Same on-wire request and completion semaphore as 0x180608b8, with
     * bounded failure instead of its RT_ASSERT after the acknowledgement wait. */
    const uint8_t request[3]={4,2,1};
    if(U32(0x20012288)>=20) { trace_event("POWER_QUEUE_FULL");return 0; }
    trace_event("POWER_BEGIN");
    FN(void,0x180600ec,const void *,unsigned)(request,sizeof(request));
    int timeout=FN(int,0x1806166c,int)(5000);
    if(FN(int,0x18061e20,void *,int)((void *)0x200122e4,timeout)!=0) {
        trace_event("POWER_TIMEOUT");return 0;
    }
    trace_event("POWER_OK");return 1;
}
#endif
#ifdef C208_WIFI_V91
static void trace_error(const char *name,unsigned code) {
    char line[40];unsigned n=length(name);
    if(n>30)n=30;
    memcpy(line,name,n);line[n++]='=';
    for(unsigned i=0;i<8;i++)line[n++]=((code>>(28-4*i))&15)<10?
        (char)('0'+((code>>(28-4*i))&15)):(char)('a'+((code>>(28-4*i))&15)-10);
    line[n]=0;trace_event(line);
}
int wifi_ap_bus_probe(void) {
    unsigned owned=S->magic==0x38303243&&S->ap_owned;
    if(owned)trace_event("AP_BUS_BEGIN");
    int result=FN(int,0x180d9134,void)();
    if(owned) {
        trace_event(result?"AP_BUS_FAIL":"AP_BUS_OK");
        if(result)trace_error("AP_BUS_ERROR",(unsigned)result);
    }
    return result;
}
int wifi_ap_driver_start(const char *ssid,const char *password,int channel) {
    if(S->magic!=0x38303243||!S->ap_owned)
        return FN(int,0x18085e20,const char *,const char *,int)(ssid,password,channel);
    /* The original device handler always passes its key buffer, even empty.
     * The old bridge chose WPA from pointer non-nullness and unconditionally
     * strlen'ed NULL. Select OPEN from zero key length and pass length zero. */
    trace_event("AP_DRIVER_BEGIN");
    unsigned ssid_len=ssid?length(ssid):0, key_len=password?length(password):0;
    if(!ssid_len||ssid_len>32||key_len>32) { trace_event("AP_ARGUMENT_ERROR");return -1; }
    uint8_t packed_ssid[33];memset(packed_ssid,0,sizeof(packed_ssid));
    packed_ssid[0]=(uint8_t)ssid_len;memcpy(packed_ssid+1,ssid,ssid_len);
    const char *key=key_len?password:0;
    unsigned security=key_len?0x00400004u:0;
    U8(0x20025094)=2;
    trace_event(key_len?"AP_KEY_PROTECTED":"AP_KEY_OPEN");
    trace_event("AP_INIT_BEGIN");
    int result=FN(int,0x180d8b9c,const void *,unsigned,const char *,unsigned,unsigned)(
        packed_ssid,security,key,key_len,(unsigned)channel&255);
    if(result) { trace_error("AP_INIT_ERROR",(unsigned)result);return -1; }
    trace_event("AP_INIT_OK");trace_event("AP_RADIO_BEGIN");
    result=FN(int,0x180d8f50,void)();
    if(result) { trace_error("AP_RADIO_ERROR",(unsigned)result);return -1; }
    trace_event("AP_RADIO_OK");return 0;
}
#endif
static const char *config_path(unsigned bank) { return bank?"/C208WIF1.CFG":"/C208WIF0.CFG"; }
static int read_record(unsigned bank,struct config_record *r) {
    int fd=open_file(config_path(bank),0); if(fd<0) return 0;
    int n=FN(int,0x1806a160,int,void *,unsigned)(fd,r,sizeof(*r));
    uint8_t extra; int tail=FN(int,0x1806a160,int,void *,unsigned)(fd,&extra,1);
    int closed=close_file(fd); return n==(int)sizeof(*r)&&tail==0&&closed==0&&record_valid(r);
}
__attribute__((noinline)) static int load_config(void) {
    struct config_record a,b; int av=read_record(0,&a), bv=read_record(1,&b);
    if(!av&&!bv) return 0;
    unsigned bank=bv&&(!av||(int32_t)(b.sequence-a.sequence)>0);
    struct config_record *r=bank?&b:&a;
    S->saved=r->net; S->sequence=r->sequence; S->bank=(int)bank; S->have_saved=r->net.ssid[0]!=0;
    memset(&a,0,sizeof(a)); memset(&b,0,sizeof(b)); return 1;
}
static int store_config(const struct credentials *net) {
    if(usb_busy()) return 0;
    struct config_record record, verify; record_make(&record,S->sequence+1,net);
    unsigned bank=(unsigned)(S->bank^1);
    int fd=open_file(config_path(bank),0x602); if(fd<0) return 0;
    int n=FN(int,0x1806a1a4,int,const void *,unsigned)(fd,&record,sizeof(record));
    int c=close_file(fd); int ok=n==(int)sizeof(record)&&c==0&&read_record(bank,&verify);
    if(ok) { for(unsigned i=0;i<sizeof(record);i++) if(((uint8_t *)&record)[i]!=((uint8_t *)&verify)[i]) { ok=0;break; } }
    if(ok) { S->sequence=record.sequence;S->bank=(int)bank;S->saved=*net;S->have_saved=net->ssid[0]!=0; }
    memset(&record,0,sizeof(record));memset(&verify,0,sizeof(verify));return ok;
}
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V94)
static const char html[]=
 "HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'none'; connect-src 'self'; script-src 'unsafe-inline'; form-action 'self'; style-src 'unsafe-inline'; frame-ancestors 'none'\r\n\r\n"
 C208_SETUP_HTML;
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V95)
static const char rescan_accepted[]="HTTP/1.0 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n已接收重新扫描请求，请稍后重新连接 C208-Setup。";
#endif
#else
static const char html[]=
 "HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'none'; form-action 'self'; style-src 'unsafe-inline'; frame-ancestors 'none'\r\n\r\n"
 "<!doctype html><html lang=zh-CN><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>C208 Wi-Fi 配网</title>"
 "<style>body{max-width:30em;margin:2em auto;padding:0 1em;font:18px system-ui}input,button{box-sizing:border-box;width:100%;padding:12px;margin:8px 0 20px;font:inherit}small{display:block;line-height:1.6}</style>"
 "<h1>C208 Wi-Fi 配网</h1><p>清风惹尘丶</p><p>输入路由器信息。提交后热点会关闭，请在设备屏幕查看连接结果。</p>"
 "<form method=post action=/configure enctype=application/x-www-form-urlencoded autocomplete=off>"
 "<label>网络名称 SSID<input name=ssid required maxlength=32></label>"
 "<label>Wi-Fi 密码<input type=password name=password maxlength=32 autocomplete=new-password></label>"
 "<small>支持开放网络和 WPA/WPA2 个人网络。密码为空，或为 8–32 个 ASCII 字符。SSID 最多 32 字节；中文按 UTF-8 字节计算。首版请使用 2.4GHz 网络；不支持企业认证或仅 WPA3 的网络。</small>"
 "<button type=submit>保存并连接</button></form><p>热点仅供临时配网，5 分钟后自动关闭。网络信息只在设备验证连接后保存。</p></html>";
#endif
static const char accepted[]="HTTP/1.0 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n已收到网络信息，正在关闭热点并连接路由器。请查看设备屏幕；连接失败会重新打开配网热点。";
static const char bad_request[]="HTTP/1.0 400 Bad Request\r\nContent-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n请求无效。请使用配网页面；SSID 为 1–32 字节，密码为空或 8–32 个 ASCII 字符。";
static const char busy_reply[]="HTTP/1.0 503 Service Unavailable\r\nConnection: close\r\n\r\nDevice busy. Try again.\n";
#ifdef C208_WIFI_V95
static const char management_html[]=
 "HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'none'; connect-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; frame-ancestors 'none'\r\n\r\n" C208_MANAGEMENT_HTML;
static const char management_accepted[]="HTTP/1.0 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n已接收，请重新连接设备配网热点。";
#endif
static void tcp_recv_set(void *p,void *fn) { FN(void,0x18073da8,void *,void *)(p,fn); }
static void tcp_sent_set(void *p,void *fn) { FN(void,0x18073db0,void *,void *)(p,fn); }
static void tcp_err_set(void *p,void *fn) { FN(void,0x18073db4,void *,void *)(p,fn); }
static int close_client(void) {
    void *p=S->client; if(!p) return 1;
    /* On success tcp_close owns the PCB until final ACK/timeout; remove callbacks first. */
    tcp_recv_set(p,0); tcp_sent_set(p,0); tcp_err_set(p,0);
    int r=FN(int,0x18073fdc,void *)(p);
    if(r!=0) return 0; /* worker retries; PCB remains valid with callbacks disabled */
    S->client=0;S->request_ready=0;S->rx_len=0;
    memset(S->request,0,sizeof(S->request));memset(&S->received,0,sizeof(S->received));return 1;
}
static int raw_recv(void *arg,void *pcb,struct pbuf *p,int err) {
    (void)arg; (void)err;
    if(!p) { if(!S->request_ready&&!S->tx) close_client(); return 0; }
    unsigned total=p->total;
    if(pcb!=S->client||S->request_ready||S->tx||total>HTTP_CAP-S->rx_len) {
        if(pcb==S->client&&!S->tx) { S->request_ready=3; }
    } else {
        for(struct pbuf *a=p;a;a=a->next) {
            if(a->len>HTTP_CAP-S->rx_len) { S->request_ready=3;break; }
            memcpy(S->request+S->rx_len,a->payload,a->len); S->rx_len+=a->len;
        }
        if(!S->request_ready) {
            int r=http_request(S->request,S->rx_len,&S->received);
#ifdef C208_WIFI_V95
            if(r>0) {
                if(S->ap_owned) { if(r==7||r==8
#ifdef C208_XIAOZHI_INPUT
                    ||r==9||r==10
#endif
                )r=-1; }
#ifdef C208_XIAOZHI_INPUT
                else if(!management_allowed(S->request,S->rx_len,S->web_ip,r==10?2:r==8)||
                        (r!=1&&r!=7&&r!=8&&r!=9&&r!=10))r=-1;
                if(r==10){int queued=xz_native_web_queue(S->request,S->rx_len);if(queued!=1)r=queued<0?11:-1;}
#else
                else if(!management_allowed(S->request,S->rx_len,S->web_ip,r==8)||
                        (r!=1&&r!=7&&r!=8))r=-1;
#endif
            }
#endif
            if(r) { S->request_ready=r<0?3:(unsigned)r; BARRIER(); }
        }
    }
    FN(void,0x1807377c,void *,unsigned)(pcb,total);
    FN(unsigned,0x180e0be8,struct pbuf *)(p); return 0;
}
static int raw_sent(void *arg,void *pcb,unsigned n) { (void)arg; if(pcb==S->client) S->tx_ack+=n; return 0; }
static void raw_error(void *arg,int err) {
    (void)arg;(void)err; S->client=0;S->request_ready=0;
    memset(S->request,0,sizeof(S->request));memset(&S->received,0,sizeof(S->received));
}
static int raw_accept(void *arg,void *pcb,int err) {
    (void)arg; if(err||S->client||
#ifdef C208_WIFI_V95
        (!S->ap_owned&&(!ready()||!sta_ip()||S->web_ip!=sta_ip()))||
#else
        !S->ap_owned||
#endif
        S->handoff) return -1;
    S->client=pcb;S->rx_len=0;S->tx=0;S->tx_len=0;S->tx_pos=0;S->tx_ack=0;S->client_ticks=clock_ms();S->request_ready=0;
    FN(void,0x18073da4,void *,void *)(pcb,0);
    tcp_recv_set(pcb,(void *)raw_recv);tcp_sent_set(pcb,(void *)raw_sent);tcp_err_set(pcb,(void *)raw_error); return 0;
}
static void raw_response(const char *text) { S->tx=text;S->tx_len=length(text);S->tx_pos=0;S->tx_ack=0; }
/* Job 1 opens, 2 closes, 3 pumps TCP. Only this callback mutates PCB lists. */
static void tcp_job(void *unused) {
    (void)unused;
    unsigned op=S->job_op;
    if(op==1&&!S->listener) {
        int conflict=0;void *p=(void *)(uintptr_t)U32(0x20023344);
        for(unsigned i=0;p&&i<32;i++) {
            if(*(uint16_t *)((uint8_t *)p+0x1a)==80) { conflict=1;break; }
            p=*(void **)((uint8_t *)p+0x0c);
            if(i==31&&p) conflict=1;
        }
        if(!conflict) {
            uint8_t *l=FN(void *,0x1807318c,unsigned)(3);
            if(l) {
                /* Compiler packs tcp_state to one byte; LISTEN pool item is 28 bytes.
                 * Memp sizes and tcp_accept/common PCB fields verified in this image. */
                memset(l,0,28);
#ifdef C208_WIFI_V95
                *(uint32_t *)l=S->web_ip;
#else
                *(uint32_t *)l=AP_IP;
#endif
                l[10]=255;
                *(void **)(l+0x14)=(void *)raw_accept;l[0x18]=1;l[0x19]=64;*(uint16_t *)(l+0x1a)=80;
                *(uint32_t *)(l+0xc)=U32(0x20023344);U32(0x20023344)=(uint32_t)(uintptr_t)l;S->listener=l;
            }
        }
#ifdef C208_WIFI_V89
    } else if(op==4) {
        /* The original DHCP starter stopped client DHCP, set the address, then
         * called netif_set_up. Suppressing it must retain all three operations. */
        S->netif_ready=0;
        uintptr_t netif=U32(0x20023320);
        for(unsigned i=0;netif&&i<16;i++,netif=U32(netif)) {
            if(U8(netif+0x32)!=(uint8_t)S->ap_name[0]||U8(netif+0x33)!=(uint8_t)S->ap_name[1]) continue;
            uint32_t ip=AP_IP, mask=0x00ffffffu, gw=0;
            FN(void,0x18072a28,void *)((void *)netif);
            FN(void,0x18073264,void *,const void *,const void *,const void *)((void *)netif,&ip,&mask,&gw);
            FN(void,0x1807331c,void *)((void *)netif);
            S->netif_ready=U32(netif+4)==AP_IP&&(U8(netif+0x31)&1);
            break;
        }
#endif
    } else if(op==2) {
        if(S->listener) { FN(int,0x18073fdc,void *)(S->listener);S->listener=0; }
        close_client();
    } else if(op==3&&S->client) {
        if(!S->tx&&S->request_ready) {
            if(S->request_ready==1) raw_response(
#ifdef C208_WIFI_V95
                S->ap_owned?html:management_html
#else
                html
#endif
            );
#ifdef C208_WIFI_V95
#ifdef C208_XIAOZHI_INPUT
            else if(S->request_ready==9&&!S->ap_owned)raw_response(xz_native_web_status());
            else if(S->request_ready==10&&!S->ap_owned){
                const char *reply=xz_native_web_reply();if(!reply){BARRIER();S->job_done=1;return;}raw_response(reply);
            }
#endif
            else if(S->request_ready==7&&!S->ap_owned) {
                char ssid[33];unsigned n=U8(0x20020944);if(n>32)n=32;
                memcpy(ssid,(const void *)0x20020945,n);ssid[n]=0;
                struct management_status status={S->status?S->status:"已连接路由器",n?ssid:S->saved.ssid,sta_ip(),FN(unsigned,0x18051320,void)()};
                if(management_json(&status,S->management_json,sizeof(S->management_json)))raw_response(S->management_json);
                else raw_response(bad_request);
            } else if(S->request_ready==8&&!S->ap_owned&&!S->handoff) {
                S->handoff=3;raw_response(management_accepted);
            }
#endif
#ifdef C208_WIFI_V93
            else if(S->request_ready==6) raw_response("HTTP/1.1 302 Found\r\nLocation: http://192.168.84.1/\r\nCache-Control: no-store\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
#endif
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V95)
            else if(S->request_ready==4)raw_response(S->scan_json);
            else if(S->request_ready==5&&!S->handoff) {
                S->handoff=2;raw_response(rescan_accepted);
            }
#endif
            else if(S->request_ready==2&&!S->handoff) {
                S->pending=S->received;S->handoff=1;raw_response(accepted);
            } else raw_response(S->request_ready==3?bad_request:busy_reply);
            memset(S->request,0,sizeof(S->request));memset(&S->received,0,sizeof(S->received));
        }
        if(S->tx) {
            while(S->tx_pos<S->tx_len) {
                unsigned count=S->tx_len-S->tx_pos;if(count>512)count=512;
                int r=FN(int,0x04009a40,void *,const void *,unsigned,unsigned)(S->client,S->tx+S->tx_pos,count,1);
                if(r)break;S->tx_pos+=count;
            }
            FN(int,0x180e0bd8,void *)(S->client);
            if(S->tx_ack>=S->tx_len&&S->tx_pos==S->tx_len) close_client();
        }
        if(S->client&&(unsigned)(clock_ms()-S->client_ticks)>=10000) close_client();
    }
    BARRIER();S->job_done=1;
}
static int network_job(unsigned op) {
    S->job_op=op;S->job_done=0;BARRIER();
    if(FN(int,0x180711d0,task_fn,void *,int)(tcp_job,0,0)!=0) return 0;
    while(!S->job_done) delay(10);BARRIER();return 1;
}
/* Replace only the AP protocol's original DHCP start call; keep legacy behavior
 * when custom provisioning does not own the AP. Called by WLAN workqueue. */
int wifi_dhcp_hook(const char *name) {
    if(S->magic==0x38303243&&S->suppress_dhcp) {
        S->ap_name[0]=name[0];S->ap_name[1]=name[1];S->ap_name[2]=0;BARRIER();return 0;
    }
    return FN(int,0x180778e8,const char *)(name);
}
/* A legacy DHCP thread can outlive quick transfer. Do not let a stray ACK
 * enter the old application's connection handler during custom provisioning. */
int wifi_legacy_dhcp_notify(unsigned event,unsigned ip) {
    if(S->magic==0x38303243&&(S->suppress_dhcp||S->ap_owned)) return 0;
    return FN(int,0x180503cc,unsigned,unsigned)(event,ip);
}
#ifdef C208_WIFI_V93
static void stop_portal_dns(void) {
    if(S->portal_dns_fd>=0) FN(int,0x18070944,int)(S->portal_dns_fd);
    S->portal_dns_fd=-1;
}
static void start_portal_dns(void) {
    stop_portal_dns();
    int fd=FN(int,0x18070e04,int,int,int)(2,2,0);
    if(fd<0) { trace_event("DNS_NO_SOCKET");return; }
    struct sockaddr4 addr={16,2,0x3500,AP_IP,{0}};
    if(FN(int,0x1807107c,int,int,int)(fd,4,0x4000)!=0||
       FN(int,0x180708b0,int,const void *,unsigned)(fd,&addr,16)!=0) {
        FN(int,0x18070944,int)(fd);trace_event("DNS_BIND_FAILED");return;
    }
    S->portal_dns_fd=fd;trace_event("DNS_OK");
}
__attribute__((noinline)) static void serve_portal_dns(void) {
    if(S->portal_dns_fd<0||!S->ap_owned)return;
    uint8_t query[512],reply[512];
    for(unsigned i=0;i<4;i++) {
        struct sockaddr4 from={0};unsigned flen=sizeof(from);
        int n=FN(int,0x18070a58,int,void *,unsigned,int,void *,unsigned *)(S->portal_dns_fd,query,sizeof(query),0,&from,&flen);
        if(n<0)break;
        if(n>512||flen!=16||from.family!=2||!from.port||!portal_client_ip(from.ip))continue;
        unsigned size=portal_dns_reply(query,(unsigned)n,reply,sizeof(reply));
        if(size)FN(int,0x18070c68,int,const void *,unsigned,int,const void *,unsigned)(S->portal_dns_fd,reply,size,0,&from,16);
    }
}
#endif
static int stop_ap(int dhcp_fd) {
#ifdef C208_WIFI_V93
    stop_portal_dns();
#endif
    if(dhcp_fd>=0) FN(int,0x18070944,int)(dhcp_fd);
    if(S->ap_owned) {
        for(unsigned i=0;i<50;i++) { if(network_job(2)&&!S->client)break; delay(20); }
        if(S->client) { notify("网页连接正在关闭");return -2; }
#ifdef C208_WIFI_V91
        if(ap_active()&&FN(int,0x1809dcac,void)()!=0) { notify("热点正在关闭，请稍候");return -2; }
#else
        if(FN(int,0x1809dcac,void)()!=0) { notify("热点正在关闭，请稍候");return -2; }
#endif
        S->ap_owned=0;
        /* Leave the interface registered, as the original quick-transfer path does. */
    }
    S->suppress_dhcp=0;S->ap_name[0]=0;S->handoff=0;S->tx=0;return -1;
}
#ifdef C208_WIFI_V95
__attribute__((noinline)) static int select_station_mode(void) {
    int result=FN(int,0x1809d424,const char *,int)("wlan0",1);
    if(result)return result;
    uintptr_t dev=U32(0x20020930);
    if(!dev)return -1;
    /* mgnt may return early for an existing STA while AP changed the shared mode. */
    FN(int,0x18062278,void *,int)((void *)0x20020a20,-1);
    result=FN(int,0x180855dc,void *,int)((void *)dev,1);
    FN(int,0x1806245c,void *)((void *)0x20020a20);
    return result;
}
#endif
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V95)
static void scan_before_ap(void) {
    memset(&S->scan,0,sizeof(S->scan));S->scan.status=2;
    notify("正在扫描附近网络");trace_event("SCAN_MODE_BEGIN");
#ifdef C208_WIFI_V95
    int mode_result=select_station_mode();
#else
    int mode_result=FN(int,0x1809d424,const char *,int)("wlan0",1);
#endif
    if(mode_result==0) {
        no_legacy_reconnect();FN(void,0x1809deb0,void)();trace_event("SCAN_BEGIN");
        uintptr_t result=(uintptr_t)FN(void *,0x1809de8c,void)();
        if(result) {
            FN(int,0x18062278,void *,int)((void *)0x20020a78,-1);
            unsigned count=U32(result);uintptr_t items=U32(result+4);
            if(count<=256&&(!count||items)) {
                for(unsigned i=0;i<count;i++)wifi_scan_add(&S->scan,(const uint8_t *)(items+60*i));
                S->scan.status=1;
            }
            FN(int,0x1806245c,void *)((void *)0x20020a78);
        }
        FN(void,0x1809deb0,void)();
        trace_event(S->scan.status==1?"SCAN_OK":"SCAN_FAIL");
#ifdef C208_WIFI_V95
        /* mode NONE calls wiced_deinit and tears down the shared STA/AP core. */
        trace_event("SCAN_KEEP_STA");
#else
        /* Historical V0092/V0093 behavior kept for reproducible old packages. */
        FN(int,0x1809d424,const char *,int)("wlan0",0);
#endif
    } else trace_event("SCAN_MODE_FAIL");
    if(!wifi_scan_json(&S->scan,S->scan_json,sizeof(S->scan_json))) {
        S->scan.count=0;S->scan.status=2;wifi_scan_json(&S->scan,S->scan_json,sizeof(S->scan_json));
    }
    notify("正在开启手机配网");
}
#endif
static int start_ap(void) {
    if(usb_busy()) { notify("请先退出 USB 模式");return -1; }
    if(S->legacy_busy||(ap_active()&&!S->ap_owned)) { notify("原快传正在使用热点");return -1; }
#ifdef C208_WIFI_V95
    if(!network_job(2)||S->client) { notify("网页连接正在关闭");return -1; }
    S->web_ip=AP_IP;S->web_retry_tick=0;
#endif
    S->auto_connect=0;wlan_disconnect();S->suppress_dhcp=1;S->ap_name[0]=0;S->leases=0;
    memset(S->mac,0,sizeof(S->mac));
#ifdef C208_WIFI_V89
    notify("正在启动配网热点");
#endif
#ifdef C208_WIFI_V90
    trace_event("START_AP");
    if(!wireless_power()) { S->suppress_dhcp=0;notify("无线供电未应答");return -1; }
#ifdef C208_WIFI_V92
    if(S->skip_scan) {
        memset(&S->scan,0,sizeof(S->scan));wifi_scan_json(&S->scan,S->scan_json,sizeof(S->scan_json));
        trace_event("SCAN_SKIPPED");
    } else scan_before_ap();
    if(S->command||usb_busy()) { S->suppress_dhcp=0;notify("配网已取消");return -1; }
#endif
#ifdef C208_WIFI_V95
    scan_before_ap();
#endif
#ifdef C208_WIFI_V94
    if(S->command||usb_busy()) { S->suppress_dhcp=0;notify("配网已取消");return -1; }
#endif
    trace_event("MODE_BEGIN");
#else
    FN(int,0x180608b8,int,int)(2,1);
#endif
    if(FN(int,0x1809d424,const char *,int)("wlan1",2)!=0) {
#ifdef C208_WIFI_V90
        trace_event("MODE_FAIL");
#endif
        S->suppress_dhcp=0;notify("AP 接口初始化失败");return -1;
    }
#ifdef C208_WIFI_V90
    trace_event("MODE_OK");trace_event("AP_BEGIN");
#endif
    S->ap_owned=1;
    if(FN(int,0x1809db74,const char *,const char *,int)("C208-Setup",0,6)!=0) {
#ifdef C208_WIFI_V90
        trace_event("AP_FAIL");
#endif
        stop_ap(-1);notify("热点开启失败");return -1;
    }
#ifdef C208_WIFI_V90
    trace_event("AP_OK");
#endif
    for(unsigned i=0;i<100&&!S->ap_name[0];i++)delay(20);
    if(!S->ap_name[0]) {
#ifdef C208_WIFI_V90
        trace_event("AP_EVENT_TIMEOUT");
#endif
        stop_ap(-1);notify("未收到 AP 接口事件");return -1;
    }
#ifdef C208_WIFI_V90
    trace_event("NETIF_BEGIN");
#endif
#ifdef C208_WIFI_V89
    if(!network_job(4)||!S->netif_ready) {
#ifdef C208_WIFI_V90
        trace_event("NETIF_FAIL");
#endif
        stop_ap(-1);notify("AP 网络接口未就绪");return -1;
    }
#else
    char name[3]={S->ap_name[0],S->ap_name[1],0};
    FN(void,0x1807737c,const char *,const char *,const char *,const char *)(name,"192.168.84.1","0.0.0.0","255.255.255.0");
#endif
#ifdef C208_WIFI_V90
    trace_event("NETIF_OK");trace_event("SERVICES_BEGIN");
#endif
    int fd=FN(int,0x18070e04,int,int,int)(2,2,0);
    int yes=1;
    struct sockaddr4 addr={16,2,0x4300,0,{0}};
    if(fd<0) { stop_ap(-1);notify("DHCP 内存不足");return -1; }
    if(FN(int,0x18070f1c,int,int,int,const void *,unsigned)(fd,0xfff,0x20,&yes,4)!=0||
       FN(int,0x1807107c,int,int,int)(fd,4,0x4000)!=0||!network_job(1)||!S->listener) {
        stop_ap(fd);notify("配网服务启动失败");return -1;
    }
    if(FN(int,0x180708b0,int,const void *,unsigned)(fd,&addr,16)!=0) {
        stop_ap(fd);notify("请重启后开启配网");return -1;
    }
#ifdef C208_WIFI_V93
    start_portal_dns();
#endif
#ifdef C208_WIFI_V90
    trace_event("SERVICES_OK");
#endif
    S->ap_ticks=clock_ms();S->handoff=0;notify("热点已开启 / 5 分钟");return fd;
}
static void serve_dhcp(int fd) {
    uint8_t request[576],reply[300]; struct sockaddr4 from; unsigned flen=16;
    int n=FN(int,0x18070a58,int,void *,unsigned,int,void *,unsigned *)(fd,request,sizeof(request),0,&from,&flen);
    if(n<240||n>576)return;
    /* Four independent leases, fixed for this temporary AP session. */
    unsigned slot=0;for(;slot<S->leases;slot++) {
        unsigned k=0;while(k<6&&S->mac[slot][k]==request[28+k])k++;if(k==6)break;
    }
    if(slot>=4)return;
    unsigned size=dhcp_reply(request,(unsigned)n,reply,(uint8_t)(100+slot));if(!size)return;
    if(slot==S->leases) { memcpy(S->mac[slot],request+28,6);S->leases++; }
    struct sockaddr4 to={16,2,0x4400,0xff54a8c0u,{0}};
    FN(int,0x18070c68,int,const void *,unsigned,int,const void *,unsigned)(fd,reply,size,0,&to,16);
}
static int command_pending(void) { return S->command!=0; }
static int do_connect_network(const struct credentials *net,int save) {
    if(!credentials_valid(net)) { notify("没有可用的网络配置");return 0; }
    if(usb_busy()) { notify("请先退出 USB 模式");return 0; }
    if(S->legacy_busy||(ap_active()&&!S->ap_owned)) { notify("原快传正在使用热点");return 0; }
    notify("正在连接路由器");wlan_disconnect();
#ifdef C208_WIFI_V90
    if(!wireless_power()) { notify("无线供电未应答");return 0; }
#else
    FN(int,0x180608b8,int,int)(2,1);
#endif
#ifdef C208_WIFI_V95
    trace_event("STA_MODE_BEGIN");
    int mode_result=select_station_mode();
    if(mode_result) { trace_error("STA_MODE_ERROR",(unsigned)mode_result);notify("STA 接口初始化失败");return 0; }
    trace_event("STA_MODE_OK");
#else
    if(FN(int,0x1809d424,const char *,int)("wlan0",1)!=0) { notify("STA 接口初始化失败");return 0; }
#endif
    no_legacy_reconnect();
    uint8_t info[60];memset(info,0,sizeof(info));
    *(uint32_t *)info=net->password[0]?0x00400006u:0;
    *(uint16_t *)(info+12)=0xffff;info[16]=(uint8_t)length(net->ssid);memcpy(info+17,net->ssid,info[16]);
#ifdef C208_WIFI_V95
    trace_event("STA_JOIN_BEGIN");
#endif
    int result=FN(int,0x1809d7bc,const void *,const char *)(info,net->password[0]?net->password:0);
#ifdef C208_WIFI_V95
    trace_error("STA_JOIN_RESULT",(unsigned)result);
    unsigned saw_link=0;
#endif
    if(result==0) {
        for(unsigned i=0;i<600;i++) {
            if(command_pending()||usb_busy()) {
#ifdef C208_WIFI_V95
                trace_event("STA_CANCEL");
#endif
                wlan_disconnect();notify("连接已取消");return 0;
            }
#ifdef C208_WIFI_V95
            if(connected()&&!saw_link) { saw_link=1;trace_event("STA_LINK_UP"); }
#endif
            if(ready()&&sta_ip()) {
#ifdef C208_WIFI_V95
                trace_event("STA_IP_OK");
#endif
                S->auto_connect=1;S->link_seen=1;S->retry_ticks=clock_ms();
                if(save&&!store_config(net)) notify("已连接，配置保存失败");
                else notify("已连接路由器");return 1;
            }
            delay(100);
        }
    }
#ifdef C208_WIFI_V95
    trace_error("STA_FLAGS",U8(0x20020992));
    trace_event(connected()?"STA_DHCP_TIMEOUT":"STA_AUTH_OR_LINK_FAIL");
    unsigned had_link=connected();wlan_disconnect();notify(had_link?"获取 IP 失败，请重试":"连接失败，请检查密码");return 0;
#else
    wlan_disconnect();notify("连接失败，请重新配网");return 0;
#endif
}
static int connect_network(const struct credentials *net,int save) {
    S->connecting=1;int r=do_connect_network(net,save);S->connecting=0;return r;
}
static unsigned take_command(void) { unsigned key=irq_off(),c=S->command;S->command=0;irq_on(key);return c; }
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V94)
__attribute__((noinline)) static int complete_handoff(int fd) {
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V95)
    unsigned rescan=S->handoff==2;S->operation_busy=1;
#ifdef C208_WIFI_V92
    if(rescan)S->skip_scan=0;
#endif
#else
    S->operation_busy=1;
#endif
    fd=stop_ap(fd);
    if(fd==-2)return fd; /* retain handoff and retry after AP stop completes */
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V95)
    if(rescan) {
        memset(&S->pending,0,sizeof(S->pending));
        if(!command_pending()&&!usb_busy())fd=start_ap();
    } else
#endif
    {
        int ok=connect_network(&S->pending,1);memset(&S->pending,0,sizeof(S->pending));
        if(!ok&&!command_pending()&&!usb_busy()) {fd=start_ap();if(fd>=0)notify("连接失败 / 热点已重开");}
    }
    S->operation_busy=0;return fd;
}
#endif
#ifdef C208_WIFI_V95
__attribute__((noinline)) static int station_web_service(int fd) {
    if(usb_busy()) {
        if(!S->ap_owned&&S->listener)network_job(2);
        return fd;
    }
    if(S->handoff==3&&!S->client) {
        if(network_job(2)&&!S->client) { S->web_ip=0;S->handoff=0;S->command=1; }
        return fd;
    }
    if(!ready()||!sta_ip()) {
        if(!S->ap_owned&&S->listener)network_job(2);
        return fd;
    }
    if(S->ap_owned) {
        if(S->handoff||S->connecting)return fd;
        fd=stop_ap(fd);
        if(fd==-2)return fd;
        trace_event("AP_CLOSED_FOR_STA");notify("已连接路由器 / 配网已关闭");
    }
    uint32_t ip=sta_ip();
    if(S->listener&&U32((uintptr_t)S->listener)!=ip) {
        if(!network_job(2)||S->client)return fd;
    }
    if(S->web_ip!=ip)S->web_retry_tick=0;
    S->web_ip=ip;
    if(!S->listener) {
        if(S->web_retry_tick&&(int32_t)(S->web_retry_tick-clock_ms())>0)return fd;
        if(!network_job(1)||!S->listener) { S->web_retry_tick=clock_ms()+5000;notify("已连接 / 网页暂不可用");return fd; }
        S->web_retry_tick=0;
        trace_event("STA_WEB_READY");
    }
    network_job(3);return fd;
}
#endif
static void worker(void *unused) {
    (void)unused;int fd=-1;delay(5000);
    while(usb_busy())delay(500);
#ifdef C208_WIFI_V90
    trace_boot();
#endif
#ifdef C208_WIFI_V96
    extern int trim96_language(unsigned);trim96_language(0);
#endif
    load_config();notify(S->have_saved?"已读取保存的网络":"未配置网络");
#if !defined(C208_WIFI_V90) || defined(C208_WIFI_V95)
    if(S->have_saved&&!command_pending()) {
#ifdef C208_WIFI_V95
        S->auto_connect=1;S->retry_ticks=clock_ms();
#endif
        S->auto_connect=1;connect_network(&S->saved,0);
    }
#endif
    for(;;) {
        unsigned cmd=take_command();
        if(cmd) {
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V94)
            S->operation_busy=1;
#endif
#ifdef C208_WIFI_V92
            if(cmd==1||cmd==8) { S->skip_scan=cmd==8;if(fd==-1&&!S->ap_owned)fd=start_ap(); }
#else
            if(cmd==1) { if(fd==-1&&!S->ap_owned) fd=start_ap(); }
#endif
            else {
                if(S->ap_owned)fd=stop_ap(fd);
                if(fd==-2) { delay(100);S->command=cmd;continue; }
                if(cmd==2) { if(S->have_saved)connect_network(&S->saved,0);else notify("请先开启 AP 配网"); }
                if(cmd==3) { S->auto_connect=0;wlan_disconnect();notify("Wi-Fi 已断开"); }
                if(cmd==4) {
                    S->auto_connect=0;wlan_disconnect();struct credentials empty={{0},{0}};
                    if(store_config(&empty)) { store_config(&empty);notify("保存的网络已清除"); }
                    else notify("清除失败 / 请退出 USB");
                }
            }
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V94)
            S->operation_busy=0;
#endif
        }
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V94)
        if(S->page_list&&(unsigned)(clock_ms()-S->last_ui_tick)>=1000) {
            S->last_ui_tick=clock_ms();FN(int,0x1805b5e8,int,int,int)(MSG_UPDATE,0,0);
        }
#endif
        if(fd>=0) {
            if(usb_busy()||(unsigned)(clock_ms()-S->ap_ticks)>=300000) { fd=stop_ap(fd);notify("配网热点已关闭"); }
            else {
                serve_dhcp(fd);
#ifdef C208_WIFI_V93
                serve_portal_dns();
#endif
                network_job(3);
                if(S->handoff&&!S->client) {
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V94)
                    fd=complete_handoff(fd);
#else
                    fd=stop_ap(fd);
                    if(fd==-2)continue;
                    int ok=connect_network(&S->pending,1);memset(&S->pending,0,sizeof(S->pending));
                    if(!ok&&!command_pending()&&!usb_busy()) { fd=start_ap();if(fd>=0)notify("连接失败 / 热点已重开"); }
#endif
                }
            }
        } else if(S->ap_owned) {
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V94)
            if(S->handoff&&!S->client)fd=complete_handoff(-1);
            else
#endif
                fd=stop_ap(-1);
        }
        else if(S->auto_connect&&S->have_saved) {
            if(usb_busy()) {
                if(!S->usb_paused) { S->usb_paused=1;wlan_disconnect();notify("USB 模式 / Wi-Fi 暂停"); }
            } else {
                if(S->usb_paused) { S->usb_paused=0;S->retry_ticks=clock_ms()-60000; }
                if(!ready()||!sta_ip()) {
                    if(S->link_seen) { S->link_seen=0;S->retry_ticks=clock_ms();notify("连接已断开 / 等待重连"); }
                    if((unsigned)(clock_ms()-S->retry_ticks)>=60000&&!ap_active()) { S->retry_ticks=clock_ms();connect_network(&S->saved,0); }
                }
            }
        }
#ifdef C208_WIFI_V95
#ifdef C208_XIAOZHI_INPUT
        xz_native_service();
#endif
        fd=station_web_service(fd);
#endif
        delay(50);
    }
}
int wifi_boot_hook(void *thread) {
    int r=FN(int,0x180648f8,void *)(thread);
    if(S->magic!=0x38303243) {
        memset(S,0,sizeof(*S));
#ifdef C208_WIFI_V93
        S->portal_dns_fd=-1;
#endif
        S->magic=0x38303243;S->bank=1;S->status="正在初始化";
        void *t=FN(void *,0x18064604,const char *,task_fn,void *,unsigned,unsigned,unsigned)("wificfg",worker,0,
#ifdef C208_WIFI_V90
            8192,
#else
            4096,
#endif
            24,10);
        if(!t||FN(int,0x180648f8,void *)(t)!=0) S->status="配网线程启动失败";
    }
    return r;
}
/* Both original call sites go through this guard before starting quick transfer. */
int wifi_legacy_ap_hook(void) {
#ifdef C208_WIFI_V96
    return -1;
#else
    S->legacy_busy=1;
    if(S->ap_owned||S->auto_connect||S->connecting) {
        S->auto_connect=0;S->command=3;BARRIER();
        unsigned i=0;while(i++<300&&(S->ap_owned||S->command||S->connecting))delay(10);
        if(S->ap_owned||S->command||S->connecting) { S->legacy_busy=0;return -1; }
    }
    int r=FN(int,0x18050554,void)();S->legacy_busy=0;return r;
#endif
}
static void go_back(void) { FN(int,0x1805b53c,int)(13); }
static void back_click(void *obj,int event) { (void)obj;if(event==6)go_back(); }
void wifi_back_gesture(int gesture) { if(gesture==4)go_back(); }
static void page_open(void *obj,int event) { (void)obj;if(event==6)FN(int,0x1805b53c,int)(PAGE); }
static void row_click(void *obj,int event) {
    if(event!=6||!obj)return;
    unsigned cmd=*(unsigned *)((uint8_t *)obj+0x4c);
#ifdef C208_WIFI_V94
    if(cmd==7) { go_back();return; }
    if(cmd!=5)return;
    cmd=(S->ap_owned||S->operation_busy||S->connecting||S->command)?3:1;
#endif
#ifdef C208_WIFI_V92
    if(cmd==0)return;
    if(cmd==7) { go_back();return; }
    if(cmd==8&&S->ap_owned) { notify("请先关闭手机配网");return; }
    if(cmd==5)cmd=(S->ap_owned||S->operation_busy)?3:1;
    if(cmd==6)cmd=(S->connecting||connected())?3:2;
    if(S->operation_busy||S->connecting||S->command) {
        if(cmd!=3) { notify("正在处理，请稍候");return; }
    }
    if(cmd==2&&!S->have_saved) { notify("未保存网络，请先手机配网");return; }
    if(cmd==4) {
        if(!S->have_saved) { notify("没有需要清除的网络");return; }
        if(!S->forget_until||(int32_t)(S->forget_until-clock_ms())<=0) {
            S->forget_until=clock_ms()+5000;notify("再次点击确认清除");return;
        }
    }
    S->forget_until=0;
#endif
    if((cmd>=1&&cmd<=4)
#ifdef C208_WIFI_V92
        ||cmd==8
#endif
    ) {
        unsigned key=irq_off();if(!S->command||cmd==3)S->command=cmd;irq_on(key);
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V94)
        notify((cmd==1||cmd==8)?"正在准备手机配网":cmd==2?"正在连接保存网络":cmd==3?"正在关闭连接":"正在清除网络");
#endif
    }
}
#if !defined(C208_WIFI_V92) && !defined(C208_WIFI_V94)
static void add_row(void *list,const char *title,const char *detail,unsigned cmd) {
#ifdef C208_WIFI_V89
    const void *icon=&wifi_icon;
#else
    const void *icon=0;
#endif
    void *row=FN(void *,0x1805c42c,void *,const void *,const char *,const char *)(list,icon,title,detail);
    if(row) { *(unsigned *)((uint8_t *)row+0x4c)=cmd;FN(void,0x18088758,void *,event_fn)(row,row_click); }
}
#endif
#ifndef C208_WIFI_V94
static void ip_text(char *out,uint32_t ip) {
    unsigned at=0;for(unsigned j=0;j<4;j++) { unsigned n=(ip>>(j*8))&255;
        if(n>=100)out[at++]=(char)('0'+n/100);if(n>=10)out[at++]=(char)('0'+(n/10)%10);out[at++]=(char)('0'+n%10);
        if(j!=3)out[at++]='.'; }out[at]=0;
}
#endif
#ifdef C208_WIFI_V90
static void *live_row(void *list,const char *text) {
    void *row=FN(void *,0x1805c42c,void *,const void *,const char *,const char *)(list,&wifi_icon,text,0);
    if(!row)return 0;
    *(unsigned *)((uint8_t *)row+0x4c)=0;
    FN(void,0x18088758,void *,event_fn)(row,row_click);
    return FN(void *,0x18099538,void *)(row);
}
#if defined(C208_WIFI_V92) || defined(C208_WIFI_V94)
static void *action_row(void *list,const char *title,const char *detail,unsigned command) {
    void *row=FN(void *,0x1805c42c,void *,const void *,const char *,const char *)(list,&wifi_icon,title,detail);
    if(!row)return 0;
    *(unsigned *)((uint8_t *)row+0x4c)=command;
    FN(void,0x18088758,void *,event_fn)(row,row_click);
    return FN(void *,0x18099538,void *)(row);
}
static void label_text(void *label,const char *text) {
    if(label)FN(void,0x18098084,void *,const char *)(label,text);
}
#endif
static void refresh_status(void) {
#ifdef C208_WIFI_V95
    label_text(S->status_label,ready()&&sta_ip()?(S->ap_owned?"已连接 / 正在关闭配网":"已连接路由器 / 配网已关闭"):S->status?S->status:"正在初始化");
    label_text(S->primary_label,S->ap_owned?"关闭手机配网":
        (S->operation_busy||S->connecting||S->command)?"取消配网准备":"开启手机配网");
    char address[16];management_ip(address,ready()&&sta_ip()?sta_ip():S->ap_owned?AP_IP:0);
    label_text(S->ip_label,address);
#elif defined(C208_WIFI_V94)
    label_text(S->status_label,S->status?S->status:"正在初始化");
    label_text(S->primary_label,S->ap_owned?"关闭手机配网":
        (S->operation_busy||S->connecting||S->command)?"取消配网准备":"开启手机配网");
    label_text(S->ip_label,"192.168.84.1");
#elif defined(C208_WIFI_V92)
    char line[96];const char *state=S->status?S->status:"正在初始化";
    const char *prefix="状态：";unsigned at=0;
    while(*prefix)line[at++]=*prefix++;
    while(*state&&at+1<sizeof(line))line[at++]=*state++;line[at]=0;label_text(S->status_label,line);
    const char *name=0;char ssid[33];
    if(S->ap_owned) {prefix="热点：";name="C208-Setup";}
    else if(ready()) {unsigned n=U8(0x20020944);if(n>32)n=32;memcpy(ssid,(const void *)0x20020945,n);ssid[n]=0;prefix="网络：";name=ssid;}
    else {prefix="保存：";name=S->have_saved?S->saved.ssid:"未配置";}
    at=0;while(*prefix)line[at++]=*prefix++;while(*name&&at+1<sizeof(line))line[at++]=*name++;line[at]=0;
    label_text(S->network_label,line);
    char ip[16];ip_text(ip,S->ap_owned?AP_IP:ready()?sta_ip():0);
    prefix=S->ap_owned?"浏览器：":"IP：";at=0;while(*prefix)line[at++]=*prefix++;
    for(unsigned i=0;ip[i];i++)line[at++]=ip[i];line[at]=0;label_text(S->ip_label,line);
    if(S->ap_owned) {
        unsigned elapsed=(clock_ms()-S->ap_ticks)/1000;unsigned sec=elapsed<300?300-elapsed:0;
        const char *lead="配网剩余：";at=0;while(*lead)line[at++]=*lead++;
        line[at++]=(char)('0'+sec/60);line[at++]=':';line[at++]=(char)('0'+sec%60/10);line[at++]=(char)('0'+sec%10);line[at]=0;
    } else {
        const char *lead="退出页面不会断开网络";at=0;while(*lead)line[at++]=*lead++;line[at]=0;
    }
    label_text(S->countdown_label,line);
    label_text(S->primary_label,S->ap_owned?"关闭手机配网":S->operation_busy?"取消配网准备":"开启手机配网");
    label_text(S->saved_action_label,S->connecting?"取消网络连接":connected()?"断开当前 Wi-Fi":"连接已保存网络");
    if(S->forget_until&&(int32_t)(S->forget_until-clock_ms())<=0)S->forget_until=0;
    label_text(S->forget_label,S->forget_until?"再次点击确认清除":"清除保存的网络");
#else
    if(S->status_label)FN(void,0x18098084,void *,const char *)(S->status_label,S->status?S->status:"正在初始化");
    const char *network=S->ap_owned?"C208-Setup":S->have_saved?S->saved.ssid:"未保存网络";
    if(S->network_label)FN(void,0x18098084,void *,const char *)(S->network_label,network);
    if(S->ip_label) {
        char ip[16];ip_text(ip,S->ap_owned?AP_IP:ready()?sta_ip():0);
        FN(void,0x18098084,void *,const char *)(S->ip_label,ip);
    }
#endif
}
#endif
void wifi_page_init(void) {
    void *root=FN(void *,0x1805afe8,int,event_fn)(42,back_click);
    FN(void,0x1805b1a0,const char *)("Wi-Fi 设置");if(!root)return;
    void *list=FN(void *,0x1805c22c,int,void *,event_fn)(1,root,row_click);S->page_list=list;if(!list)return;
#ifdef C208_WIFI_V90
    S->previous_sleep=U8(0x20011c26);S->sleep_held=1;
    if(!S->previous_sleep)FN(void,0x1805156c,unsigned)(1);
#ifdef C208_WIFI_V94
    S->status_label=live_row(list,S->status?S->status:"正在初始化");
    S->primary_label=action_row(list,"开启手机配网","C208-Setup / 无密码",5);
    S->ip_label=live_row(list,"192.168.84.1");
    action_row(list,"返回本机设置",0,7);
#else
    S->status_label=live_row(list,S->status?S->status:"正在初始化");
    S->network_label=live_row(list,"未保存网络");
    S->ip_label=live_row(list,"0.0.0.0");
#ifdef C208_WIFI_V92
    S->countdown_label=live_row(list,"手机配网");
    S->primary_label=action_row(list,"开启手机配网","扫描后开启无密码热点",5);
    S->saved_action_label=action_row(list,"连接已保存网络",0,6);
    S->forget_label=action_row(list,"清除保存的网络","需再次点击确认",4);
    action_row(list,"手动输入配网","跳过扫描快速开启",8);
    action_row(list,"返回本机设置",0,7);
#else
    add_row(list,"开启 AP 配网","免密码 / 5 分钟",1);
    add_row(list,"关闭热点 / 断开 Wi-Fi",0,3);
    add_row(list,"连接已保存网络",0,2);
    add_row(list,"清除保存的网络",0,4);
#endif
#endif
    refresh_status();
#else
    add_row(list,"状态",S->status?S->status:"正在初始化",0);
    if(S->ap_owned) {
        add_row(list,"免密码热点","C208-Setup",0);add_row(list,"浏览器地址","192.168.84.1",0);
        add_row(list,"关闭配网热点",0,3);
    } else {
        if(ready()&&sta_ip()) {
            char ip[16],ssid[33];unsigned n=U8(0x20020944);if(n>32)n=32;
            memcpy(ssid,(const void *)0x20020945,n);ssid[n]=0;
            ip_text(ip,sta_ip());add_row(list,"当前网络",ssid,0);add_row(list,"IP 地址",ip,0);
        }
        add_row(list,"开启 AP 配网","免密码 / 5 分钟",1);
    }
    if(S->have_saved)add_row(list,"保存的网络",S->saved.ssid,0);
    add_row(list,"连接已保存网络",0,2);add_row(list,"断开 Wi-Fi",0,3);add_row(list,"清除保存的网络",0,4);
#endif
    FN(void,0x1805c4dc,void *)(list);FN(void,0x18086994,void *)(wifi_back_gesture);
}
void wifi_page_deinit(void) {
    S->page_list=0;
#ifdef C208_WIFI_V90
    S->status_label=0;S->network_label=0;S->ip_label=0;
#ifdef C208_WIFI_V94
    S->primary_label=0;
#endif
#ifdef C208_WIFI_V92
    S->primary_label=0;S->saved_action_label=0;S->forget_label=0;S->countdown_label=0;S->forget_until=0;
#endif
    if(S->sleep_held) {
        S->sleep_held=0;
        if(!S->previous_sleep)FN(void,0x1805156c,unsigned)(0);
    }
#endif
    FN(void,0x1805b1dc,void)();
}
void wifi_page_message(const uint8_t *msg) {
    if(msg&&msg[0]==MSG_UPDATE) {
#ifdef C208_WIFI_V90
        refresh_status();
#else
        wifi_page_deinit();wifi_page_init();
#endif
    }
}
#ifdef C208_XIAOZHI_INPUT
void *wifi_settings_create(int kind,void *root,event_fn callback){
    void *list=FN(void *,0x1805c22c,int,void *,event_fn)(kind,root,callback);
    if(!list)return list;
    void *row=FN(void *,0x1805c42c,void *,const void *,const char *,const char *)(list,&wifi_icon,"Wi-Fi 设置",0);
    if(row){*(unsigned *)((uint8_t *)row+0x4c)=13;FN(void,0x18088758,void *,event_fn)(row,page_open);}
    return list;
}
void wifi_settings_append(void *list){FN(void,0x1805c4dc,void *)(list);}
#else
void wifi_settings_append(void *list) {
#ifdef C208_WIFI_V89
    const void *icon=&wifi_icon;
#else
    const void *icon=0;
#endif
    void *row=FN(void *,0x1805c42c,void *,const void *,const char *,const char *)(list,icon,"Wi-Fi 设置",0);
    if(row) { *(unsigned *)((uint8_t *)row+0x4c)=13;FN(void,0x18088758,void *,event_fn)(row,page_open); }
#ifdef C208_WIFI_V89
    FN(void,0x1805c4dc,void *)(list);
#else
    FN(void,0x18588918,void *)(list); /* Historical V0086/V0088 factory entry. */
#endif
}
#endif

#if defined(C208_WIFI_V93) || defined(C208_WIFI_V94)
/* At the original page tail r6 is the root and r7 its text style. */
void wifi_device_info_update(void *gesture,void *root,void *style) {
    FN(void,0x18086994,void *)(gesture);
    uintptr_t context=U32(0x20011da8);
    if(context&&U32(context+4)) {
        uint16_t major=0,minor=0,patch=0;char text[64];
        FN(void,0x180416d0,uint16_t *,uint16_t *,uint16_t *)(&major,&minor,&patch);
        unsigned version=FN(unsigned,0x180416c4,void)();
        FN(int,0x180dd0fc,char *,const char *,...)(text,"ZhiyinOS %d.%d.%d V%04d",(int)major,(int)minor,(int)patch,(int)version);
        FN(void,0x18098084,void *,const char *)((void *)U32(context+4),text);
    }
    void *label=FN(void *,0x18098300,void *,void *)(root,0);
    if(!label)return;
    FN(void,0x180981f0,void *,int)(label,1);
    FN(void,0x18097548,void *,int)(label,1);
    FN(void,0x18089c34,void *,int)(label,240);
    FN(void,0x1808969c,void *,void *)(label,style);
    FN(void,0x18098084,void *,const char *)(label,"By清风惹尘丶");
    FN(void,0x18089000,void *,void *,int,int,int)(label,root,1,0,208);
}
#endif
