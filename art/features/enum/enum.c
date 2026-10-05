// ============================================================
// enum.c — the enum feature, in one file
//
// Sections, in order: ast, parse, eval, runtime, gc, feature.
// ============================================================

#include "enum.h"
#include "parser.h"
#include "interp.h"
#include "scope.h"
#include "gc.h"
#include "feature.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ---------------- ast ----------------

// ============================================================
// enum_ast.c — enum AST constructors + free + node names
// ============================================================



Node *node_enum_decl(int line, int col, ObjString *name,
                     Node **members, int member_count)
{
    EnumDeclNode *n = (EnumDeclNode *)ast_alloc_node(
        sizeof(EnumDeclNode), NODE_ENUM_DECL, line, col, false);
    n->name = name;
    n->members = ast_copy_node_array(members, member_count);
    n->member_count = member_count;
    return (Node *)n;
}

Node *node_enum_member(int line, int col, ObjString *name, Node *value)
{
    EnumMemberNode *n = (EnumMemberNode *)ast_alloc_node(
        sizeof(EnumMemberNode), NODE_ENUM_MEMBER, line, col, false);
    n->name = name;
    n->value = value;
    return (Node *)n;
}

bool enum_free_children(Node *n)
{
    switch (n->type)
    {
    case NODE_ENUM_DECL:
    {
        EnumDeclNode *e = (EnumDeclNode *)n;
        for (int i = 0; i < e->member_count; i++)
            node_free_tree(&e->members[i]);
        free(e->members);
        return true;
    }
    case NODE_ENUM_MEMBER:
    {
        EnumMemberNode *m = (EnumMemberNode *)n;
        if (m->value)
            node_free_tree(&m->value);
        return true;
    }
    default:
        return false;
    }
}

const char *enum_node_name(NodeType t)
{
    switch (t)
    {
    case NODE_ENUM_DECL:   return "enum_decl";
    case NODE_ENUM_MEMBER: return "enum_member";
    default:               return NULL;
    }
}

// ---------------- parse ----------------

// ============================================================
// enum_parse.c — parse enum declarations
//
// An enum body contains only member names, optionally with
// `= <value>`:
//
//     enum Color { Red, Green, Blue }
//     enum Http  { OK = 200, NotFound = 404 }
//
// Anything else — `fun`, `class`, `interface`, a bare `{` —
// is a syntax error. The parser reports it once and skips the
// rest of the enum body, which keeps a bad declaration from
// spinning the loop on a token it can't consume.
// ============================================================



Node *enum_parse(Parser *P)
{
    Token enum_tok = P->previous;
    Token name_tok = parser_consume(P, TOKEN_IDENT,
                                    "expected enum name after 'enum'");
    ObjString *name = parser_token_to_string(P, &name_tok);

    parser_consume(P, TOKEN_LEFT_BRACE, "expected '{' after enum name");

    int cap = 8, count = 0;
    Node **members = malloc(sizeof(Node *) * cap);

    while (!parser_check(P, TOKEN_RIGHT_BRACE) &&
           !parser_check(P, TOKEN_EOF))
    {
        // Enum bodies accept only member names. Anything else is a
        // syntax error. Before reporting, check for the constructs
        // a user is most likely to have meant (a method or nested
        // type) so the message names the problem.
        if (!parser_check(P, TOKEN_IDENT))
        {
            if (parser_check(P, TOKEN_FUN) ||
                parser_check(P, TOKEN_CLASS) ||
                parser_check(P, TOKEN_INTERFACE))
            {
                parser_error(P, "enums contain only member names; "
                                "'%s' is not allowed in an enum body",
                              token_type_name(P->current.type));
            }
            else
            {
                parser_error(P, "expected enum member name");
            }

            // Skip the rest of the enum. Once the body is malformed
            // we can't trust anything inside it, and bailing out
            // guarantees the loop terminates on any input.
            //
            // Brace-count so a nested block (like a method body)
            // doesn't fool us into stopping on its own `}`.
            int depth = 0;
            while (!parser_check(P, TOKEN_EOF))
            {
                if (parser_check(P, TOKEN_LEFT_BRACE))
                    depth++;
                else if (parser_check(P, TOKEN_RIGHT_BRACE))
                {
                    if (depth == 0)
                        break;
                    depth--;
                }
                parser_advance(P);
            }
            break;
        }

        Token m_tok = P->current;
        parser_advance(P);
        ObjString *m_name = parser_token_to_string(P, &m_tok);

        Node *m_value = NULL;
        if (parser_match(P, TOKEN_EQUAL))
        {
            m_value = parse_expression(P);
            if (m_value == NULL)
            {
                for (int i = 0; i < count; i++)
                    node_free_tree(&members[i]);
                free(members);
                return NULL;
            }
        }

        if (count >= cap)
        {
            cap *= 2;
            members = realloc(members, sizeof(Node *) * cap);
        }
        members[count++] = node_enum_member(m_tok.line, m_tok.column,
                                            m_name, m_value);

        parser_match(P, TOKEN_COMMA);
    }

    parser_consume(P, TOKEN_RIGHT_BRACE, "expected '}' after enum body");

    Node *result = node_enum_decl(enum_tok.line, enum_tok.column,
                                  name, members, count);
    free(members);
    return result;
}

