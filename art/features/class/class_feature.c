#include "class.h"
#include "feature.h"
#include "interp.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>

bool class_to_string(ArtState *S, Value v, ObjString **out);

static bool class_eval_node(ArtState *S, Node *n, Value *out)
{
    switch (n->type)
    {
    case NODE_CLASS_DECL:
        *out = eval_class_decl(S, n);
        return true;
    case NODE_THIS:
        *out = eval_this(S, n);
        return true;
    case NODE_SUPER:
        *out = eval_super(S, n);
        return true;
    default:
        return false;
    }
}

static bool class_pre_member_get(ArtState *S, MemberNode *m,
                                 Node *at, Value *out)
{
    if (m->target->type != NODE_SUPER)
        return false;

    if (S->active_class == NULL ||
        S->active_class->superclass == NULL)
    {
        art_runtime_error(S, at,
                          "'super' used outside of a subclass method");
    }

    ObjClass *parent = S->active_class->superclass;
    ObjClosure *cl = class_find_method_any(S, parent, m->name);
    if (cl == NULL)
        art_runtime_error(S, at, "no method '%s' on super",
                          obj_string_to_utf8(m->name));

    Value self = interp_this(S, at);
    ObjBoundMethod *bm = obj_bound_method_new(S, self, OBJ_VAL(cl));
    bm->start_class = parent;
    *out = OBJ_VAL(bm);
    return true;
}

static bool class_member_get(ArtState *S, Value target, ObjString *name,
                             Node *at, Value *out)
{
    if (IS_CLASS(target))
    {
        ObjClass *klass = AS_CLASS(target);

        for (ObjClass *c = klass; c != NULL; c = c->superclass)
        {
            Value v = table_get(c->static_methods, name);
            if (IS_TABLE(v))
            {
                ObjTable *ov = AS_TABLE(v);
                if (ov->array_count > 0)
                {
                    *out = ov->array[0];
                    return true;
                }
            }
            else if (IS_CLOSURE(v) || IS_NATIVE(v))
            {
                *out = v;
                return true;
            }

            Value sv = table_get(c->statics, name);
            if (!IS_NIL(sv))
            {
                *out = sv;
                return true;
            }
        }

        art_runtime_error(S, at, "no static member '%s' on %s",
                          obj_string_to_utf8(name),
                          obj_string_to_utf8(klass->name));
    }

    if (IS_INSTANCE(target))
    {
        *out = instance_get_member(S, AS_INSTANCE(target), name, at);
        return true;
    }

    return false;
}

static bool class_member_set(ArtState *S, Value target, ObjString *name,
                             Value value, Node *at, Value *out)
{
    if (IS_INSTANCE(target))
    {
        *out = instance_set_member(S, AS_INSTANCE(target), name, value, at);
        return true;
    }

    if (IS_CLASS(target))
    {
        ObjClass *klass = AS_CLASS(target);
        for (ObjClass *c = klass; c != NULL; c = c->superclass)
        {
            for (int i = 0; i < c->field_count; i++)
            {
                Field *f = &c->fields[i];
                if (!f->is_static || f->name != name)
                    continue;

                if (f->is_private && S->active_class != c)
                    art_runtime_error(S, at, "field '%s' is private",
                                      obj_string_to_utf8(name));
                if (f->is_const)
                    art_runtime_error(S, at, "field '%s' is const",
                                      obj_string_to_utf8(name));

                table_set(S, c->statics, name, value);
                *out = value;
                return true;
            }
        }

        art_runtime_error(S, at, "no static member '%s' on %s",
                          obj_string_to_utf8(name),
                          obj_string_to_utf8(klass->name));
    }

    return false;
}

