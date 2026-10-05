#ifndef ART_PATTERN_H
#define ART_PATTERN_H

#include <stdint.h>
#include <stdbool.h>
#include "state.h"

// ============================================================
// pattern — Lua-style pattern matching
//
// A pattern is a small text-matching language, separate from
// regex. Syntax:
//
//   %a %c %d %g %l %p %s %u %w %x    character classes
//   %A %C %D %G %L %P %S %U %W %X    complements
//   %<punct>                          escaped literal
//   .                                 any single character
//   [set] [^set] [a-z] [%a%d]         sets, ranges, negation
//   *  +  -  ?                        quantifiers (greedy, lazy)
//   ^  $                              anchors
//   (...)  ()                         substring / position captures
//   %1..%9                            back-references
//   %bxy                              balanced match
//   %f[set]                           frontier
//
// NOTE: this shares syntax with the format-string language in
// String.format, but means different things. `%s` in a pattern
// matches a whitespace character; `%s` in a format string
// substitutes the next argument. Same glyph, unrelated meanings.
//
// The engine works on UTF-16 code units. Non-ASCII characters
// match literally and match `%A` (the complement), but do not
// match `%a` / `%d` / `%w` — those checks are ASCII-only, same
// as Lua.
// ============================================================

#define PAT_MAX_CAPTURES 32

// One capture within a match.
//
//   len >= 0   substring capture, offset and unit count
//   len == -2  position capture (written `()` in the pattern),
//              `start` is the 0-based offset
typedef struct
{
    int start;
    int len;
} PatternCap;

// Result of a successful match.
typedef struct
{
    int match_start; // 0-based offset of the whole match
    int match_len;   // 0 if the pattern matched empty
    int ncaptures;
    PatternCap caps[PAT_MAX_CAPTURES];
} PatternMatch;

// Scan `src` for `pat`, starting at offset `init`.
//
// Returns true if a match was found, filling `out`. Returns
// false if the pattern didn't match anywhere, leaving `out`
// uninitialized.
//
// `init` is 0-based; pass 0 to scan from the start. Out-of-range
// values are clamped.
//
// On malformed patterns (unbalanced brackets, dangling %, bad
// back-reference), raises a runtime error through `S`.
bool pattern_match(ArtState *S,
                   const uint16_t *src, int src_len,
                   const uint16_t *pat, int pat_len,
                   int init,
                   PatternMatch *out);

#endif // ART
