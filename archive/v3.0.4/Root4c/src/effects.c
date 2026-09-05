#include "effects.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

/* ─── Platform thread shim ─── */
#ifdef _WIN32
typedef struct { void* (*fn)(void*); void* arg; HANDLE h; } thread_t;
static unsigned __stdcall thread_thunk(void *p) {
    thread_t *t = (thread_t*)p;
    t->fn(t->arg);
    return 0;
}
static int thread_start(thread_t *t, void* (*fn)(void*), void *arg) {
    t->fn = fn; t->arg = arg;
    t->h = (HANDLE)_beginthreadex(NULL, 0, thread_thunk, t, 0, NULL);
    return t->h ? 1 : 0;
}
static void thread_join(thread_t *t) { WaitForSingleObject(t->h, INFINITE); CloseHandle(t->h); }
static void thread_sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
typedef struct { void* (*fn)(void*); void* arg; pthread_t h; } thread_t;
static int thread_start(thread_t *t, void* (*fn)(void*), void *arg) {
    t->fn = fn; t->arg = arg;
    return pthread_create(&t->h, NULL, fn, arg) == 0 ? 1 : 0;
}
static void thread_join(thread_t *t) { pthread_join(t->h, NULL); }
static void thread_sleep_ms(int ms) { usleep((useconds_t)ms * 1000); }
#endif

/* ─── Frame helpers ─── */
EffectFrame* effects_frame_new(int nrows, int width) {
    EffectFrame *f = (EffectFrame*)calloc(1, sizeof(EffectFrame));
    if (!f) return NULL;
    f->nrows = nrows;
    f->width = width;
    /* Each row is expanded with ANSI color sequences (open+char+reset can be
       ~24 bytes per source cell), so allocate a large fixed buffer. */
    f->rows = (char**)calloc((size_t)nrows + 1, sizeof(char*));
    if (!f->rows) { free(f); return NULL; }
    size_t cap = 8192;
    for (int i = 0; i < nrows; i++) {
        f->rows[i] = (char*)calloc(cap, 1);
        if (!f->rows[i]) {
            for (int j = 0; j < i; j++) free(f->rows[j]);
            free(f->rows);
            free(f);
            return NULL;
        }
    }
    return f;
}

void effects_frame_free(EffectFrame *f) {
    if (!f) return;
    if (f->rows) {
        for (int i = 0; i < f->nrows; i++) free(f->rows[i]);
        free(f->rows);
    }
    free(f);
}

void effects_erase(int nrows) {
    for (int i = 0; i < nrows; i++) printf("\033[1A\033[2K");
    fflush(stdout);
}

bool effects_skip(void) {
    return getenv("EMTYPYIE_NO_ANIM") != NULL;
}

/* ─── Bake context (race-free parallel baking) ─── */

typedef struct {
    EffectProducer producer;
    void *user;
    int   total;        /* number of frames to bake */
    int   next_index;   /* next frame index to assign to a worker */
    int   done;         /* number of frames baked so far */
    int   workers;
    EffectFrame **frames;  /* pre-allocated array, one slot per frame */
    volatile int stop;
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mtx;
#endif
} BakeCtx;

static void bake_lock(BakeCtx *c) {
#ifdef _WIN32
    EnterCriticalSection(&c->cs);
#else
    pthread_mutex_lock(&c->mtx);
#endif
}
static void bake_unlock(BakeCtx *c) {
#ifdef _WIN32
    LeaveCriticalSection(&c->cs);
#else
    pthread_mutex_unlock(&c->mtx);
#endif
}
static void* worker_main(void *arg) {
    BakeCtx *c = (BakeCtx*)arg;
    for (;;) {
        bake_lock(c);
        if (c->stop || c->next_index >= c->total) { bake_unlock(c); break; }
        int idx = c->next_index++;
        bake_unlock(c);

        /* Each worker bakes a distinct frame index into its own slot.
           No slot is shared concurrently, and frames are never freed by
           the player while workers may touch them — race-free. */
        EffectFrame *f = c->producer(idx, c->user);
        c->frames[idx] = f;

        bake_lock(c);
        c->done++;
        bake_unlock(c);
    }
    return NULL;
}

