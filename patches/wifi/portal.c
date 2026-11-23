#include "portal.h"
static unsigned be16(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
int portal_client_ip(uint32_t ip){
    unsigned host=ip>>24;
    return (ip&0x00ffffffu)==0x0054a8c0u&&host>=2&&host<=254;
}
unsigned portal_dns_reply(const uint8_t *p,unsigned n,uint8_t *q,unsigned cap){
    if(n<17||n>512||(p[2]&0xf8)||be16(p+4)!=1||be16(p+6)||be16(p+8))return 0;
    unsigned at=12,wire=0;
    for(;;){
        if(at>=n)return 0;
        unsigned len=p[at++];wire++;
        if(len>63||wire>255||at+len>n)return 0;
        if(!len)break;
        wire+=len;if(wire>255)return 0;at+=len;
    }
    if(at+4>n||be16(p+at+2)!=1)return 0;
    unsigned type=be16(p+at);at+=4;
    unsigned answer=type==1,total=at+(answer?16:0);
    if(total>cap)return 0;
    for(unsigned i=0;i<at;i++)q[i]=p[i];
    q[2]=(uint8_t)(0x84|(p[2]&1));q[3]=0; /* QR, authoritative, preserve RD, no recursion. */
    q[4]=0;q[5]=1;q[6]=0;q[7]=(uint8_t)answer;q[8]=0;q[9]=0;q[10]=0;q[11]=0;
    if(answer){
        static const uint8_t rr[16]={0xc0,0x0c,0,1,0,1,0,0,0,10,0,4,192,168,84,1};
        for(unsigned i=0;i<16;i++)q[at+i]=rr[i];
    }
    /* AAAA / HTTPS / other types return NOERROR with no data; IPv4 A is local. */
    return total;
}
int portal_get_line(const char *p,unsigned n){
    if(n<14||p[0]!='G'||p[1]!='E'||p[2]!='T'||p[3]!=' '||p[4]!='/')return 0;
    const char *v=p+n-9;
    if(v[0]!=' '||v[1]!='H'||v[2]!='T'||v[3]!='T'||v[4]!='P'||v[5]!='/'||v[6]!='1'||v[7]!='.'||(v[8]!='0'&&v[8]!='1'))return 0;
    for(unsigned i=4;i<n-9;i++)if((unsigned char)p[i]<=32||(unsigned char)p[i]>=127)return 0;
    return 1;
}
