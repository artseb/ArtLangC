#ifndef ART_NUMBER_H
#define ART_NUMBER_H

#include "value.h"
#include "state.h"

// ============================================================
// number — arithmetic and comparison semantics
//
// Owns the arithmetic (+, -, *, /, %) and comparison
// (<, <=, >, >=) semantics for int and float operands.
// Registered as a binop feature hook.
//
// Locked decisions:
//   int / int is floor division
//   int % int is floored
//   int op int stays int; any float promotes both operands
//
// Equality is NOT claimed here — core's value_equal handles it.
// ============================================================

typedef struct Feature Feature;
extern Feature number_feature;

#endif // ART_NUMBER_H
