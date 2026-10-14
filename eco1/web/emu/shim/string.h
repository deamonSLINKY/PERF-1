/* SPDX-License-Identifier: GPL-3.0-only */
/* the web build without Emscripten (web/emu/build_clang.sh): the few libc pieces it uses */
#pragma once
#include <stddef.h>
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int strcmp(const char *a, const char *b);
size_t strlen(const char *s);
int strncmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);
