#include "enum.h"
#include "feature.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

static bool enum_eval_node(ArtState *S, Node *n, Value *out)
{
    if (n->type == NODE_ENUM_DECL)
    {
        *out = eval_enum_decl(S, n);
        return true;
    }
    return false;
}

static bool enum_member_get(ArtState *S, Value target, ObjString *name,
                            Node *at, Value *out)
{
    if (IS_ENUM(target))
    {
        *out = enum_get_member(S, AS_ENUM(target), name, at);
        return true;
    }
    if (IS_ENUM_VALUE(target))
    {
        *out = enum_value_get_member(S, AS_ENUM_VALUE(target), name, at);
        return true;
    }
    return false;
}

static bool enum_to_string(ArtState *S, Value v, ObjString **out)
{
    if (IS_ENUM(v))
    {
        *out = AS_ENUM(v)->name;
        return true;
    }
    if (IS_ENUM_VALUE(v))
    {
        ObjEnumValue *ev = AS_ENUM_VALUE(v);
        char *en = obj_string_to_utf8(ev->parent->name);
        char *mn = obj_string_to_utf8(ev->name);
        *out = obj_string_from_fmt(S, "%s.%s", en, mn);
        free(en);
        free(mn);
        return true;
    }
    return false;
}

static void enum_register_stmts(void)
{
    parser_register_stmt(TOKEN_ENUM, enum_parse);
}

static bool enum_binop(ArtState *S, Node *at, TokenType op,
                       Value a, Value b, Value *out)
{
    (void)S; (void)at;
    if (op != TOKEN_IN) return false;
    if (!IS_ENUM(b)) return false;

    ObjEnum *e = AS_ENUM(b);
    for (int i = 0; i < e->member_count; i++)
    {
        if (value_equal(OBJ_VAL(e->ordered[i]), a))
        {
            *out = BOOL_VAL(true);
            return true;
        }
    }
    *out = BOOL_VAL(false);
    return true;
}

Feature enum_feature = {
    .name = "enum",
    .register_stmts = enum_register_stmts,
    .free_children = enum_free_children,
    .node_name = enum_node_name,
    .gc_blacken = enum_gc_blacken,
    .gc_free_buffers = enum_gc_free_buffers,
    .eval_node = enum_eval_node,
    .member_get = enum_member_get,
    .to_string = enum_to_string,
    .binop = enum_binop,
};
