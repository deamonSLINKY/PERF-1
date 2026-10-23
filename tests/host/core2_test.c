/* SPDX-License-Identifier: GPL-3.0-only */
/* The reverb as a job another core can run (en_core2_attach): the output must be bit for bit the same
 * whichever way the job runs, and a lost core must not take the sound down.
 *   serial    no second core: the job runs in place
 *   deferred  "the other core" runs the job at join, after the chunk's input was written: if the job read a slot
 *             being written, or the tank state moved under it, the output would differ
 *   thread    a real second thread runs it, concurrently with the render
 *   mixed     the second core is busy for some chunks (submit refuses): in place for those, thread for the rest
 *   lost      the thread is lost at chunk 400 (join answers 0): the sound goes on, with no NaN, on one core
 *   lostpad   it stops answering at chunk 500, while the first core waits for the pad's stage (alive answers 0)
 * Prints "<mode> <checksum of the output>"; tests/run_tests.sh compares the first four. */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static void (*job_fn)(void *);
static void *job_ctx;
static atomic_uint req, done;
static atomic_int quit;
static int mode;                    /* 0 serial 1 deferred 2 thread 3 mixed 4 lost 5 lostpad */
static unsigned chunk, mix_state = 12345u;

static void *worker(void *a)
{
    unsigned seen = 0;
    (void)a;
    while (!atomic_load(&quit)) {
        unsigned r = atomic_load_explicit(&req, memory_order_acquire);
        if (r == seen)
            continue;
        job_fn(job_ctx);
        seen = r;
        atomic_store_explicit(&done, r, memory_order_release);
    }
    return 0;
}

static int submit(void (*fn)(void *), void *ctx)
{
    chunk++;
    if ((mode == 4 && chunk > 400u) || (mode == 5 && chunk > 500u))
        return 0;               /* retired: in place from here */
    if (mode == 3) {
        mix_state = mix_state * 1103515245u + 12345u;
        if ((mix_state >> 16) % 3u == 0u)
            return 0;               /* busy: run it in place */
    }
    job_fn = fn;
    job_ctx = ctx;
    if (mode == 1) {
        atomic_fetch_add(&req, 1u);  /* it runs at join */
        return 1;
    }
    atomic_fetch_add_explicit(&req, 1u, memory_order_release);
    return 1;
}

static void fence(void) { atomic_thread_fence(memory_order_seq_cst); }
static int alive(void)
{
    if (mode == 1 && atomic_load(&done) != atomic_load(&req)) {      /* deferred: the job runs when the first core waits for it */
        job_fn(job_ctx);
        atomic_store(&done, atomic_load(&req));
    }
    return !(mode == 5 && chunk == 500u);
}

static int join(void)
{
    if (mode == 5 && chunk == 500u) {
        while (atomic_load_explicit(&done, memory_order_acquire) != atomic_load(&req))
            ;                       /* (the stand-in thread is let finish: nothing real to hold) */
        return 0;
    }
    if (mode == 1) {                /* it ran when the pad's stage was waited for (alive) */
        if (atomic_load(&done) != atomic_load(&req)) {
            job_fn(job_ctx);
            atomic_store(&done, atomic_load(&req));
        }
        return 1;
    }
    if (mode == 4 && chunk == 400u) {
        atomic_store(&done, atomic_load(&req));   /* (the job is abandoned: mark it finished) */
        return 0;
    }
    while (atomic_load_explicit(&done, memory_order_acquire) != atomic_load(&req))
        ;
    return 1;
}

int main(int argc, char **argv)
{
    static int32_t buf[2 * 256];
    static const unsigned sizes[] = {256, 173, 40, 256, 99, 256, 256, 211};
    const char *names[] = {"serial", "deferred", "thread", "mixed", "lost", "lostpad"};
    uint32_t h = 2166136261u, nan = 0;
    pthread_t th;
    unsigned b;
    FILE *dump = argc > 2 ? fopen(argv[2], "wb") : 0;     /* the whole output, to compare builds (tools, not the suite) */
    mode = argc > 1 ? atoi(argv[1]) : 0;
    en_init();
    en_play(1);
    en_set(P_REVERB, 100);
    if (mode > 0) {
        en_core2_attach(submit, join, fence, alive);
        if (mode != 1)
            pthread_create(&th, 0, worker, 0);
    }
    for (b = 0; b < 1200u; b++) {
        unsigned n = sizes[b % (sizeof sizes / sizeof sizes[0])], j;
        en_render(buf, n, 2048);
        if (dump)
            fwrite(buf, 4, 2 * n, dump);
        for (j = 0; j < 2 * n; j++) {
            h = (h ^ (uint32_t)buf[j]) * 16777619u;
            if (buf[j] > 8388607 || buf[j] < -8388608)
                nan++;
        }
    }
    if (mode > 0 && mode != 1) {
        atomic_store(&quit, 1);
        pthread_join(th, 0);
    }
    if (nan) {
        printf("FAIL %u samples out of range\n", nan);
        return 1;
    }
    if (mode > 0 && en_c2_jobs == 0) {
        printf("FAIL no job ran on the other core\n");
        return 1;
    }
    if ((mode == 4 || mode == 5) && en_c2_lost != 1) {
        printf("FAIL lost %u times\n", (unsigned)en_c2_lost);
        return 1;
    }
    printf("%s %08x\n", names[mode], (unsigned)h);
    return 0;
}
