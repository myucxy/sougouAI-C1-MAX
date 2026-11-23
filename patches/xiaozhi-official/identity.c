#include "identity.h"
#include "bearssl.h"
#include <string.h>
static uint64_t le64(const uint8_t *p){uint64_t n=0;for(unsigned i=0;i<8;i++)n|=(uint64_t)p[i]<<(8*i);return n;}
static void put64(uint8_t *p,uint64_t n){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(n>>(8*i));}
static void authenticate(const uint8_t *record,uint8_t out[32]){
 br_hmac_key_context key;br_hmac_context hmac;br_hmac_key_init(&key,&br_sha256_vtable,record+32,32);
 br_hmac_init(&hmac,&key,32);br_hmac_update(&hmac,record,80);br_hmac_out(&hmac,out);
}
int xo_identity_validate(const uint8_t record[XO_ID_BYTES]){
 if(memcmp(record,"XZID\1\0\160\0",8))return 0;
 if((record[22]&0xf0)!=0x40||(record[24]&0xc0)!=0x80)return 0;
 unsigned nonzero=0;for(unsigned i=32;i<64;i++)nonzero|=record[i];if(!nonzero)return 0;
 for(unsigned i=72;i<80;i++)if(record[i])return 0;
 uint64_t time=le64(record+64);if(time<1735689600ULL||time>4102444800ULL)return 0;
 uint8_t tag[32];authenticate(record,tag);unsigned diff=0;for(unsigned i=0;i<32;i++)diff|=tag[i]^record[80+i];return !diff;
}
int xo_identity_reserve(const struct xo_identity_io *io,const uint8_t mac[6],uint64_t now,char uuid[37],uint8_t entropy[32]){
 if(!io||!io->read||!io->commit||!mac||!uuid||!entropy)return 0;
 memset(entropy,0,32);uuid[0]=0;
 if((mac[0]&1)||now<1735689600ULL||now>4102444800ULL)return 0;
 unsigned nonzero=0;for(unsigned i=0;i<6;i++)nonzero|=mac[i];if(!nonzero)return 0;
 int ok=0;uint8_t a[XO_ID_BYTES]={0},b[XO_ID_BYTES]={0},next[XO_ID_BYTES]={0},verify[XO_ID_BYTES]={0};
 if(!io->read(io->context,0,a)||!io->read(io->context,1,b)||!xo_identity_validate(a)||!xo_identity_validate(b)||memcmp(a+16,b+16,48))goto finished;
 uint64_t left=le64(a+8),right=le64(b+8);if(left==right||((left>right?left-right:right-left)!=1))goto finished;
 unsigned bank=left>right?1:0;const uint8_t *current=bank?a:b;uint64_t seq=bank?left:right;
 /* RTC correction may move backwards; durable adjacent counters prevent seed reuse. */
 if(seq==UINT64_MAX)goto finished;
 memcpy(next,current,sizeof(next));put64(next+8,seq+1);put64(next+64,now);authenticate(next,next+80);
 if(!io->commit(io->context,bank,next)||!io->read(io->context,bank,verify)||memcmp(verify,next,sizeof(next)))goto finished;
 br_hmac_key_context key;br_hmac_context hmac;br_hmac_key_init(&key,&br_sha256_vtable,next+32,32);br_hmac_init(&hmac,&key,32);
 static const char domain[]="C208 official TLS connection v1";
 br_hmac_update(&hmac,domain,sizeof(domain)-1);br_hmac_update(&hmac,next+16,16);br_hmac_update(&hmac,mac,6);br_hmac_update(&hmac,next+8,8);br_hmac_out(&hmac,entropy);
 unsigned n=0;for(unsigned i=0;i<16;i++){if(i==4||i==6||i==8||i==10)uuid[n++]='-';uuid[n++]="0123456789abcdef"[next[16+i]>>4];uuid[n++]="0123456789abcdef"[next[16+i]&15];}uuid[n]=0;
 ok=1;
finished:;
 /* Reduce lifetime of the master key on the worker stack. */
 volatile uint8_t *wipe=a;for(unsigned i=0;i<sizeof(a);i++)wipe[i]=0;wipe=b;for(unsigned i=0;i<sizeof(b);i++)wipe[i]=0;wipe=next;for(unsigned i=0;i<sizeof(next);i++)wipe[i]=0;wipe=verify;for(unsigned i=0;i<sizeof(verify);i++)wipe[i]=0;
 return ok;
}
