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

```
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

```
add_library(art_lib SHARED ${CORE_SRC})
```

Then P/Invoke from C#:

```
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

## What to build in order

1. **Static link a hello-world.** Open an `ArtState`, run
`print("hi")`, close. Verify the binary actually runs.
2. **Expose a native function.** Add `art_register_native(S,
"Log", my_log_fn)` to `art.h` (about 20 lines — the
internals already support it). Now scripts can call back
into the engine.
3. **Pass a number in, get a number out.** `art_run_string`
returns the value of the last expression as a `Value`. Add
a public accessor for `int`/`float`/`string`, and the engine
can call a script and use the result.
4. **Error retrieval.** Add `const char *art_last_error(ArtState
*S)`. Currently errors go to stderr; engines want them as a
string they can log or display.
5. **Value marshaling.** Tables in, tables out. This is where
it gets fiddly and where you'll spend the most time.

Steps 1–3 are a day's work. Step 4 is an afternoon. Step 5 is
open-ended.

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

