/* Freestanding ARM EABI helpers. Cortex-M cannot run Android's ARM-state builtins. */
#include <stddef.h>
#include <stdint.h>
void *memcpy(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
void *memmove(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;if(a<b)while(n--)*a++=*b++;else while(n){n--;a[n]=b[n];}return d;}
void *memset(void *d,int v,size_t n){unsigned char *a=d;while(n--)*a++=(unsigned char)v;return d;}
int memcmp(const void *x,const void *y,size_t n){const unsigned char *a=x,*b=y;while(n--){if(*a!=*b)return *a-*b;a++;b++;}return 0;}
size_t strlen(const char *s){size_t n=0;while(s[n])n++;return n;}
int strcmp(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
int strncmp(const char *a,const char *b,size_t n){while(n--){if(*a!=*b)return (unsigned char)*a-(unsigned char)*b;if(!*a)return 0;a++;b++;}return 0;}
char *strchr(const char *s,int c){do{if(*s==(char)c)return (char *)s;}while(*s++);return 0;}
void __aeabi_memclr4(void *d,size_t n){memset(d,0,n);}
void __aeabi_memclr8(void *d,size_t n){memset(d,0,n);}
void __aeabi_memclr(void *d,size_t n){memset(d,0,n);}
void __aeabi_memcpy4(void *d,const void *s,size_t n){memcpy(d,s,n);}
void __aeabi_memcpy8(void *d,const void *s,size_t n){memcpy(d,s,n);}
void __aeabi_memcpy(void *d,const void *s,size_t n){memcpy(d,s,n);}
void __aeabi_memmove(void *d,const void *s,size_t n){memmove(d,s,n);}
void __aeabi_memset(void *d,size_t n,int value){memset(d,value,n);}
void __aeabi_memset4(void *d,size_t n,int value){memset(d,value,n);}
uint64_t __udivmoddi4(uint64_t numerator,uint64_t denominator,uint64_t *remainder){
 uint64_t q=0,r=0;
 if(!denominator){if(remainder)*remainder=numerator;return UINT64_MAX;}
 for(unsigned i=64;i;i--){unsigned carry=(unsigned)(r>>63);r=(r<<1)|((numerator>>(i-1))&1u);if(carry||r>=denominator){r-=denominator;q|=(uint64_t)1<<(i-1);}}
 if(remainder)*remainder=r;return q;
}
int abs(int n){return n<0?-n:n;}
int64_t c208_sdivmod(int64_t numerator,int64_t denominator,int64_t *remainder){
 uint64_t a=(uint64_t)numerator,b=(uint64_t)denominator,r;
 if(numerator<0)a=0-a;if(denominator<0)b=0-b;
 uint64_t q=__udivmoddi4(a,b,&r);if((numerator<0)!=(denominator<0))q=0-q;
 if(numerator<0)r=0-r;if(remainder)*remainder=(int64_t)r;return (int64_t)q;
}
