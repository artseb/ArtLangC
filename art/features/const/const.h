#ifndef ART_CONST_H
#define ART_CONST_H

#include "value.h"
#include "state.h"

typedef struct Parser Parser;

bool const_try_match(Parser *P);

void art_scope_declare_const(ArtState *S, ObjScope *scope,
                             ObjString *name, Value value);

typedef struct Feature Feature;
extern Feature const_feature;

#endif
