// ============================================================
// interface.c — the interface feature, in one file
//
// Sections, in order: ast, parse, eval, runtime, gc, feature.
// ============================================================

#include "interface.h"
#include "parser.h"
#include "interp.h"
#include "scope.h"
#include "gc.h"
#include "feature.h"
#include <stdlib.h>
#include <string.h>

// ---------------- ast ----------------

static ObjString **copy_names(ObjString **src, int n)
{
    if (n == 0)
        return NULL;
    ObjString **dst = malloc(sizeof(ObjString *) * n);
    memcpy(dst, src, sizeof(ObjString *) * n);
    return dst;
}

Node *node_interface_decl(int line, int col,
                          ObjString *name,
                          ObjString **parents, int parent_count,
                          ObjString **method_names, int *method_arities,
                          bool *method_variadics,
                          int method_count,
                          ObjString **getter_names, int getter_count,
                          ObjString **setter_names, int setter_count)
{
    InterfaceDeclNode *n = (InterfaceDeclNode *)ast_alloc_node(
        sizeof(InterfaceDeclNode), NODE_INTERFACE_DECL, line, col, false);

    n->name = name;
    n->parents = copy_names(parents, parent_count);
    n->parent_count = parent_count;

    n->method_names = copy_names(method_names, method_count);
    n->method_arities = NULL;
    n->method_variadics = NULL;
    if (method_count > 0)
    {
        n->method_arities = malloc(sizeof(int) * method_count);
        memcpy(n->method_arities, method_arities,
               sizeof(int) * method_count);
        n->method_variadics = malloc(sizeof(bool) * method_count);
        memcpy(n->method_variadics, method_variadics,
               sizeof(bool) * method_count);
    }
    n->method_count = method_count;

    n->getter_names = copy_names(getter_names, getter_count);
    n->getter_count = getter_count;

    n->setter_names = copy_names(setter_names, setter_count);
    n->setter_count = setter_count;

    return (Node *)n;
}

bool interface_free_children(Node *n)
{
    if (n->type != NODE_INTERFACE_DECL)
        return false;

    InterfaceDeclNode *in = (InterfaceDeclNode *)n;
    free(in->parents);
    free(in->method_names);
    free(in->method_arities);
    free(in->method_variadics);
    free(in->getter_names);
    free(in->setter_names);
    return true;
}

const char *interface_node_name(NodeType t)
{
    if (t == NODE_INTERFACE_DECL)
        return "interface_decl";
    return NULL;
}

// ---------------- parse ----------------

Node *interface_parse(Parser *P)
{
    Token iface_tok = P->previous;
    Token name_tok = parser_consume(P, TOKEN_IDENT,
                                    "expected interface name");
    ObjString *name = parser_token_to_string(P, &name_tok);

    int pcap = 4, pcount = 0;
    ObjString **parents = NULL;

    if (parser_match(P, TOKEN_EXTENDS))
    {
        parents = malloc(sizeof(ObjString *) * pcap);
        for (;;)
        {
            Token t = parser_consume(P, TOKEN_IDENT,
                                     "expected parent interface name");
            if (pcount >= pcap)
            {
                pcap *= 2;
                parents = realloc(parents, sizeof(ObjString *) * pcap);
            }
            parents[pcount++] = parser_token_to_string(P, &t);
            if (!parser_match(P, TOKEN_COMMA))
                break;
        }
    }

    parser_consume(P, TOKEN_LEFT_BRACE,
                   "expected '{' after interface header");

    int mcap = 8, mcount = 0;
    ObjString **mnames = malloc(sizeof(ObjString *) * mcap);
    int *marity = malloc(sizeof(int) * mcap);
    bool *mvariadic = malloc(sizeof(bool) * mcap);

    int gcap = 4, gcount = 0;
    ObjString **gnames = malloc(sizeof(ObjString *) * gcap);

    int scap = 4, scount = 0;
    ObjString **snames = malloc(sizeof(ObjString *) * scap);

    while (!parser_check(P, TOKEN_RIGHT_BRACE) &&
           !parser_check(P, TOKEN_EOF))
    {
        if (parser_match(P, TOKEN_FUN))
        {
            Token mt = parser_consume(P, TOKEN_IDENT,
                                      "expected method name");
            ObjString *mn = parser_token_to_string(P, &mt);

            parser_consume(P, TOKEN_LEFT_PAREN, "expected '('");

            int arity = 0;
            bool variadic = false;

            if (!parser_check(P, TOKEN_RIGHT_PAREN))
            {
                for (;;)
                {
                    if (parser_match(P, TOKEN_DOT_DOT_DOT))
                    {
                        parser_consume(P, TOKEN_IDENT,
                                       "expected name after '...'");
                        variadic = true;
                        break;
                    }

                    parser_consume(P, TOKEN_IDENT, "expected parameter");
                    arity++;
                    if (parser_match(P, TOKEN_COLON))
                        parser_consume(P, TOKEN_IDENT,
                                       "expected type name");
                    if (parser_match(P, TOKEN_EQUAL))
                        parse_expression(P);
                    if (!parser_match(P, TOKEN_COMMA))
                        break;
                }
            }
            parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')'");

            if (mcount >= mcap)
            {
                mcap *= 2;
                mnames = realloc(mnames, sizeof(ObjString *) * mcap);
                marity = realloc(marity, sizeof(int) * mcap);
                mvariadic = realloc(mvariadic, sizeof(bool) * mcap);
            }
            mnames[mcount] = mn;
            marity[mcount] = arity;
            mvariadic[mcount] = variadic;
            mcount++;
        }
        else if (parser_match(P, TOKEN_GET))
        {
            Token gt = parser_consume(P, TOKEN_IDENT, "expected name");
            if (gcount >= gcap)
            {
                gcap *= 2;
                gnames = realloc(gnames, sizeof(ObjString *) * gcap);
            }
            gnames[gcount++] = parser_token_to_string(P, &gt);
        }
        else if (parser_match(P, TOKEN_SET))
        {
            Token st = parser_consume(P, TOKEN_IDENT, "expected name");
            parser_consume(P, TOKEN_LEFT_PAREN, "expected '('");
            parser_consume(P, TOKEN_IDENT, "expected parameter");
            parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')'");
            if (scount >= scap)
            {
                scap *= 2;
                snames = realloc(snames, sizeof(ObjString *) * scap);
            }
            snames[scount++] = parser_token_to_string(P, &st);
        }
        else
        {
            parser_error(P, "expected 'fun', 'get', 'set', or '}'");
            parser_synchronize(P);
        }
    }

    parser_consume(P, TOKEN_RIGHT_BRACE, "expected '}' to close interface");

    Node *result = node_interface_decl(
        iface_tok.line, iface_tok.column,
        name,
        parents, pcount,
        mnames, marity, mvariadic, mcount,
        gnames, gcount,
        snames, scount);

    free(parents);
    free(mnames);
    free(marity);
    free(mvariadic);
    free(gnames);
    free(snames);
    return result;
}

