#pragma once
struct timespec { long tv_sec, tv_nsec; };
#define CLOCK_MONOTONIC 1
int clock_gettime(int c, struct timespec *t);
