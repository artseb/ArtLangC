// ============================================================
// gc.c — mark-sweep garbage collector
// ============================================================

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gc.h"
#include "state.h"
#include "ast.h"
#include "features/features.h"

void art_gc_init(GcState *gc)
{
    gc->objects = NULL;
    gc->bytes_allocated = 0;
    gc->next_gc = ART_GC_INITIAL_THRESHOLD;
    gc->roots = NULL;
    gc->root_count = 0;
    gc->root_capacity = 0;
    gc->gray = NULL;
    gc->gray_count = 0;
    gc->gray_capacity = 0;
    gc->collections = 0;
    gc->enabled = true;
    gc->collecting = false;
}

void *art_gc_alloc(ArtState *S, size_t size, ObjType type)
{
    if (S->gc.enabled && !S->gc.collecting)
    {
        if (S->gc.bytes_allocated + size > S->gc.next_gc)
        {
            art_gc_collect(S);
        }
    }

    Obj *obj = malloc(size);
    if (!obj)
    {
        fprintf(stderr, "art: out of memory (requested %zu bytes)\n", size);
        abort();
    }

    obj->type = type;
    obj->marked = false;
    obj->size = size;
    obj->next = S->gc.objects;
    S->gc.objects = obj;
    S->gc.bytes_allocated += size;

    return obj;
}

void *art_realloc(ArtState *S, void *ptr, size_t old_size, size_t new_size)
{
    S->gc.bytes_allocated += new_size - old_size;

    if (new_size == 0)
    {
        free(ptr);
        return NULL;
    }

    void *result = realloc(ptr, new_size);
    if (!result)
    {
        fprintf(stderr, "art: out of memory (realloc %zu bytes)\n", new_size);
        abort();
    }
    return result;
}

static void gray_push(ArtState *S, Obj *o)
{
    if (S->gc.gray_count + 1 > S->gc.gray_capacity)
    {
        int new_cap = S->gc.gray_capacity < ART_GC_GRAY_STACK_INIT
                          ? ART_GC_GRAY_STACK_INIT
                          : S->gc.gray_capacity * 2;
        S->gc.gray = realloc(S->gc.gray, sizeof(Obj *) * new_cap);
        S->gc.gray_capacity = new_cap;
    }
    S->gc.gray[S->gc.gray_count++] = o;
}

static void mark_object(ArtState *S, Obj *o)
{
    if (o == NULL || o->marked)
        return;
    o->marked = true;
    gray_push(S, o);
}

void art_gc_mark_object(ArtState *S, Obj *o)
{
    mark_object(S, o);
}

void art_gc_mark_value(ArtState *S, Value v)
{
    if (IS_OBJ(v))
        mark_object(S, AS_OBJ(v));
}

static void blacken_object(ArtState *S, Obj *o)
{
    switch (o->type)
    {
    case OBJ_STRING:
        break;

    case OBJ_TABLE:
    {
        ObjTable *t = (ObjTable *)o;
        for (int i = 0; i < t->array_count; i++)
        {
            art_gc_mark_value(S, t->array[i]);
        }
        for (int i = 0; i < t->hash_capacity; i++)
        {
            TableEntry *e = &t->entries[i];
            if (e->key != NULL)
            {
                mark_object(S, (Obj *)e->key);
                art_gc_mark_value(S, e->value);
            }
        }
        break;
    }

    case OBJ_FUNCTION:
    {
        ObjFunction *fn = (ObjFunction *)o;
        mark_object(S, (Obj *)fn->name);
        // Parameters carry GC-managed type references. Without
        // marking them, a function whose type annotations resolve
        // to a class or interface can crash on the next collection.
        if (fn->params != NULL)
        {
            for (int i = 0; i < fn->arity; i++)
            {
                mark_object(S, (Obj *)fn->params[i].type_name);
                mark_object(S, (Obj *)fn->params[i].type_class);
                mark_object(S, (Obj *)fn->params[i].type_interface);
            }
        }
        break;
    }

    case OBJ_CLOSURE:
    {
        ObjClosure *closure = (ObjClosure *)o;
        mark_object(S, (Obj *)closure->function);
        mark_object(S, (Obj *)closure->captured_scope);
        mark_object(S, (Obj *)closure->owner_class);
        break;
    }

    case OBJ_NATIVE:
    {
        ObjNative *n = (ObjNative *)o;
        mark_object(S, (Obj *)n->name);
        break;
    }

    case OBJ_SCOPE:
    {
        ObjScope *scope = (ObjScope *)o;
        mark_object(S, (Obj *)scope->vars);
        mark_object(S, (Obj *)scope->consts);
        mark_object(S, (Obj *)scope->parent);
        break;
    }

    case OBJ_ENUM:
    case OBJ_ENUM_VALUE:
        enum_gc_blacken(S, o);
        break;

    case OBJ_CLASS:
    case OBJ_INSTANCE:
    case OBJ_BOUND_METHOD:
        class_gc_blacken(S, o);
        break;

    case OBJ_INTERFACE:
        interface_gc_blacken(S, o);
        break;

    case OBJ_TYPE_COUNT:
        break;
    }
}

