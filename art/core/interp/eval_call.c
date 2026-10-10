// ============================================================
// eval_call.c — function declaration, closure calls, call site
// ============================================================

#include "interp.h"
#include "scope.h"

#define MAX_ARGS 64

static void native_frame_push(ArtState *S, Node *at)
{
    if (S->frame_count >= ART_FRAMES_MAX)
        return;

    S->frames[S->frame_count].closure = NULL;
    S->frames[S->frame_count].call_site = at;
    S->frames[S->frame_count].file_name = S->current_file;
    S->frame_count++;
}

static void native_frame_pop(ArtState *S)
{
    if (S->frame_count > 0)
        S->frame_count--;
}

Value call_native_method(ArtState *S, ObjNative *nat, Value this_val,
                         int argc, Value *args, Node *at)
{
    if (nat->arity >= 0 && nat->arity != argc)
        art_runtime_error(S, at, "expected %d arguments, got %d",
                          nat->arity, argc);

    if (S->frame_count >= ART_FRAMES_MAX)
        art_runtime_error(S, at, "call depth limit reached");

    ObjScope *saved = S->scope;
    S->scope = obj_scope_new(S, saved);

    art_scope_declare(S, S->scope, S->this_name, this_val);

    native_frame_push(S, at);
    Value result = nat->fn(S, argc, args);
    native_frame_pop(S);

    S->scope = saved;
    return result;
}

Value eval_fun_decl(ArtState *S, Node *n)
{
    FunDeclNode *f = (FunDeclNode *)n;

    ObjClosure *cl = obj_closure_new(S, f->fn);
    GC_PUSH(S, OBJ_VAL(cl));
    cl->captured_scope = S->scope;
    cl->owner_class = S->active_class;

    if (f->fn->name != NULL)
        art_scope_declare(S, S->scope, f->fn->name, OBJ_VAL(cl));

    GC_POP(S, 1);
    return OBJ_VAL(cl);
}

Value call_method(ArtState *S, ObjClosure *cl, Value this_val,
                  int argc, Value *args, Node *at)
{
    ObjFunction *fn = cl->function;
    int min_arity = function_min_arity(fn);

    if (fn->is_variadic)
    {
        if (argc < min_arity)
            art_runtime_error(S, at,
                              "expected at least %d arguments, got %d",
                              min_arity, argc);
    }
    else
    {
        if (argc < min_arity || argc > fn->arity)
            art_runtime_error(S, at,
                              "expected %d to %d arguments, got %d",
                              min_arity, fn->arity, argc);
    }

    if (S->frame_count >= ART_FRAMES_MAX)
        art_runtime_error(S, at,
                          "call depth limit reached (%d) — likely runaway recursion",
                          ART_FRAMES_MAX);

    ObjScope *saved_scope = S->scope;
    ControlFlow saved_control = S->control;
    Value saved_return = S->return_value;
    ObjClass *saved_class = S->active_class;

    S->scope = obj_scope_new(S, cl->captured_scope);
    S->active_class = cl->owner_class;

    if (!IS_NIL(this_val))
        art_scope_declare(S, S->scope, S->this_name, this_val);

    for (int i = 0; i < fn->arity; i++)
    {
        Value v;
        if (i < argc)
        {
            v = args[i];
        }
        else if (fn->params[i].default_value != NULL)
        {
            v = art_eval(S, fn->params[i].default_value);
            if (S->control != CONTROL_NONE)
            {
                S->scope = saved_scope;
                S->active_class = saved_class;
                return NIL_VAL;
            }
        }
        else
        {
            art_runtime_error(S, at, "internal: missing required argument");
        }
        art_scope_declare(S, S->scope, fn->params[i].name, v);
    }

    if (fn->is_variadic)
    {
        ObjTable *rest = obj_table_new(S);
        GC_PUSH(S, OBJ_VAL(rest));
        for (int i = fn->arity; i < argc; i++)
            table_push(S, rest, args[i]);
        art_scope_declare(S, S->scope, fn->params[fn->arity].name,
                          OBJ_VAL(rest));
        GC_POP(S, 1);
    }

    S->frames[S->frame_count].closure = cl;
    S->frames[S->frame_count].call_site = at;
    S->frames[S->frame_count].file_name = S->current_file;
    S->frame_count++;
    S->control = CONTROL_NONE;
    S->return_value = NIL_VAL;

    art_eval(S, fn->body);

    Value result = NIL_VAL;
    if (S->control == CONTROL_RETURN)
        result = S->return_value;
    else if (S->control == CONTROL_BREAK || S->control == CONTROL_CONTINUE)
    {
        S->scope = saved_scope;
        S->active_class = saved_class;
        S->frame_count--;
        art_runtime_error(S, at, "break/continue outside loop");
    }

    S->scope = saved_scope;
    S->control = saved_control;
    S->return_value = saved_return;
    S->active_class = saved_class;
    S->frame_count--;

    return result;
}

