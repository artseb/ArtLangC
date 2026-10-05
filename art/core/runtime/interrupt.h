#ifndef ART_INTERRUPT_H
#define ART_INTERRUPT_H

#include <stdbool.h>

// ============================================================
// interrupt — Ctrl-C handling
//
// A process-wide SIGINT handler sets a flag. The interpreter
// checks the flag on hot paths (every node evaluation, parser
// loops, large table operations) and raises a runtime error
// when it's set. That routes through the normal longjmp
// machinery, so an infinite loop in user code unwinds cleanly
// instead of locking the machine.
//
// The handler does one thing: set a volatile flag. It's the
// safest thing a signal handler can do.
// ============================================================

// Install the SIGINT handler. Idempotent — safe to call more
// than once. Called from art_open.
void art_install_interrupt_handler(void);

// True if Ctrl-C has fired since the last clear. Checked by
// art_eval, the parser's top-level loop, and the table sort
// inner loop.
bool art_interrupt_pending(void);

// Reset the flag. Called when a fresh run starts, and when a
// pending interrupt has been consumed by a runtime error.
void art_clear_interrupt(void);

#endif // ART_INTERRUPT_H
