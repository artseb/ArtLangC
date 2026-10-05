// ============================================================
// interrupt.c — Ctrl-C handling
//
// One global flag, one handler that sets it. Every ArtState in
// the process shares it, which is what you want: Ctrl-C means
// "stop what I'm doing", not "stop the state I happened to be
// running when you pressed it".
// ============================================================

#include <signal.h>
#include "interrupt.h"

static volatile sig_atomic_t g_interrupt = 0;

static void on_sigint(int sig)
{
    (void)sig;
    g_interrupt = 1;
}

void art_install_interrupt_handler(void)
{
    // signal() rather than sigaction() for portability. The
    // handler only writes to a volatile sig_atomic_t, which is
    // safe on every platform we care about. sigaction's
    // SA_RESTART behavior is not something we need.
    signal(SIGINT, on_sigint);
}

bool art_interrupt_pending(void)
{
    return g_interrupt != 0;
}

void art_clear_interrupt(void)
{
    g_interrupt = 0;
}
