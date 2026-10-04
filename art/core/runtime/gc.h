#ifndef ART_GC_H
#define ART_GC_H

#include <stddef.h>
#include "value.h"

// ============================================================
// GC state — embedded in ArtState
// ============================================================

typedef struct GcState
{
    Obj *objects; // intrusive list of all heap objects
    size_t bytes_allocated;
    size_t next_gc; // threshold that triggers collection

    // Temporary root stack. Values held only in C locals must be
    // pushed here, or the collector may free them mid-function.
    Value *roots;
    int root_count;
    int root_capacity;

    // Gray stack for tri-color marking
    Obj **gray;
    int gray_count;
    int gray_capacity;

    size_t collections; // stats
    bool enabled;
    bool collecting; // guard against reentrancy
} GcState;

void art_gc_init(GcState *gc);
void art_gc_free_all(ArtState *S);
void art_gc_collect(ArtState *S);

void *art_gc_alloc(ArtState *S, size_t size, ObjType type);

void *art_realloc(ArtState *S, void *ptr, size_t old_size, size_t new_size);

#define ALLOCATE_OBJ(S, type, obj_type) \
    ((type *)art_gc_alloc((S), sizeof(type), (obj_type)))

void art_gc_mark_value(ArtState *S, Value v);
void art_gc_mark_object(ArtState *S, Obj *o);

Value *art_gc_push_root(ArtState *S, Value v);
void art_gc_pop_roots(ArtState *S, int count);

#define GC_PUSH(S, v) art_gc_push_root((S), (v))
#define GC_POP(S, n) art_gc_pop_roots((S), (n))

#define ART_GC_INITIAL_THRESHOLD (1024 * 1024) // 1 MB
#define ART_GC_HEAP_GROW_FACTOR 2
#define ART_GC_GRAY_STACK_INIT 64

#endif // ART_GC_H
