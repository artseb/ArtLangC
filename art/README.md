# art/ — interpreter source

Core is the interpreter. Features are the language.

    art/
      art.c, art.h        public embedding API (art_open, art_run_*)
      cli.c               the `art` executable: file runner and REPL
      core/runtime/       values, GC, interpreter state
      core/syntax/        lexer, parser, AST, pattern engine
      core/interp/        the tree-walking evaluator
      features/           one folder per language feature

Dependencies point one way: `features` -> `core/interp` -> `core/syntax`
-> `core/runtime`. Core reaches features only through
`features/registry.h`.

## core/runtime

Owns `Value`, the `Obj` header, the mark-sweep GC, and `ArtState`.

1. `value.h` — tagged union, `OBJ_TYPES(X)`, accessors, every Obj struct
2. `value.c` — equality, hashing, truthiness, type matching
3. `gc.h` / `gc.c` — allocation, mark, sweep, temporary roots
4. `state.h` / `state.c` — `ArtState`, root tables, call frames
5. `register.h` / `register.c` — `art_define_native/method/value/global`
6. `interrupt.h` / `interrupt.c` — Ctrl-C flag

The Obj structs for strings, tables, and functions are declared in
`value.h` even though their constructors live in the feature folders:
core needs the layout (the intern table is an `ObjTable` keyed by
`ObjString *`, every scope and class holds tables, `CallFrame` holds an
`ObjClosure *`, and `gc.c` walks those fields directly).

## core/syntax

Lexer, parser (`parser/`), AST, and the Lua-style pattern engine.
Token and node lists are X-macros in `token.h` and `ast.h`, so a new
keyword or node type is added in one place. `ast_free.c` releases
trees; features contribute their own nodes through hooks.

## core/interp

1. `scope.h` — the scope chain (GC-managed `ObjScope`)
2. `interp.h` / `interp.c` — `art_eval` dispatch, `art_run_source` (the setjmp boundary)
3. `error.c` — `art_runtime_error`: prints `file:line:col: message` plus a stack trace, then `longjmp`s
4. `eval_expr.c`, `eval_control.c`, `eval_call.c`, `eval_table.c` — the node evaluators
5. `format.c` — format specs for interpolation

The calling convention (`call_method`, `call_native_method`,
`eval_call`, `call_any`, `eval_fun_decl`) lives in `eval_call.c`
because every step of it touches core state: scopes, `S->frames`,
`S->control`, `S->active_class`, `S->return_value`.

Invariants:

- Control flow goes through `S->control` (NONE / RETURN / BREAK / CONTINUE).
  Any child eval that can set it must be checked by the caller.
- Runtime errors `longjmp` to the nearest `ErrorFrame` (from
  `art_run_source` or `attempt()`). The error path resets scope,
  active_class, control, return_value, and frame_count.
- Feature hooks run first at every dispatch point (eval_node,
  member_get, member_set, pre_call, call, binop, unop, to_string).

## features

| Folder | Owns |
|---|---|
| `number/` | int/float arithmetic and comparison |
| `string/` | string storage, interning, concat, String methods, patterns (`find`, `gsub`) |
| `table/` | the one collection type: array part, hash part, Table methods |
| `function/` | function, closure, and native constructors |
| `class/` | classes, instances, dispatch, inheritance, operators, `this`, `super` |
| `enum/` | enums, `values()`, `names()`, `fromName`, `fromValue`, `in` |
| `const/` | the `const` modifier |
| `switch/` | switch expression and statement |
| `interface/` | interfaces, `implements` verification |
| `import/` | `import()`, the module cache, cycle detection |
| `misc/` | `print`, `tostring`, `error`, `attempt` |
| `math/`, `file/`, `time/` | the `Math`, `File`, `Time` libraries |

Small features are a single `<feature>.c` plus `<feature>.h`. Larger
ones split by concern: `_parse`, `_ast`, `_eval`, `_runtime`, `_gc`,
`_methods`, `_builtins`. Start with `<feature>.h`, then the file that
defines the `Feature` struct (`<feature>_feature.c`, or `<feature>.c`).

Adding a feature: write the folder, include its header and add one
`X(name)` line in `features/registry.h`. Source files are found by
wildcard, so the Makefile and CMakeLists need no edit.

Rules:

- Core includes `features/registry.h` and nothing else from a feature.
- Features include core headers freely, and other feature headers
  only for a real dependency (class includes interface).
- Hook priority is array order in `registry.h`.
- `register_builtins` runs at state creation, `register_stmts` at parser
  init, both before any ART code runs.

## Portability

- Build with `-std=c11`. POSIX calls (`strdup`, `realpath`,
  `nanosleep`, `mkdir`) need `_XOPEN_SOURCE 700`, which the few files
  that use them define themselves before any include.
- Never name a header after a system header that a `-I` path could
  shadow. `art/features/features.h` once hid glibc's `<features.h>`
  and broke every Linux build, hence `registry.h`.
- Windows-only code is limited to `#ifdef _WIN32` / `_MSC_VER` blocks in
  `import_builtin.c`, `time_builtins.c`, and `tests/test_class_eval.c`.