void effects_play(EffectProducer producer, int total_frames, int fps,
                  int workers, void *user) {
    if (effects_skip()) return;
    if (fps <= 0) fps = 20;
    if (workers < 1) workers = 1;
    if (workers > 8) workers = 8;
    if (total_frames <= 0) return;

    BakeCtx c;
    memset(&c, 0, sizeof(c));
    c.producer = producer;
    c.user = user;
    c.total = total_frames;
    c.workers = workers;
    c.next_index = 0;
    c.done = 0;
    c.stop = 0;
    c.frames = (EffectFrame**)calloc((size_t)total_frames, sizeof(EffectFrame*));
#ifdef _WIN32
    InitializeCriticalSection(&c.cs);
#else
    pthread_mutex_init(&c.mtx, NULL);
#endif

    /* Phase 1: bake all frames in parallel across the worker pool. */
    thread_t *ts = (thread_t*)calloc((size_t)workers, sizeof(thread_t));
    for (int i = 0; i < workers; i++) thread_start(&ts[i], worker_main, &c);
    for (int i = 0; i < workers; i++) thread_join(&ts[i]);
    free(ts);

#ifdef _WIN32
    DeleteCriticalSection(&c.cs);
#else
    pthread_mutex_destroy(&c.mtx);
#endif

    /* Phase 2: blit frames in order on the main thread. */
    int last_rows = 0;
    for (int i = 0; i < total_frames; i++) {
        EffectFrame *f = c.frames[i];
        if (!f) continue;
        if (i > 0 && last_rows > 0) effects_erase(last_rows);
        for (int r = 0; r < f->nrows; r++) printf("%s\n", f->rows[r]);
        fflush(stdout);
        last_rows = f->nrows;
        int delay_ms = 1000 / fps;
        thread_sleep_ms(delay_ms);
        effects_frame_free(f);
        c.frames[i] = NULL;
    }
    free(c.frames);
}

/* ─── Baking Bread art ───
 * Signature v3.0.2 about-art. Rendered with theme colors; the shimmer
 * effect shifts which palette role (main/accent/dim) each cell uses per
 * frame, giving a gentle "baking" glow. */

static const char *BAKE_ART[] = {
"████████████████████████████                        ",
"                                      ████░░░░▒▒░░▒▒░░░░░░░░▒▒▒▒░░░░░░████                      ",
"                                    ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██                    ",
"                                  ████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒██                  ",
"                                ██        ████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██                ",
"                              ██              ▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒▓▓              ",
"                            ▓▓░░              ░░▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒██            ",
"                            ██                    ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██            ",
"                          ██                      ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██          ",
"                          ██                      ░░██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██          ",
"                          ██                        ██░░░░░░░░░░░░░░▒▒░░░░░░░░░░░░░░▒▒▓▓        ",
"                        ██                            ▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██        ",
"                        ██                            ▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██      ",
"                        ██                              ▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██      ",
"              ████████  ██░░                            ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██    ",
"            ██        ████░░░░░░                        ██▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██    ",
"          ██              ████░░░░                        ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██    ",
"        ▓▓░░              ░░░░▓▓░░░░░░                    ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒██  ",
"      ██                        ████░░░░░░░░              ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██  ",
"    ██                              ██░░░░░░░░░░░░          ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██  ",
"    ██                                ████░░░░░░░░░░░░      ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██  ",
"  ▓▓░░                                ░░▒▒▓▓▓▓░░░░░░░░  ░░░░██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒▓▓",
"  ██                                        ░░████░░░░░░░░░░░░██░░░░░░░░░░░░░░░░░░░░░░░░░▒▒██",
"██░░██                                        ░░░░██░░░░░░░░░░██▒▒░░░░░░░░░░░░░░░░░░░░░▒▒▒▒██",
"██░░░░██                                        ░░░░██░░░░░░░░██▒▒▒▒▒▒░░░░░░░░░░░░░▒▒▒▒▒▒▒▒██",
"██░░░░▒▒▓▓▓▓                                      ░░░░▓▓▓▓░░░░██▒▒▒▒▒▒▒▒▒▒▒▒░░░░░░░▒▒▒▒▒▒▒▒██",
"  ██░░░░░░░░████                                  ████░░░░██░░██▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒██",
"    ██░░░░░░░░░░██████                      ██████░░░░░░░░░░████▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒██",
"  ██  ████░░░░░░░░░░▒▒██████            ████░░░░░░░░░░░░░░▒▒▒▒██▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒██",
"  ████  ░░████░░░░░░░░░░░░░░████████████░░░░░░░░░░░░████▒▒▒▒▒▒▒▒██▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒██",
"  ██▒▒██    ░░██████░░░░░░░░▒▒▒▒░░░░▒▒░░░░░░░░██████▒▒▒▒████▒▒▒▒██▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒██",
"  ██░░░░████    ░░░░██████░░░░░░░░░░░░████████░░░░▒▒▒▒▒▒▒▒▒▒██▒▒▒▒██▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒██",
"    ██░░░░▒▒▓▓▓▓██    ░░░░██████████▓▓░░████▒▒░░░░░░████▒▒▒▒██▒▒▒▒██▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒██  ",
"      ██░░░░░░░░░░████████    ░░░░░░░░██░░░░░░░░████    ██▒▒▒▒██▒▒██▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒██  ",
"      ░░▓▓▓▓░░░░░░▒▒▒▒▒▒▒▒▓▓██▒▒▓▓▓▓▓▓▒▒░░░░▓▓██        ▓▓▒▒▒▒██▒▒▒▒██▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▓▓██    ",
"        ░░░░▓▓▓▓▓▓░░░░░░░░▒▒▒▒▒▒░░▒▒░░░░░░▓▓░░          ░░▓▓▓▓██▒▒▒▒██▒▒▒▒▒▒▒▒▒▒▒▒▓▓▓▓▓▓░░▓▓    ",
"                  ██████▓▓░░░░░░░░░░▓▓████                      ████████▓▓████▓▓██              ",
"                          ██████████                                                              ",
"                          ░░░░  ░░                                                              ",
"                                                                                                ",
"                                                                                                ",
"                                                                                                ",
"                                                                                                ",
"  ░░░░                                                  ░░  ░░░░          ░░                    ",
"  ░░░░  ░░░░░░  ░░                                                      ░░░░                    ",
"  ░░░░░░░░░░░░░░░░░░░░ "
};
#define BAKE_ROWS (int)(sizeof(BAKE_ART)/sizeof(BAKE_ART[0]))