Value eval_call(ArtState *S, Node *n)
{
    CallNode *c = (CallNode *)n;

    FEATURES_FOR_EACH(f)
    {
        if (f->pre_call == NULL)
            continue;
        Value out;
        if (f->pre_call(S, c, &out))
            return out;
    }

    Value callee = art_eval(S, c->callee);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    if (c->arg_count > MAX_ARGS)
        art_runtime_error(S, n, "too many arguments (%d, max %d)",
                          c->arg_count, MAX_ARGS);

    Value args[MAX_ARGS];
    for (int i = 0; i < c->arg_count; i++)
    {
        args[i] = art_eval(S, c->args[i]);
        if (S->control != CONTROL_NONE)
            return NIL_VAL;
    }

    if (IS_NATIVE(callee))
    {
        ObjNative *nat = AS_NATIVE(callee);
        if (nat->arity >= 0 && nat->arity != c->arg_count)
            art_runtime_error(S, n, "expected %d arguments, got %d",
                              nat->arity, c->arg_count);

        Node *saved = S->current_node;
        S->current_node = n;
        Value result = nat->fn(S, c->arg_count, args);
        S->current_node = saved;
        return result;
    }

    if (IS_CLOSURE(callee))
        return call_method(S, AS_CLOSURE(callee), NIL_VAL,
                           c->arg_count, args, n);

    if (IS_BOUND_METHOD(callee))
    {
        ObjBoundMethod *bm = AS_BOUND_METHOD(callee);

        if (IS_NATIVE(bm->method))
        {
            return call_native_method(S, AS_NATIVE(bm->method),
                                      bm->receiver, c->arg_count, args, n);
        }

        ObjClosure *cl = AS_CLOSURE(bm->method);
        ObjClass *search = bm->start_class;

        if (search == NULL)
            search = builtin_class_of(S, bm->receiver);

        if (search == NULL)
            return call_method(S, cl, bm->receiver, c->arg_count, args, n);

        ObjString *name = cl->function->name;
        ObjClosure *resolved = class_find_method(
            S, search, name, c->arg_count, args, n);

        if (resolved == NULL)
            art_runtime_error(S, n,
                              "no overload of '%s' matches these arguments",
                              obj_string_to_utf8(name));

        return call_method(S, resolved, bm->receiver,
                           c->arg_count, args, n);
    }

    FEATURES_FOR_EACH(f)
    {
        if (f->call == NULL)
            continue;
        Value out;
        if (f->call(S, callee, c->arg_count, args, n, &out))
            return out;
    }

    art_runtime_error(S, n, "cannot call %s", value_type_name(callee));
    return NIL_VAL;
}

Value call_any(ArtState *S, Value callee, int argc, Value *args, Node *at)
{
    if (IS_CLOSURE(callee))
        return call_method(S, AS_CLOSURE(callee), NIL_VAL, argc, args, at);

    if (IS_NATIVE(callee))
    {
        ObjNative *n = AS_NATIVE(callee);
        if (n->arity >= 0 && n->arity != argc)
            art_runtime_error(S, at, "expected %d arguments, got %d",
                              n->arity, argc);
        return n->fn(S, argc, args);
    }

    if (IS_BOUND_METHOD(callee))
    {
        ObjBoundMethod *bm = AS_BOUND_METHOD(callee);
        if (IS_NATIVE(bm->method))
            return call_native_method(S, AS_NATIVE(bm->method),
                                      bm->receiver, argc, args, at);
        return call_method(S, AS_CLOSURE(bm->method), bm->receiver,
                           argc, args, at);
    }

    art_runtime_error(S, at, "cannot call %s", value_type_name(callee));
    return NIL_VAL;
}
