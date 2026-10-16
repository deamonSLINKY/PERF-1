/* SPDX-License-Identifier: GPL-3.0-only */
#include "string.h"
#include "time.h"
void *memcpy(void *d, const void *s, size_t n) { char *a = d; const char *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n)
{
    char *a = d; const char *b = s;
    if (a < b) while (n--) *a++ = *b++;
    else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}
void *memset(void *d, int c, size_t n) { char *a = d; while (n--) *a++ = (char)c; return d; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return (unsigned char)*a - (unsigned char)*b; }
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int clock_gettime(int c, struct timespec *t) { (void)c; t->tv_sec = 0; t->tv_nsec = 0; return 0; }
/* the simulator's script and file code: present in the source, never called in the browser */
#include "stdio.h"
#include "stdlib.h"
FILE *stderr, *stdout;
FILE *fopen(const char *p, const char *m) { (void)p; (void)m; return 0; }
int fclose(FILE *f) { (void)f; return 0; }
size_t fwrite(const void *b, size_t s, size_t n, FILE *f) { (void)b; (void)s; (void)f; return n; }
size_t fread(void *b, size_t s, size_t n, FILE *f) { (void)b; (void)s; (void)n; (void)f; return 0; }
int fprintf(FILE *f, const char *fmt, ...) { (void)f; (void)fmt; return 0; }
int printf(const char *fmt, ...) { (void)fmt; return 0; }
int snprintf(char *b, size_t n, const char *fmt, ...) { (void)fmt; if (n) b[0] = 0; return 0; }
int sscanf(const char *s, const char *fmt, ...) { (void)s; (void)fmt; return 0; }
int fseek(FILE *f, long o, int w) { (void)f; (void)o; (void)w; return -1; }
long ftell(FILE *f) { (void)f; return 0; }
int fflush(FILE *f) { (void)f; return 0; }
char *fgets(char *b, int n, FILE *f) { (void)b; (void)n; (void)f; return 0; }
int fputc(int c, FILE *f) { (void)f; return c; }
int fputs(const char *s, FILE *f) { (void)s; (void)f; return 0; }
int atoi(const char *s) { int v = 0, neg = *s == '-'; if (neg || *s == '+') s++; while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0'; return neg ? -v : v; }
long strtol(const char *s, char **e, int b) { (void)b; if (e) *e = (char *)s; return atoi(s); }
unsigned long strtoul(const char *s, char **e, int b) { (void)b; if (e) *e = (char *)s; return (unsigned long)atoi(s); }
double atof(const char *s) { return atoi(s); }
void exit(int c) { (void)c; __builtin_trap(); }
void *malloc(size_t n) { (void)n; return 0; }
void free(void *p) { (void)p; }
int abs(int x) { return x < 0 ? -x : x; }
#include "math.h"
int strncmp(const char *a, const char *b, size_t n) { for (; n && *a && *a == *b; n--) a++, b++; return n ? (unsigned char)*a - (unsigned char)*b : 0; }
char *strchr(const char *s, int c) { for (; *s; s++) if (*s == (char)c) return (char *)s; return c ? 0 : (char *)s; }
double log10(double x) { (void)x; return 0.0; }   /* the scripts' peak meter only */
double sqrt(double x) { return __builtin_sqrt(x); }
float sqrtf(float x) { return __builtin_sqrtf(x); }
