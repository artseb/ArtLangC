# Embedding ART

You want a game engine to run scripts written in ART. This is
the path from "I have ART's source" to "my engine runs a script."

## The public API

ART is a C library. `art/art.h` is the whole surface an embedder
needs:

```c
typedef struct ArtState ArtState;

ArtState *art_open(void);
void art_close(ArtState *S);
bool art_run_string(ArtState *S, const char *source, const char *name);
bool art_run_file(ArtState *S, const char *path);
```

That's it. Four functions. `art_open` returns an isolated
interpreter state. `art_run_string` or `art_run_file` runs a
program. `art_close` frees everything. Multiple `ArtState`s can
coexist in one process — nothing is global.

## Three integration paths

### 1. Static link (C or C++)

Add `art/core/**` and `art/features/**` to your build, then call
the four functions. This is what the `art_lib` target in
`CMakeLists.txt` produces.

If your engine is C++, the ART sources compile as C++-compatible
C. Wrap the includes in `extern "C"` at the call sites:

```c+
extern "C" {
    #include "art.h"
}

void run_script(const char *path) {
    ArtState *S = art_open();
    art_run_file(S, path);
    art_close(S);
}
```

### 2. Shared library (for C# / P/Invoke)

Build ART as a DLL. The current CMake sets `art_lib` to
`STATIC` — you'll want to add a shared variant, or flip the
setting:

```cmake
add_library(art_lib SHARED ${CORE_SRC})
```

Then P/Invoke from C#:

```c#
using System;
using System.Runtime.InteropServices;

public static class ART {
    [DllImport("art.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr art_open();

    [DllImport("art.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void art_close(IntPtr state);

    [DllImport("art.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool art_run_string(
        IntPtr state,
        [MarshalAs(UnmanagedType.LPStr)] string source,
        [MarshalAs(UnmanagedType.LPStr)] string name);
}

// Usage:
var S = ART.art_open();
ART.art_run_string(S, "print(\"hello from ART\")", "test");
ART.art_close(S);
```

`art_open` returns an opaque handle. `art_run_string` returns
true on success, false on error (errors print to stderr — see
"what's missing" below).

### 3. Shell out

The engine writes a `.art` file and invokes `art.exe` as a
subprocess, reads stdout. Fine for a first prototype, bad for a
shipped game.

## For a C# / Unity engine

Unity + C# + ART is the shortest path for someone coming from
Unity experience. Rough plan:

1. **Build ART as a DLL.** One CMake flag flip, one build.
2. **Drop the DLL into `Assets/Plugins/`.** Unity picks it up.
3. **P/Invoke the three functions** shown above.
4. **Run `"print(\"hello\")"`** from a `MonoBehaviour.Start()`
and see it in the Unity console.

Once that works, everything else is refinement.

## For a Godot engine

GDScript can't P/Invoke directly. You need a **GDExtension** —
a C++ wrapper that Godot loads as a plugin, which in turn calls
into ART. The wrapper is thin (register a `RunScript(path)`
function on a custom node, forward to `art_run_file`), but
GDExtension has its own build system and learning curve. A
weekend for someone comfortable with Godot's C++ API, longer
otherwise.

## Worked example: engine calls script, script calls engine

The full round-trip an engine actually needs. Six steps, one
C file, about 40 lines.

### 1. Hello world

```c
#include "art.h"

int main(void)
{
    ArtState *S = art_open();
    art_run_string(S, "print(\"hello from ART\")", "<embed>");
    art_close(S);
    return 0;
}
```

Build it, run it, see the print. Everything else builds on this.

### 2. Native Function

```c
#include <stdio.h>
#include <stdlib.h>
#include "art.h"

// The engine-side implementation.
static Value engine_log(ArtState *S, int argc, Value *argv)
{
    for (int i = 0; i < argc; i++) {
        ObjString *s = value_to_string(S, argv[i]);
        char *u = obj_string_to_utf8(s);
        fputs(u, stdout);
        free(u);
        if (i < argc - 1) fputc(' ', stdout);
    }
    fputc('\n', stdout);
    return NIL_VAL;
}

int main(void)
{
    ArtState *S = art_open();
    art_register_native(S, "EngineLog", engine_log, -1);

    art_run_string(S,
        "EngineLog(\"loading level\", 3, \"entities\")",
        "<embed>");

    art_close(S);
    return 0;
}
```

Now scripts can call `EngineLog(...)` and it lands in the engine.

### 3. Value accessors for result
`art_run_string` currently returns `bool`, success or failure. To get
the value of the last expression, drop to `art_run_source`:

```c
#include "interp.h"      // for art_run_source
#include "state.h"       // for ArtState's fields

Value art_run_value(ArtState *S, const char *source, const char *name)
{
    return art_run_source(S, source, (int)strlen(source), name);
}
```

Then inspect the returned `Value` directly. The macros are in `value.h`:

```c
Value v = art_run_value(S, "6 * 7", "<embed>");
if (IS_INT(v))
    printf("result = %lld\n", (long long)AS_INT(v));
if (IS_STRING(v)) {
    char *s = obj_string_to_utf8(AS_STRING(v));
    printf("result = %s\n", s);
    free(s);
}
```

### 4. Pass a number in
Bind a global, then read it back from the script:

```c
ObjString *key = obj_string_from_utf8(S, "input", 5);
table_set(S, S->globals, key, INT_VAL(42));

Value v = art_run_value(S, "input * 2", "<embed>");
// v is INT_VAL(84)
```

Or through `register.h`'s helpers if you prefer not to touch S->globals
directly:
```c
art_define_global(S, "input", INT_VAL(42));
```

### 5. Structured error value

`error(v)` accepts any value; `attempt` hands it back unchanged:

```art
local ok, detail = attempt(fun() {
    error(["code" = 404, "msg" = "not found"])
})

if (!ok) {
    EngineLog("script failed:", detail.code, detail.msg)
}
```

From the engine's side, this is what makes distinguishing "file
not found" from "permission denied" possible without
string-parsing.

### 6. What's left

Value marshaling both ways. Tables in, tables out. Walk the
array and hash parts of an `ObjTable` and convert each element.
Tedious but mechanical.

Error retrieval as a string. Add `const char *art_last_error(S)`
if your engine wants to log errors somewhere other than stderr.

## What doesn't work today

- **Sandboxing.** No way to disable `File` or `Time`. An
untrusted script can read the filesystem. If your game loads
user-authored mods, this matters.
- **Coroutines.** No way to write "wait 3 seconds, then run
this" as a linear script. You write a state machine, or the
engine schedules the resume.
- **Value marshaling helpers.** Passing a table between C# and
ART is manual — walk the array and hash parts, convert each
element.
- **Error retrieval as a string.** Stderr only, for now.
- **Debugger / profiler.** None. You get a stack trace on
error and that's it.
- **Dynamic library builds.** The current CMake only produces
a static library. Adding a shared target is a few lines.

None of these block a first prototype. They're the things you
add once the basic integration works and you know what hurts.

## What to read

- `art/art.h` — the public API
- `examples/` — the language, by example
- `CHANGELOG.md` — the full feature list
- `art/core/runtime/README.md` — the object model, GC, and how
`ArtState` owns everything

If you want the C# P/Invoke path fleshed out with a working
Unity project template, or a C++ wrapper class with RAII around
`ArtState`, that's a separate writeup — ask and I'll produce it.

