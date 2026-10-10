#ifndef ART_STATE_H
#define ART_STATE_H

#include <stdint.h>
#include <setjmp.h>
#include "value.h"
#include "gc.h"

typedef struct CallFrame
{
    ObjClosure *closure;
    Node *call_site;
    const char *file_name;
} CallFrame;

typedef struct ErrorFrame
{
    jmp_buf buf;
    struct ErrorFrame *prev;
    const char *file_name;
    bool suppress_output;
    char message[512];
} ErrorFrame;

#define ART_FRAMES_MAX 2048
#define ART_STACK_MAX (1024 * 8)

typedef enum
{
    CONTROL_NONE = 0,
    CONTROL_RETURN,
    CONTROL_BREAK,
    CONTROL_CONTINUE,
} ControlFlow;

struct ArtState
{
    GcState gc;

    Value stack[ART_STACK_MAX];
    Value *stack_top;
    CallFrame frames[ART_FRAMES_MAX];
    int frame_count;

    uint64_t rng_state;

    ObjClass *active_class;

    bool last_error;

    const char *current_file;
    ObjTable *import_cache;
    ObjTable *imports_in_progress;

    Node *current_node;

    ObjClass *file_class;

    ControlFlow control;
    Value return_value;
    Value thrown_value;

    ObjTable *strings;
    ObjTable *globals;

    ObjClass *builtin_classes[OBJ_TYPE_COUNT];

    ObjScope *global_scope;
    ObjScope *scope;
    ErrorFrame *error_frame;

    // Pre-interned "this". Every method call binds it, every
    // method body reads it. Caching the pointer here skips an
    // intern-table probe on each call.
    ObjString *this_name;
};

ArtState *art_state_new(void);
void art_state_free(ArtState *S);

#endif // ART_STATE_H