static void mark_roots(ArtState *S)
{
    for (Value *v = S->stack; v < S->stack_top; v++)
    {
        art_gc_mark_value(S, *v);
    }

    for (int i = 0; i < S->frame_count; i++)
    {
        mark_object(S, (Obj *)S->frames[i].closure);
    }

    mark_object(S, (Obj *)S->global_scope);
    mark_object(S, (Obj *)S->scope);

    mark_object(S, (Obj *)S->strings);
    mark_object(S, (Obj *)S->globals);

    mark_object(S, (Obj *)S->import_cache);
    mark_object(S, (Obj *)S->imports_in_progress);

    for (int i = 0; i < OBJ_TYPE_COUNT; i++)
    {
        if (S->builtin_classes[i] != NULL)
        {
            mark_object(S, (Obj *)S->builtin_classes[i]);
        }
    }

    for (int i = 0; i < S->gc.root_count; i++)
    {
        art_gc_mark_value(S, S->gc.roots[i]);
    }
}

static void free_object_buffers(ArtState *S, Obj *o)
{
    if (o->type == OBJ_INSTANCE)
    {
        ObjInstance *inst = (ObjInstance *)o;
        if (inst->userdata && inst->userdata_free)
            inst->userdata_free(inst->userdata);
    }

    switch (o->type)
    {
    case OBJ_STRING:
    {
        ObjString *s = (ObjString *)o;
        art_realloc(S, s->chars,
                    sizeof(uint16_t) * (s->unit_count + 1), 0);
        break;
    }

    case OBJ_TABLE:
        obj_table_free(S, (ObjTable *)o);
        break;

    case OBJ_FUNCTION:
    {
        ObjFunction *fn = (ObjFunction *)o;
        if (fn->body)
            node_free_tree(&fn->body);
        if (fn->params)
        {
            // Default-value AST nodes are owned by the function,
            // not by its body. Free them explicitly.
            for (int i = 0; i < fn->arity; i++)
            {
                if (fn->params[i].default_value)
                    node_free_tree(&fn->params[i].default_value);
            }
            free(fn->params);
        }
        break;
    }

    case OBJ_ENUM:
    case OBJ_ENUM_VALUE:
        enum_gc_free_buffers(S, o);
        break;

    case OBJ_CLASS:
    case OBJ_INSTANCE:
    case OBJ_BOUND_METHOD:
        class_gc_free_buffers(S, o);
        break;

    case OBJ_INTERFACE:
        interface_gc_free_buffers(S, o);
        break;

    case OBJ_SCOPE:
    case OBJ_CLOSURE:
    case OBJ_NATIVE:
    case OBJ_TYPE_COUNT:
        break;
    }
}

static void sweep(ArtState *S)
{
    Obj **obj = &S->gc.objects;

    while (*obj != NULL)
    {
        Obj *current = *obj;

        if (!current->marked)
        {
            *obj = current->next;
            size_t obj_size = current->size;
            free_object_buffers(S, current);
            free(current);
            S->gc.bytes_allocated -= obj_size;
        }
        else
        {
            current->marked = false;
            obj = &current->next;
        }
    }
}

void art_gc_collect(ArtState *S)
{
    if (S->gc.collecting)
        return;
    if (!S->gc.enabled)
        return;

    S->gc.collecting = true;

    mark_roots(S);

    while (S->gc.gray_count > 0)
    {
        Obj *o = S->gc.gray[--S->gc.gray_count];
        blacken_object(S, o);
    }

    sweep(S);

    S->gc.next_gc = S->gc.bytes_allocated * ART_GC_HEAP_GROW_FACTOR;
    if (S->gc.next_gc < ART_GC_INITIAL_THRESHOLD)
    {
        S->gc.next_gc = ART_GC_INITIAL_THRESHOLD;
    }

    S->gc.collections++;
    S->gc.collecting = false;
}

Value *art_gc_push_root(ArtState *S, Value v)
{
    if (S->gc.root_count + 1 > S->gc.root_capacity)
    {
        int new_cap = S->gc.root_capacity < 16
                          ? 16
                          : S->gc.root_capacity * 2;
        S->gc.roots = realloc(S->gc.roots, sizeof(Value) * new_cap);
        S->gc.root_capacity = new_cap;
    }
    S->gc.roots[S->gc.root_count] = v;
    return &S->gc.roots[S->gc.root_count++];
}

void art_gc_pop_roots(ArtState *S, int count)
{
    S->gc.root_count -= count;
    if (S->gc.root_count < 0)
        S->gc.root_count = 0;
}

void art_gc_free_all(ArtState *S)
{
    Obj *o = S->gc.objects;
    while (o != NULL)
    {
        Obj *next = o->next;
        free_object_buffers(S, o);
        free(o);
        o = next;
    }
    S->gc.objects = NULL;
    S->gc.bytes_allocated = 0;

    free(S->gc.roots);
    S->gc.roots = NULL;
    S->gc.root_count = 0;
    S->gc.root_capacity = 0;

    free(S->gc.gray);
    S->gc.gray = NULL;
    S->gc.gray_count = 0;
    S->gc.gray_capacity = 0;
}
