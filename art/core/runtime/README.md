# core/runtime — values, memory, and interpreter state

Owns the object model (`Value`, `Obj` header, `ObjType`), the
mark-sweep GC, and `ArtState`.

Read in this order:

1. `value.h` — the tagged union, `OBJ_TYPES(X)`, accessors, every Obj struct
2. `value.c` — equality, hashing, truthiness, type-name lookup, type matching
3. `gc.h` / `gc.c` — allocation, mark, sweep, temporary roots
4. `state.h` / `state.c` — `ArtState` construction, root tables, call frames
5. `register.h` / `register.c` — the four builtin-registration helpers

Depends on: `features/features.h` (for the value-layer hooks in
`value.c`). Otherwise nothing.

Depended on by: everything.

## What's here vs. what moved

Every constructor now lives with its feature:

| Original file | New home |
|---|---|
| `obj_string.c` | `features/string/string_runtime.c` |
| `obj_table.c` | `features/table/table_runtime.c` |
| `obj_function.c` | `features/function/function_runtime.c` + `features/interface/interface_runtime.c` |

The struct declarations stayed in `value.h` because core needs
the layout — see `features/README.md` for the reasoning.
