#ifndef C208_OFFICIAL_STRING_H
#define C208_OFFICIAL_STRING_H
#include <stddef.h>
void *memcpy(void *,const void *,size_t);
void *memmove(void *,const void *,size_t);
void *memset(void *,int,size_t);
int memcmp(const void *,const void *,size_t);
int strcmp(const char *,const char *);
int strncmp(const char *,const char *,size_t);
size_t strlen(const char *);
char *strchr(const char *,int);
#endif
