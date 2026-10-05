// POSIX.1-2008 + XSI (_XOPEN_SOURCE 700) for strdup, realpath, nanosleep. Under -std=c11 glibc hides
// them, and an implicit strdup() returns int, truncating the pointer on
// 64-bit. Must come before the first system include.
#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif

// ============================================================
// time_builtins.c — the Time namespace
//
//   Time.now()      -> float, seconds since Unix epoch
//   Time.clock()    -> float, seconds of CPU time (monotonic)
//   Time.sleep(ms)  -> nil, blocks for `ms` milliseconds
//
// now()   uses timespec_get (C11), sub-second precision.
// clock() uses clock() and CLOCKS_PER_SEC, coarse but monotonic.
// sleep() uses nanosleep, which POSIX and MinGW provide.
// ============================================================

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "time.h"
#include "feature.h"
#include "interp.h"
#include "gc.h"
#include "register.h"

// MSVC has no nanosleep. Sleep() from windows.h takes
// milliseconds, so sub-millisecond requests round up. That's
// what the Windows scheduler delivers on real hardware anyway,
// so the practical behavior matches the POSIX build.
#ifdef _MSC_VER
#include <windows.h>
static int art_win_nanosleep(const struct timespec *req,
                             struct timespec *rem)
{
    (void)rem;
    long ms = (long)(req->tv_sec * 1000 + req->tv_nsec / 1000000);
    if (ms > 0)
        Sleep((DWORD)ms);
    return 0;
}
#define nanosleep art_win_nanosleep
#endif

static Value time_now(ArtState *S, int argc, Value *argv)
{
    (void)S;
    (void)argc;
    (void)argv;
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return FLOAT_VAL((double)ts.tv_sec + (double)ts.tv_nsec / 1e9);
}

static Value time_clock(ArtState *S, int argc, Value *argv)
{
    (void)S;
    (void)argc;
    (void)argv;
    clock_t c = clock();
    return FLOAT_VAL((double)c / (double)CLOCKS_PER_SEC);
}

static Value time_sleep(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    if (!IS_NUMBER(argv[0]))
        art_runtime_error(S, S->current_node,
                          "Time.sleep expects a number of milliseconds");

    double ms = AS_NUMBER(argv[0]);
    if (ms < 0)
        ms = 0;

    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000.0);
    ts.tv_nsec = (long)((ms - (double)ts.tv_sec * 1000.0) * 1e6);
    nanosleep(&ts, NULL);
    return NIL_VAL;
}

static void time_register_builtins(ArtState *S)
{
    ObjTable *time = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(time));

    art_define_native(S, time, "now", time_now, 0);
    art_define_native(S, time, "clock", time_clock, 0);
    art_define_native(S, time, "sleep", time_sleep, 1);

    time->frozen = true;

    art_define_global(S, "Time", OBJ_VAL(time));

    GC_POP(S, 1);
}

Feature time_feature = {
    .name = "time",
    .register_builtins = time_register_builtins,
};