// ---------------- eval ----------------

static bool class_has_method(ObjClass *klass, ObjString *name,
                             int arity, bool is_variadic)
{
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        Value v = table_get(c->methods, name);
        if (!IS_TABLE(v))
            continue;

        ObjTable *ov = AS_TABLE(v);
        for (int i = 0; i < ov->array_count; i++)
        {
            ObjClosure *cl = AS_CLOSURE(ov->array[i]);
            ObjFunction *fn = cl->function;

            if (is_variadic)
            {
                if (fn->is_variadic && function_min_arity(fn) <= arity)
                    return true;
            }
            else
            {
                if (function_accepts_arity(fn, arity))
                    return true;
            }
        }
    }
    return false;
}

static bool class_has_getter(ObjClass *klass, ObjString *name)
{
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        if (table_has(c->getters, name))
            return true;
        for (int i = 0; i < c->field_count; i++)
            if (c->fields[i].name == name)
                return true;
    }
    return false;
}

static bool class_has_setter(ObjClass *klass, ObjString *name)
{
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        if (table_has(c->setters, name))
            return true;
        for (int i = 0; i < c->field_count; i++)
            if (c->fields[i].name == name)
                return true;
    }
    return false;
}

static int missing_from_one(ObjClass *klass, ObjInterface *iface,
                            ObjString **missing, int missing_cap,
                            int found)
{
    for (int i = 0; i < iface->method_count; i++)
    {
        if (!class_has_method(klass, iface->method_names[i],
                              iface->method_arities[i],
                              iface->method_variadics[i]))
        {
            if (found < missing_cap)
                missing[found] = iface->method_names[i];
            found++;
        }
    }

    for (int i = 0; i < iface->getter_count; i++)
    {
        if (!class_has_getter(klass, iface->getter_names[i]))
        {
            if (found < missing_cap)
                missing[found] = iface->getter_names[i];
            found++;
        }
    }

    for (int i = 0; i < iface->setter_count; i++)
    {
        if (!class_has_setter(klass, iface->setter_names[i]))
        {
            if (found < missing_cap)
                missing[found] = iface->setter_names[i];
            found++;
        }
    }

    for (int i = 0; i < iface->parent_count; i++)
    {
        found = missing_from_one(klass, iface->parents[i],
                                 missing, missing_cap, found);
    }

    return found;
}

int interface_missing_members(ObjClass *klass, ObjInterface *iface,
                              ObjString **missing, int missing_cap)
{
    return missing_from_one(klass, iface, missing, missing_cap, 0);
}

