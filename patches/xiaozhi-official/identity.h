#ifndef C208_OFFICIAL_IDENTITY_H
#define C208_OFFICIAL_IDENTITY_H
#include <stdint.h>
#define XO_ID_BYTES 112u
struct xo_identity_io {
 int (*read)(void *,unsigned bank,uint8_t record[XO_ID_BYTES]);
 int (*commit)(void *,unsigned bank,const uint8_t record[XO_ID_BYTES]);
 void *context;
};
/* Both journals must validate. Broken/missing journals fail closed, never reuse an older seed counter. */
int xo_identity_reserve(const struct xo_identity_io *,const uint8_t mac[6],uint64_t now,
                        char uuid[37],uint8_t entropy[32]);
int xo_identity_validate(const uint8_t record[XO_ID_BYTES]);
#endif
