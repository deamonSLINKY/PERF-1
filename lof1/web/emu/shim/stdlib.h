#pragma once
#include <stddef.h>
int atoi(const char *s);
long strtol(const char *s, char **e, int b);
unsigned long strtoul(const char *s, char **e, int b);
double atof(const char *s);
void exit(int c);
void *malloc(size_t n);
void free(void *p);
int abs(int x);