static bool class_pre_call(ArtState *S, CallNode *c, Value *out)
{
    if (c->callee->type != NODE_SUPER)
        return false;

    if (S->active_class == NULL || S->active_class->superclass == NULL)
        art_runtime_error(S, c->callee,
                          "'super' used outside of a subclass method");

    if (c->arg_count > 64)
        art_runtime_error(S, c->callee, "too many arguments");

    Value args[64];
    for (int i = 0; i < c->arg_count; i++)
    {
        args[i] = art_eval(S, c->args[i]);
        if (S->control != CONTROL_NONE)
        {
            *out = NIL_VAL;
            return true;
        }
    }

    ObjClass *parent = S->active_class->superclass;
    ObjClosure *ctor = class_find_method(S, parent, parent->name,
                                         c->arg_count, args, c->callee);
    if (ctor == NULL)
        art_runtime_error(S, c->callee,
                          "no super constructor for %s with %d args",
                          obj_string_to_utf8(parent->name), c->arg_count);

    Value self = interp_this(S, c->callee);
    *out = call_method(S, ctor, self, c->arg_count, args, c->callee);
    return true;
}

static bool class_call(ArtState *S, Value callee, int argc, Value *args,
                       Node *at, Value *out)
{
    if (!IS_CLASS(callee)) return false;
    *out = class_instantiate(S, AS_CLASS(callee), argc, args, at);
    return true;
}

static bool is_commutative(TokenType op)
{
    return op == TOKEN_PLUS ||
           op == TOKEN_STAR ||
           op == TOKEN_EQUAL_EQUAL ||
           op == TOKEN_BANG_EQUAL;
}

static bool class_binop(ArtState *S, Node *at, TokenType op,
                        Value a, Value b, Value *out)
{
    if (op == TOKEN_AND || op == TOKEN_OR)
        return false;

    if (op == TOKEN_BANG_EQUAL)
    {
        Value args[1] = {b};
        if (IS_INSTANCE(a))
        {
            ObjClosure *eq = class_find_operator(
                S, AS_INSTANCE(a)->klass, TOKEN_EQUAL_EQUAL, 1, args);
            if (eq != NULL)
            {
                Value r = call_method(S, eq, a, 1, args, at);
                *out = BOOL_VAL(value_is_falsy(r));
                return true;
            }
        }
        if (IS_INSTANCE(b))
        {
            Value sargs[1] = {a};
            ObjClosure *eq = class_find_operator(
                S, AS_INSTANCE(b)->klass, TOKEN_EQUAL_EQUAL, 1, sargs);
            if (eq != NULL)
            {
                Value r = call_method(S, eq, b, 1, sargs, at);
                *out = BOOL_VAL(value_is_falsy(r));
                return true;
            }
        }
        return false;
    }

    if (IS_INSTANCE(a))
    {
        Value args[1] = {b};
        ObjClosure *ov = class_find_operator(S, AS_INSTANCE(a)->klass,
                                             op, 1, args);
        if (ov != NULL)
        {
            *out = call_method(S, ov, a, 1, args, at);
            return true;
        }
    }

    if (is_commutative(op) && IS_INSTANCE(b))
    {
        Value args[1] = {a};
        ObjClosure *ov = class_find_operator(S, AS_INSTANCE(b)->klass,
                                             op, 1, args);
        if (ov != NULL)
        {
            *out = call_method(S, ov, b, 1, args, at);
            return true;
        }
    }

    return false;
}

static bool class_unop(ArtState *S, Node *at, TokenType op, Value v,
                       Value *out)
{
    if (!IS_INSTANCE(v)) return false;
    if (op != TOKEN_MINUS) return false;

    ObjClosure *ov = class_find_operator(S, AS_INSTANCE(v)->klass,
                                         TOKEN_MINUS, 0, NULL);
    if (ov == NULL) return false;

    *out = call_method(S, ov, v, 0, NULL, at);
    return true;
}

static void class_register_stmts(void)
{
    parser_register_stmt(TOKEN_CLASS, class_parse);
}

Feature class_feature = {
    .name = "class",
    .register_stmts = class_register_stmts,
    .free_children = class_free_children,
    .node_name = class_node_name,
    .gc_blacken = class_gc_blacken,
    .gc_free_buffers = class_gc_free_buffers,
    .eval_node = class_eval_node,
    .pre_member_get = class_pre_member_get,
    .member_get = class_member_get,
    .member_set = class_member_set,
    .pre_call = class_pre_call,
    .call = class_call,
    .binop = class_binop,
    .unop = class_unop,
    .to_string = class_to_string,
};
