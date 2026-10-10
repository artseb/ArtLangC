#ifndef ART_INTERP_H
#define ART_INTERP_H

#include <setjmp.h>
#include "ast.h"
#include "value.h"
#include "state.h"
#include "features/registry.h"

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn))
#elif defined(_MSC_VER)
__declspec(noreturn)
#endif
void art_runtime_error(ArtState *S, Node *at, const char *fmt, ...);
void art_throw_value(ArtState *S, Node *at, Value v);

Value art_run_source(ArtState *S, const char *source, int length,
                     const char *file_name);

Value art_run_ast(ArtState *S, Node *program);

Value art_eval(ArtState *S, Node *n);

Value eval_binop(ArtState *S, Node *at, TokenType op, Value a, Value b);
Value eval_unary(ArtState *S, Node *n);

Value eval_fun_decl(ArtState *S, Node *n);
Value eval_call(ArtState *S, Node *n);
Value call_any(ArtState *S, Value callee, int argc, Value *args, Node *at);

void interp_init(ArtState *S);

Value call_method(ArtState *S, ObjClosure *cl, Value this_val,
                  int argc, Value *args, Node *at);

Value eval_table_literal(ArtState *S, Node *n);
Value eval_index(ArtState *S, Node *n);
Value eval_member(ArtState *S, Node *n);
Value eval_for_in(ArtState *S, Node *n);
Value eval_assign_member(ArtState *S, Node *n, TokenType binop);
Value eval_assign_index(ArtState *S, Node *n, TokenType binop);

Value call_native_method(ArtState *S, ObjNative *nat, Value this_val,
                         int argc, Value *args, Node *at);

ObjClass *builtin_class_of(ArtState *S, Value v);

ObjString *value_to_string(ArtState *S, Value v);

#endif // ART_INTERP_H
