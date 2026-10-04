#ifndef ART_SCOPE_H
#define ART_SCOPE_H

#include <stdbool.h>
#include "value.h"
#include "state.h"

void art_scope_push(ArtState *S);

void art_scope_pop(ArtState *S);

void art_scope_declare(ArtState *S, ObjScope *scope,
                       ObjString *name, Value value);

bool art_scope_lookup(ObjScope *start, ObjString *name,
                      ObjScope **out_scope, Value *out_value);

bool art_scope_assign(ArtState *S, ObjScope *start, ObjString *name,
                      Value value, bool create_if_missing);

#endif // ART_SCOPE_H
