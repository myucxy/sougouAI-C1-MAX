#ifndef C208_OPUS_STDIO
#define C208_OPUS_STDIO
typedef struct c208_file FILE;
extern FILE *stderr;int fprintf(FILE *,const char *,...);int fputc(int,FILE *);int printf(const char *,...);
#endif
