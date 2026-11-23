#ifndef C208_OFFICIAL_TLS_H
#define C208_OFFICIAL_TLS_H
#include <stdint.h>
#include "bearssl.h"
/* Retain full incoming records while bounding outgoing plaintext fragments. */
#define XO_TLS_OUTPUT_PLAINTEXT_CAP 1024
struct xo_tls {br_ssl_client_context client;br_x509_minimal_context verifier;
 unsigned char input[BR_SSL_BUFSIZE_INPUT];
 unsigned char output[XO_TLS_OUTPUT_PLAINTEXT_CAP+85];
};
/* Caller supplies verified platform time, a CSPRNG seed and reviewed trust roots. */
int xo_tls_init(struct xo_tls *,const char *hostname,const br_x509_trust_anchor *,unsigned roots,
                uint64_t unix_seconds,const unsigned char *entropy,unsigned entropy_bytes);
unsigned xo_tls_context_bytes(void);
#endif
