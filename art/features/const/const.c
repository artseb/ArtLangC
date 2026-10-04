// ============================================================
// const.c — the const modifier
// ============================================================

#include "const.h"
#include "feature.h"
#include "parser.h"

bool const_try_match(Parser *P)
{
    return parser_match(P, TOKEN_CONST);
}

void art_scope_declare_const(ArtState *S, ObjScope *scope,
                             ObjString *name, Value value)
{
    table_set(S, scope->vars, name, value);
    table_set(S, scope->consts, name, BOOL_VAL(true));
}

Feature const_feature = {
    .name = "const",
};
