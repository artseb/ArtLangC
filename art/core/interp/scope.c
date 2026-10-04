// ============================================================
// scope.c — scope chain operations
// ============================================================

#include <stdlib.h>
#include "scope.h"
#include "state.h"

ObjScope *obj_scope_new(ArtState *S, ObjScope *parent)
{
    ObjScope *sc = ALLOCATE_OBJ(S, ObjScope, OBJ_SCOPE);
    sc->parent = parent;

    GC_PUSH(S, OBJ_VAL(sc));
    sc->vars = obj_table_new(S);
    sc->consts = obj_table_new(S);
    GC_POP(S, 1);

    return sc;
}

void art_scope_push(ArtState *S)
{
    S->scope = obj_scope_new(S, S->scope);
}

void art_scope_pop(ArtState *S)
{
    if (S->scope == NULL)
        return;
    if (S->scope == S->global_scope)
        return;
    S->scope = S->scope->parent;
}

void art_scope_declare(ArtState *S, ObjScope *scope,
                       ObjString *name, Value value)
{
    table_set(S, scope->vars, name, value);
}

bool art_scope_lookup(ObjScope *start, ObjString *name,
                      ObjScope **out_scope, Value *out_value)
{
    for (ObjScope *sc = start; sc != NULL; sc = sc->parent)
    {
        if (table_has(sc->vars, name))
        {
            if (out_scope)
                *out_scope = sc;
            if (out_value)
                *out_value = table_get(sc->vars, name);
            return true;
        }
    }
    return false;
}

bool art_scope_assign(ArtState *S, ObjScope *start, ObjString *name,
                      Value value, bool create_if_missing)
{
    for (ObjScope *sc = start; sc != NULL; sc = sc->parent)
    {
        if (table_has(sc->vars, name))
        {
            if (table_has(sc->consts, name))
                return false;
            table_set(S, sc->vars, name, value);
            return true;
        }
    }
    if (create_if_missing)
    {
        table_set(S, S->global_scope->vars, name, value);
        return true;
    }
    return false;
}