Value eval_interface_decl(ArtState *S, Node *n)
{
    InterfaceDeclNode *d = (InterfaceDeclNode *)n;

    ObjInterface *iface = obj_interface_new(S, d->name);
    GC_PUSH(S, OBJ_VAL(iface));

    if (d->parent_count > 0)
    {
        iface->parents = malloc(sizeof(ObjInterface *) * d->parent_count);
        for (int i = 0; i < d->parent_count; i++)
        {
            Value pv;
            if (!art_scope_lookup(S->scope, d->parents[i], NULL, &pv) ||
                !IS_INTERFACE(pv))
            {
                art_runtime_error(S, n, "unknown parent interface '%s'",
                                  obj_string_to_utf8(d->parents[i]));
            }
            iface->parents[i] = AS_INTERFACE(pv);

            if (interface_satisfies(iface->parents[i], iface))
                art_runtime_error(S, n, "circular interface inheritance");
        }
        iface->parent_count = d->parent_count;
    }

    if (d->method_count > 0)
    {
        iface->method_names = malloc(sizeof(ObjString *) * d->method_count);
        iface->method_arities = malloc(sizeof(int) * d->method_count);
        iface->method_variadics = malloc(sizeof(bool) * d->method_count);
        for (int i = 0; i < d->method_count; i++)
        {
            iface->method_names[i] = d->method_names[i];
            iface->method_arities[i] = d->method_arities[i];
            iface->method_variadics[i] = d->method_variadics[i];
        }
        iface->method_count = d->method_count;
    }

    if (d->getter_count > 0)
    {
        iface->getter_names = malloc(sizeof(ObjString *) * d->getter_count);
        for (int i = 0; i < d->getter_count; i++)
            iface->getter_names[i] = d->getter_names[i];
        iface->getter_count = d->getter_count;
    }

    if (d->setter_count > 0)
    {
        iface->setter_names = malloc(sizeof(ObjString *) * d->setter_count);
        for (int i = 0; i < d->setter_count; i++)
            iface->setter_names[i] = d->setter_names[i];
        iface->setter_count = d->setter_count;
    }

    art_scope_declare(S, S->scope, d->name, OBJ_VAL(iface));

    GC_POP(S, 1);
    return OBJ_VAL(iface);
}

// ---------------- runtime ----------------

ObjInterface *obj_interface_new(ArtState *S, ObjString *name)
{
    ObjInterface *iface = ALLOCATE_OBJ(S, ObjInterface, OBJ_INTERFACE);
    iface->name = name;
    iface->parents = NULL;
    iface->parent_count = 0;
    iface->method_names = NULL;
    iface->method_arities = NULL;
    iface->method_variadics = NULL;
    iface->method_count = 0;
    iface->getter_names = NULL;
    iface->getter_count = 0;
    iface->setter_names = NULL;
    iface->setter_count = 0;
    return iface;
}

// ---------------- gc ----------------

bool interface_gc_blacken(ArtState *S, Obj *o)
{
    if (o->type != OBJ_INTERFACE)
        return false;

    ObjInterface *iface = (ObjInterface *)o;
    art_gc_mark_object(S, (Obj *)iface->name);

    for (int i = 0; i < iface->parent_count; i++)
        art_gc_mark_object(S, (Obj *)iface->parents[i]);
    for (int i = 0; i < iface->method_count; i++)
        art_gc_mark_object(S, (Obj *)iface->method_names[i]);
    for (int i = 0; i < iface->getter_count; i++)
        art_gc_mark_object(S, (Obj *)iface->getter_names[i]);
    for (int i = 0; i < iface->setter_count; i++)
        art_gc_mark_object(S, (Obj *)iface->setter_names[i]);

    return true;
}

bool interface_gc_free_buffers(ArtState *S, Obj *o)
{
    (void)S;
    if (o->type != OBJ_INTERFACE)
        return false;

    ObjInterface *iface = (ObjInterface *)o;
    free(iface->parents);
    free(iface->method_names);
    free(iface->method_arities);
    free(iface->method_variadics);
    free(iface->getter_names);
    free(iface->setter_names);
    return true;
}

// ---------------- feature ----------------

static bool interface_eval_node(ArtState *S, Node *n, Value *out)
{
    if (n->type == NODE_INTERFACE_DECL)
    {
        *out = eval_interface_decl(S, n);
        return true;
    }
    return false;
}

static void interface_register_stmts(void)
{
    parser_register_stmt(TOKEN_INTERFACE, interface_parse);
}

Feature interface_feature = {
    .name = "interface",
    .register_stmts = interface_register_stmts,
    .free_children = interface_free_children,
    .node_name = interface_node_name,
    .gc_blacken = interface_gc_blacken,
    .gc_free_buffers = interface_gc_free_buffers,
    .eval_node = interface_eval_node,
};
