#include "interface.h"
#include "interp.h"
#include "scope.h"

#include <stdlib.h>
#include <string.h>

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