int effects_baking_bread_frames(void) { return 24; }

EffectFrame* effects_baking_bread_frame(int index, void *user) {
    (void)user;
    EffectFrame *f = effects_frame_new(BAKE_ROWS, 96);
    if (!f) return NULL;

    /* shimmer phase: 0..1 over the frame cycle */
    double t = (double)(index % effects_baking_bread_frames()) /
               (double)effects_baking_bread_frames();
    double wave = 0.5 + 0.5 * sin(t * 6.2831853);

    for (int r = 0; r < BAKE_ROWS; r++) {
        const char *src = BAKE_ART[r];
        size_t len = strlen(src);
        char *out = f->rows[r];
        size_t o = 0;
        size_t cap = 8192;
        for (size_t c = 0; c < len; c++) {
            if (src[c] == ' ') {
                if (o + 1 < cap) out[o++] = ' ';
                continue;
            }
            const char *cell = NULL;
            /* Block glyphs are 3-byte UTF-8 (0xE2 0x96/0x91 ...). Match by
               byte sequence; color solid blocks (█▓) and light blocks (▒░). */
            if (src[c] == (char)0xE2 && c + 2 < len) {
                unsigned char b1 = (unsigned char)src[c+1];
                unsigned char b2 = (unsigned char)src[c+2];
                if (b1 == 0x96 && (b2 == 0x88 || b2 == 0x93)) {        /* █ or ▓ */
                    if (wave > 0.5 && ((int)c % 7 < 3))
                        cell = retro_accent(b2 == 0x88 ? "█" : "▓");
                    else
                        cell = retro(b2 == 0x88 ? "█" : "▓");
                } else if ((b1 == 0x96 && (b2 == 0x92 || b2 == 0x91)) || /* ▒ ░ */
                           (b1 == 0x91 && b2 == 0x91)) {                /* ░ */
                    if (b1 == 0x96 && b2 == 0x92)
                        cell = retro_dim("▒");
                    else
                        cell = retro_dim("░");
                }
                if (cell) { c += 2; }
            }
            if (!cell) {
                /* ASCII / unexpected glyph: pass through uncolored */
                if (o + 1 < cap) out[o++] = src[c];
                continue;
            }
            size_t cl = strlen(cell);
            if (o + cl + 1 < cap) {
                memcpy(out + o, cell, cl);
                o += cl;
            }
        }
        out[o] = '\0';
    }
    return f;
}
