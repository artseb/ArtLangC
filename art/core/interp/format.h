#ifndef ART_FORMAT_H
#define ART_FORMAT_H

#include "value.h"
#include "state.h"

// ============================================================
// format — the format-spec mini-language inside `${expr:spec}`
//
// String interpolation is ART's one and only way to build
// formatted text. There is no String.format method. A spec
// following a colon inside `${...}` is parsed here and applied
// to the value the expression produced.
//
// Spec grammar (Python-style):
//
//   [[fill]align][sign][#][0][width][.precision][type]
//
//   fill       any single character, default space
//   align      < left, > right, ^ center
//   sign       + always show, - only negatives, space for positives
//   #          alternate form: 0x/0X prefix for hex, 0o for octal
//   0          zero-pad numbers
//   width      minimum field width
//   .prec      decimal places for floats, max chars for strings
//   type       d i x X o f e g s c
//
// Default alignment: strings left, numbers right. When a fill
// character is given, it must come immediately before the align
// character.
//
// If the type is omitted, the default is chosen from the value:
// int -> 'd', float -> 'g', everything else -> 's'.
//
// Examples:
//   "${x}"          tostring(x), no padding
//   "${x:5}"        right-aligned to width 5
//   "${x:<5}"       left-aligned to width 5
//   "${x:05}"       zero-padded to width 5
//   "${x:*^9}"      centered, filled with '*'
//   "${x:+.2f}"     signed float, 2 decimals
//   "${x:#x}"       hex with 0x prefix
//   "${name:>20}"   right-aligned string
//   "${s:.3}"       first 3 chars
// ============================================================

// Format `v` according to `spec`. Never returns NULL — raises a
// runtime error on invalid specs and unknown types.
ObjString *format_value(ArtState *S, Value v, ObjString *spec);

#endif // ART_FORMAT_H