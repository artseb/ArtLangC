#ifndef ART_FEATURE_H
#define ART_FEATURE_H

#include "ast.h"
#include "value.h"
#include "state.h"

// ============================================================
// Feature — one per language extension
//
// Core walks g_features in declaration order and calls each
// hook. A hook returns true if it handled the case; false lets
// core (or the next feature) try.
//
// Any hook may be NULL. NULL means "this feature has nothing to
// say about this kind of event."
//
// Priority: g_features[] order. First feature to claim an event
// wins.
// ============================================================

typedef struct Feature
{
    const char *name;

    // Parser statement-table registration. Runs once at parser
    // init, before any parse.
    void (*register_stmts)(void);

    // Builtin installation into S->global_scope->vars. Runs once
    // at runtime boot, before any ART code executes.
    void (*register_builtins)(ArtState *S);

    bool (*free_children)(Node *n);

    const char *(*node_name)(NodeType t);

    bool (*gc_blacken)(ArtState *S, Obj *o);
    bool (*gc_free_buffers)(ArtState *S, Obj *o);

    bool (*eval_node)(ArtState *S, Node *n, Value *out);

    bool (*pre_member_get)(ArtState *S, MemberNode *m, Node *at, Value *out);
    bool (*member_get)(ArtState *S, Value target, ObjString *name,
                       Node *at, Value *out);
    bool (*member_set)(ArtState *S, Value target, ObjString *name,
                       Value value, Node *at, Value *out);

    bool (*pre_call)(ArtState *S, CallNode *c, Value *out);
    bool (*call)(ArtState *S, Value callee, int argc, Value *args,
                 Node *at, Value *out);

    bool (*binop)(ArtState *S, Node *at, TokenType op,
                  Value a, Value b, Value *out);
    bool (*unop)(ArtState *S, Node *at, TokenType op, Value v, Value *out);

    bool (*to_string)(ArtState *S, Value v, ObjString **out);

    bool (*value_equal)(Value a, Value b, bool *out);
    bool (*value_hash)(Value v, uint32_t *out);
} Feature;

extern Feature **g_features;
extern int g_feature_count;

#define FEATURES_FOR_EACH(var)                                      \
    for (int _fi = 0; _fi < g_feature_count; _fi++)                 \
        for (Feature *var = g_features[_fi], *_once = (Feature *)1; \
             _once != NULL;                                         \
             _once = NULL)

#endif // ART_FEATURE_H
