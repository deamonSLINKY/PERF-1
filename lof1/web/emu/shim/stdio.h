#pragma once
#include <stddef.h>
typedef struct shim_file FILE;
extern FILE *stderr, *stdout;
FILE *fopen(const char *p, const char *m);
int fclose(FILE *f);
size_t fwrite(const void *b, size_t s, size_t n, FILE *f);
size_t fread(void *b, size_t s, size_t n, FILE *f);
int fprintf(FILE *f, const char *fmt, ...);
int printf(const char *fmt, ...);
int snprintf(char *b, size_t n, const char *fmt, ...);
int sscanf(const char *s, const char *fmt, ...);
int fseek(FILE *f, long o, int w);
long ftell(FILE *f);
int fflush(FILE *f);
char *fgets(char *b, int n, FILE *f);
int fputc(int c, FILE *f);
int fputs(const char *s, FILE *f);
#define SEEK_SET 0
#define SEEK_END 2