// ---------------- eval ----------------

static void define_enum_native(ArtState *S, ObjEnum *e, const char *name,
                               NativeFn fn, int arity)
{
    ObjString *n = obj_string_from_utf8(S, name, (int)strlen(name));
    GC_PUSH(S, OBJ_VAL(n));
    ObjNative *nat = obj_native_new(S, fn, n, arity);
    GC_POP(S, 1);

    GC_PUSH(S, OBJ_VAL(nat));
    table_set(S, e->methods, n, OBJ_VAL(nat));
    GC_POP(S, 1);
}

static ObjEnum *this_enum(ArtState *S)
{
    ObjString *tn = obj_string_from_utf8(S, "this", 4);
    Value v;
    if (!art_scope_lookup(S->scope, tn, NULL, &v))
        return NULL;
    if (!IS_ENUM(v))
        return NULL;
    return AS_ENUM(v);
}

static Value enum_values(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    ObjEnum *e = this_enum(S);
    if (e == NULL) return NIL_VAL;

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));
    for (int i = 0; i < e->member_count; i++)
        table_push(S, out, OBJ_VAL(e->ordered[i]));
    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value enum_names(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    ObjEnum *e = this_enum(S);
    if (e == NULL) return NIL_VAL;

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));
    for (int i = 0; i < e->member_count; i++)
        table_push(S, out, OBJ_VAL(e->ordered[i]->name));
    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value enum_from_name(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    ObjEnum *e = this_enum(S);
    if (e == NULL) return NIL_VAL;
    if (!IS_STRING(argv[0])) return NIL_VAL;
    if (!table_has(e->members, AS_STRING(argv[0]))) return NIL_VAL;
    return table_get(e->members, AS_STRING(argv[0]));
}

static Value enum_from_value(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    ObjEnum *e = this_enum(S);
    if (e == NULL) return NIL_VAL;
    for (int i = 0; i < e->member_count; i++)
    {
        if (value_equal(e->ordered[i]->value, argv[0]))
            return OBJ_VAL(e->ordered[i]);
    }
    return NIL_VAL;
}

