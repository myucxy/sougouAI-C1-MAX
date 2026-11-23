#include "tls.h"
#include <string.h>
static int hostname_valid(const char *p){
 if(!p||!*p)return 0;unsigned n=0,label=0;
 for(;*p;p++){
  unsigned c=(unsigned char)*p;if(++n>253)return 0;
  if(c=='.'){if(!label||p[-1]=='-')return 0;label=0;}
  else if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'){if(!label&&c=='-')return 0;if(++label>63)return 0;}
  else return 0;
 }return label&&p[-1]!='-';
}
unsigned xo_tls_context_bytes(void){return (unsigned)sizeof(struct xo_tls);}
int xo_tls_init(struct xo_tls *t,const char *host,const br_x509_trust_anchor *roots,unsigned n,
 uint64_t now,const unsigned char *entropy,unsigned bytes){
 /* No timer/MAC/constant seed fallback; no certificate or hostname bypass. */
 if(!t||!hostname_valid(host)||!roots||!n||!entropy||bytes<32||now<1735689600ULL||now>4102444800ULL)return 0;
 br_ssl_client_init_full(&t->client,&t->verifier,roots,n);
 br_ssl_engine_set_versions(&t->client.eng,BR_TLS12,BR_TLS12);
 static const uint16_t suites[]={BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
  BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
  BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256};
 br_ssl_engine_set_suites(&t->client.eng,suites,sizeof(suites)/sizeof(suites[0]));
 br_x509_minimal_set_time(&t->verifier,(uint32_t)(now/86400)+719528,(uint32_t)(now%86400));
 br_ssl_engine_set_buffers_bidi(&t->client.eng,t->input,sizeof(t->input),t->output,sizeof(t->output));
 br_ssl_engine_inject_entropy(&t->client.eng,entropy,bytes);
 return br_ssl_client_reset(&t->client,host,0);
}
