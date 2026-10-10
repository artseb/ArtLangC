// ============================================================
// state.c — ArtState lifecycle
// ============================================================

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "state.h"

ArtState *art_state_new(void)
{
    ArtState *S = malloc(sizeof(ArtState));
    if (!S)
        return NULL;

    // Zero everything FIRST. The original order (seed, then memset)
    // wiped the RNG state to 0, which is a fixed point of xorshift64
    // — Math.random() returned 0 unless the user called seed().
    memset(S, 0, sizeof(ArtState));

    S->rng_state = (uint64_t)time(NULL) * 2654435761u;
    if (S->rng_state == 0)
        S->rng_state = 0x9E3779B97F4A7C15ull;

    art_gc_init(&S->gc);

    S->current_node = NULL;
    S->current_file = NULL;
    S->active_class = NULL;
    S->last_error = false;
    S->thrown_value = NIL_VAL;

    S->import_cache = obj_table_new(S);
    S->imports_in_progress = obj_table_new(S);

    S->file_class = NULL;

    S->stack_top = S->stack;
    S->frame_count = 0;

    for (int i = 0; i < OBJ_TYPE_COUNT; i++)
    {
        S->builtin_classes[i] = NULL;
    }

    S->strings = obj_table_new(S);
    S->globals = obj_table_new(S);

    // Pre-intern "this". Safe here — S->strings exists.
    S->this_name = obj_string_from_utf8(S, "this", 4);

    S->global_scope = ALLOCATE_OBJ(S, ObjScope, OBJ_SCOPE);
    S->global_scope->vars = S->globals;
    S->global_scope->consts = obj_table_new(S);
    S->global_scope->parent = NULL;
    S->scope = S->global_scope;
    S->error_frame = NULL;

    return S;
}

void art_state_free(ArtState *S)
{
    if (!S)
        return;

    art_gc_free_all(S);
    free(S);
}
