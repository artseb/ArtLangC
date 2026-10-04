# core/interp — the tree-walking evaluator

Walks the AST and produces `Value`s.

Read in this order:

1. `scope.h` / `scope.c` — the scope chain (GC-managed ObjScope)
2. `interp.h` — `art_eval`, `art_run_source`, `art_run_ast`, error API
3. `error.c` — `art_runtime_error`: prints `file:line:col: message` + stack trace, then `longjmp`s
4. `interp.c` — the eval dispatch table and `art_run_source` (the setjmp boundary)
5. `eval_expr.c` — literals, vars, binary ops, unary, interpolation, `is`
6. `eval_control.c` — decl, assign, block, if, while, for-range, return/break/continue, multi-decl
7. `eval_call.c` — closure calls, native calls, method calls, argument binding
8. `eval_table.c` — table literals, indexing, member access, for-in

Depends on: `core/runtime`, `core/syntax`, and `features/features.h`.
Depended on by: `art/art.c` and `art/cli.c`.

## Why the calling convention lives here

`eval_call.c` owns `call_method`, `call_native_method`, `eval_call`,
`call_any`, and `eval_fun_decl`. The function *objects* they work
with live under `features/function/`. The calling convention itself
stays in core.

Every line of `call_method` manipulates core interpreter state:

- pushes an `ObjScope` (scope.h, core)
- binds `this` and the parameters
- evaluates default-value AST nodes via `art_eval` (core)
- pushes a `CallFrame` onto `S->frames` (state.h, core)
- runs the body via `art_eval`
- interprets `S->control` and restores `S->scope`, `S->active_class`,
  `S->return_value` on the way out

## Key invariants

- Control flow uses `S->control` (NONE / RETURN / BREAK / CONTINUE).
  Every child eval that could set it must be checked by the caller.
- Runtime errors `longjmp` to the nearest `ErrorFrame`, set up by
  `art_run_source` or by `attempt()`. `art_run_source`'s error path
  resets scope, active_class, control, return_value, frame_count.
- Feature hooks run first at every dispatch point (eval_node,
  member_get, member_set, pre_call, call, binop, unop, to_string).
