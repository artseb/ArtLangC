#ifndef ART_FUNCTION_H
#define ART_FUNCTION_H

#include "value.h"
#include "state.h"

typedef struct Feature Feature;
extern Feature function_feature;

int function_min_arity(ObjFunction *fn);

bool function_accepts_arity(ObjFunction *fn, int argc);

#endif // ART_FUNCTION_H
