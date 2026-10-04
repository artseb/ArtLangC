# features — the language on top of the interpreter

Core is the interpreter. Features are the language.

Read a feature in this order:

1. `<feature>/<feature>.h` — the single include for the whole feature
2. `<feature>/<feature>_feature.c` — the `Feature` struct
3. Then whatever you actually need: `_parse`, `_ast`, `_runtime`, `_eval`, `_methods`, `_gc`, `_builtins`

Feature folders:

| Folder | Owns |
|---|---|
| `number/` | Arithmetic and comparison semantics for int/float |
| `string/` | String storage, conversion, interning, concat, substring, String methods, string hashing |
| `table/` | The one collection type: array part, hash part, Table methods |
| `function/` | ObjFunction, ObjClosure, ObjNative constructors, arity helpers |
| `class/` | ObjClass, ObjInstance, ObjBoundMethod, dispatch, inheritance, operators, members, `this`, `super` |
| `enum/` | ObjEnum, ObjEnumValue, `values()`, `names()`, `fromName`, `fromValue`, `in` |
| `const/` | The `const` modifier, scoped const table check |
| `switch/` | The switch expression/statement |
| `interface/` | ObjInterface, `implements` verification, satisfaction |
| `import/` | The `import()` builtin, cache, cycles |
| `misc/` | print, tostring, error, attempt |
| `math/` | The Math library |
| `file/` | The File class |
| `time/` | The Time library |

Depends on: `core/runtime`, `core/syntax`, `core/interp`.
Depended on by: `core/interp` (through `g_features[]` only).

## Rules

- Core may include `features/features.h` and nothing else from a
  feature folder.
- Features include core headers freely.
- Features may include other feature headers when there's a real
  dependency (class includes interface).
- Hook priority is array order in `features.c`.
- `register_builtins` runs at runtime boot, before any ART code.
- `register_stmts` runs at parser init, before any parse.

## What stays in core, and why

**Structs: `ObjString`, `ObjTable`, `ObjFunction`, `ObjClosure`,
`ObjNative`.** Declared in `core/runtime/value.h` even though
implementations live under `features/`. Core needs the layout:

- The intern table (`S->strings`) is an `ObjTable` whose keys are
  `ObjString *`, and it compares `key->hash` / `key->chars` directly.
- Every scope has `vars` / `consts` tables; every class has five
  method/getter/setter/static tables.
- `CallFrame` holds an `ObjClosure *` directly.
- `gc.c`'s `blacken_object` iterates table and function fields directly.

**The calling convention.** `call_method`, `call_native_method`,
`eval_call`, `call_any`, `eval_fun_decl` live in
`core/interp/eval_call.c`. See `core/interp/README.md`.

**The builtin-registration helpers.** `art_define_native`,
`art_define_method`, `art_define_value`, `art_define_global` live
in `core/runtime/register.h` / `.c`.

## The builtins/ folder is gone

Every builtin now lives in a feature:

- `print`, `tostring`, `error`, `attempt` → `misc/`
- `Math` → `math/`
- `File` → `file/`
- `Time` → `time/`
- `import` → `import/`
- String methods → `string/`
- Table methods → `table/`

The old `art/builtins.h` is now a one-line shim for source
compatibility.
