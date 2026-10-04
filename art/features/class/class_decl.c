// ============================================================
// class_decl.c — class declaration and method registration
//
// Ordering in eval_class_decl:
//   1. Create the class.
//   2. Set up fields (and mirror statics into klass->statics).
//   3. Declare the class in scope (so its own name resolves).
//   4. Register methods.
//   5. Verify implements.
//
// Steps 3 and 4 matter: a method's type annotation can reference
// the class's own name, so the class must be in scope before
// register_method resolves param types. Step 5 after step 4 is
// why `implements` can see the methods it needs to verify.
// ============================================================

#include "class.h"
#include "interp.h"
#include "scope.h"
#include "gc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool same_signature(ObjFunction *a, ObjFunction *b)
{
    if (a->arity != b->arity) return false;
    if (a->is_variadic != b->is_variadic) return false;
    for (int i = 0; i < a->arity; i++)
        if (a->params[i].type_name != b->params[i].type_name)
            return false;
    return true;
}

void class_resolve_param_types(ArtState *S, ObjFunction *fn)
{
    for (int i = 0; i < fn->arity; i++)
    {
        Param *p = &fn->params[i];
        p->type_class = NULL;
        p->type_interface = NULL;

        if (p->type_name == NULL) continue;

        Value v;
        if (art_scope_lookup(S->scope, p->type_name, NULL, &v))
        {
            if (IS_CLASS(v)) p->type_class = AS_CLASS(v);
            else if (IS_INTERFACE(v)) p->type_interface = AS_INTERFACE(v);
        }
    }
}

static void register_method(ArtState *S, ObjClass *klass,
                            ObjClosure *cl, Node *at)
{
    ObjFunction *fn = cl->function;

    if (!fn->is_static && !fn->is_getter && !fn->is_setter &&
        !fn->is_operator && fn->name == klass->name)
    {
        fn->is_constructor = true;
    }

    fn->owner_class = klass;

    ObjTable *target;
    ObjString *key = fn->name;
    bool overloadable = true;

    if (fn->is_getter) { target = klass->getters; overloadable = false; }
    else if (fn->is_setter) { target = klass->setters; overloadable = false; }
    else if (fn->is_static) { target = klass->static_methods; }
    else { target = klass->methods; }

    if (fn->is_operator)
    {
        const char *opname = token_type_name((TokenType)fn->operator_op);
        key = obj_string_from_utf8(S, opname, (int)strlen(opname));
    }

    if (!overloadable)
    {
        if (table_has(target, key))
            art_runtime_error(S, at, "duplicate %s '%s'",
                              fn->is_getter ? "getter" : "setter",
                              obj_string_to_utf8(key));
        table_set(S, target, key, OBJ_VAL(cl));
        return;
    }

    Value existing = table_get(target, key);
    ObjTable *overloads;

    if (IS_TABLE(existing)) { overloads = AS_TABLE(existing); }
    else
    {
        overloads = obj_table_new(S);
        GC_PUSH(S, OBJ_VAL(overloads));
        table_set(S, target, key, OBJ_VAL(overloads));
        GC_POP(S, 1);
    }

    for (int i = 0; i < overloads->array_count; i++)
    {
        ObjClosure *other = AS_CLOSURE(overloads->array[i]);
        if (same_signature(other->function, fn))
            art_runtime_error(S, at, "duplicate function detected: '%s'",
                              obj_string_to_utf8(key));
    }

    table_push(S, overloads, OBJ_VAL(cl));
}

Value eval_class_decl(ArtState *S, Node *n)
{
    ClassDeclNode *c = (ClassDeclNode *)n;

    ObjClass *super = NULL;
    if (c->superclass_name != NULL)
    {
        Value sv;
        if (!art_scope_lookup(S->scope, c->superclass_name, NULL, &sv) ||
            !IS_CLASS(sv))
        {
            art_runtime_error(S, n, "unknown superclass '%s'",
                              obj_string_to_utf8(c->superclass_name));
        }
        super = AS_CLASS(sv);
    }

    ObjClass *klass = obj_class_new(S, c->name, super);
    GC_PUSH(S, OBJ_VAL(klass));

    if (c->field_count > 0)
    {
        klass->fields = malloc(sizeof(Field) * c->field_count);
        klass->field_count = 0;
    }

    for (int i = 0; i < c->field_count; i++)
    {
        FieldDeclNode *fd = (FieldDeclNode *)c->fields[i];

        Value def = NIL_VAL;
        if (fd->default_value)
        {
            def = art_eval(S, fd->default_value);
            if (S->control != CONTROL_NONE) { GC_POP(S, 1); return NIL_VAL; }
        }

        Field *f = &klass->fields[klass->field_count];
        f->name = fd->name;
        f->default_value = def;
        f->is_private = fd->is_private;
        f->is_static = fd->is_static;
        f->is_const = fd->is_const;

        klass->field_count++;

        // Static fields live in the statics table so that
        // `Class.field` reads and writes reach them. Instance
        // fields stay in klass->fields only.
        if (fd->is_static)
            table_set(S, klass->statics, fd->name, def);
    }

    art_scope_declare(S, S->scope, c->name, OBJ_VAL(klass));

    for (int i = 0; i < c->method_count; i++)
    {
        MethodDeclNode *md = (MethodDeclNode *)c->methods[i];
        if (!md->fn) continue;

        class_resolve_param_types(S, md->fn);

        ObjClosure *cl = obj_closure_new(S, md->fn);
        GC_PUSH(S, OBJ_VAL(cl));
        cl->owner_class = klass;
        cl->captured_scope = S->scope;
        register_method(S, klass, cl, n);
        GC_POP(S, 1);
    }

    if (c->implements_count > 0)
    {
        klass->interfaces = malloc(sizeof(ObjInterface *) * c->implements_count);
        klass->interface_count = 0;

        for (int i = 0; i < c->implements_count; i++)
        {
            Value iv;
            if (!art_scope_lookup(S->scope, c->implements_names[i], NULL, &iv) ||
                !IS_INTERFACE(iv))
            {
                art_runtime_error(S, n, "unknown interface '%s'",
                                  obj_string_to_utf8(c->implements_names[i]));
            }
            ObjInterface *iface = AS_INTERFACE(iv);

            ObjString *missing[32];
            int n_missing = interface_missing_members(klass, iface,
                                                      missing, 32);
            if (n_missing > 0)
            {
                char buf[512];
                int w = 0;
                for (int k = 0; k < n_missing && k < 8; k++)
                {
                    const char *mn = obj_string_to_utf8(missing[k]);
                    w += snprintf(buf + w, sizeof(buf) - w,
                                  "%s%s", k ? ", " : "", mn);
                    if (w >= (int)sizeof(buf) - 4) break;
                }
                art_runtime_error(S, n,
                                  "%s does not implement %s (missing: %s)",
                                  obj_string_to_utf8(klass->name),
                                  obj_string_to_utf8(iface->name), buf);
            }

            klass->interfaces[klass->interface_count++] = iface;
        }
    }

    GC_POP(S, 1);
    return OBJ_VAL(klass);
}