Value eval_enum_decl(ArtState *S, Node *n)
{
    EnumDeclNode *d = (EnumDeclNode *)n;

    ObjEnum *e = obj_enum_new(S, d->name);
    GC_PUSH(S, OBJ_VAL(e));

    if (d->member_count > 0)
        e->ordered = malloc(sizeof(ObjEnumValue *) * d->member_count);

    Value first_val = NIL_VAL;
    bool have_type = false;

    for (int i = 0; i < d->member_count; i++)
    {
        EnumMemberNode *m = (EnumMemberNode *)d->members[i];

        Value v = NIL_VAL;
        if (m->value != NULL)
        {
            v = art_eval(S, m->value);
            if (S->control != CONTROL_NONE) { GC_POP(S, 1); return NIL_VAL; }

            if (!have_type)
            {
                first_val = v;
                have_type = true;
            }
            else if (v.type != first_val.type)
            {
                art_runtime_error(S, m->value,
                                  "enum member '%s' has type %s, expected %s",
                                  obj_string_to_utf8(m->name),
                                  value_type_name(v),
                                  value_type_name(first_val));
            }
        }

        ObjEnumValue *ev = obj_enum_value_new(S, e, m->name, v);
        GC_PUSH(S, OBJ_VAL(ev));

        table_set(S, e->members, m->name, OBJ_VAL(ev));
        e->ordered[e->member_count++] = ev;

        GC_POP(S, 1);
    }

    define_enum_native(S, e, "values", enum_values, 0);
    define_enum_native(S, e, "names", enum_names, 0);
    define_enum_native(S, e, "fromName", enum_from_name, 1);
    define_enum_native(S, e, "fromValue", enum_from_value, 1);

    art_scope_declare(S, S->scope, d->name, OBJ_VAL(e));

    GC_POP(S, 1);
    return OBJ_VAL(e);
}

Value enum_value_get_member(ArtState *S, ObjEnumValue *ev,
                            ObjString *name, Node *at)
{
    if (obj_string_eq_ascii(name, "name")) return OBJ_VAL(ev->name);
    if (obj_string_eq_ascii(name, "value")) return ev->value;

    art_runtime_error(S, at, "no member '%s' on enum value",
                      obj_string_to_utf8(name));
    return NIL_VAL;
}

Value enum_get_member(ArtState *S, ObjEnum *e, ObjString *name, Node *at)
{
    if (table_has(e->members, name))
        return table_get(e->members, name);

    if (table_has(e->methods, name))
    {
        Value v = table_get(e->methods, name);
        if (IS_NATIVE(v))
        {
            ObjBoundMethod *bm = obj_bound_method_new(S, OBJ_VAL(e), v);
            return OBJ_VAL(bm);
        }
        return v;
    }

    art_runtime_error(S, at, "no member '%s' on enum '%s'",
                      obj_string_to_utf8(name),
                      obj_string_to_utf8(e->name));
    return NIL_VAL;
}

// ---------------- runtime ----------------

ObjEnum *obj_enum_new(ArtState *S, ObjString *name)
{
    ObjEnum *e = ALLOCATE_OBJ(S, ObjEnum, OBJ_ENUM);
    GC_PUSH(S, OBJ_VAL(e));

    e->name = name;
    e->members = obj_table_new(S);
    e->methods = obj_table_new(S);
    e->ordered = NULL;
    e->member_count = 0;

    GC_POP(S, 1);
    return e;
}

ObjEnumValue *obj_enum_value_new(ArtState *S, ObjEnum *parent,
                                 ObjString *name, Value value)
{
    ObjEnumValue *ev = ALLOCATE_OBJ(S, ObjEnumValue, OBJ_ENUM_VALUE);
    ev->parent = parent;
    ev->name = name;
    ev->value = value;
    return ev;
}

// ---------------- gc ----------------

bool enum_gc_blacken(ArtState *S, Obj *o)
{
    switch (o->type)
    {
    case OBJ_ENUM:
    {
        ObjEnum *e = (ObjEnum *)o;
        art_gc_mark_object(S, (Obj *)e->name);
        art_gc_mark_object(S, (Obj *)e->members);
        art_gc_mark_object(S, (Obj *)e->methods);
        for (int i = 0; i < e->member_count; i++)
            art_gc_mark_object(S, (Obj *)e->ordered[i]);
        return true;
    }
    case OBJ_ENUM_VALUE:
    {
        ObjEnumValue *ev = (ObjEnumValue *)o;
        art_gc_mark_object(S, (Obj *)ev->parent);
        art_gc_mark_object(S, (Obj *)ev->name);
        art_gc_mark_value(S, ev->value);
        return true;
    }
    default:
        return false;
    }
}

bool enum_gc_free_buffers(ArtState *S, Obj *o)
{
    if (o->type != OBJ_ENUM) return false;
    (void)S;
    ObjEnum *e = (ObjEnum *)o;
    free(e->ordered);
    return true;
}

// ---------------- feature ----------------

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
