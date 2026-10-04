# Building ART

Three ways. Pick whichever matches what you have installed.

## What do I have?

    gcc --version       # GCC
    clang --version     # Clang
    cmake --version     # CMake

If none of those print a version, install one:

| Platform | GCC + make | Clang | CMake |
|---|---|---|---|
| Windows | MSYS2 UCRT64: `pacman -S mingw-w64-ucrt-x86_64-gcc make` | same package, `clang` variant | `pacman -S mingw-w64-ucrt-x86_64-cmake` |
| macOS   | `xcode-select --install` | same (Xcode tools include clang) | `brew install cmake` |
| Debian / Ubuntu | `sudo apt install build-essential` | `sudo apt install clang` | `sudo apt install cmake` |
| Fedora / RHEL  | `sudo dnf install gcc make` | `sudo dnf install clang` | `sudo dnf install cmake` |

## 1. Make — the default

    make            # builds bin/art and every test binary
    make test       # runs them all
    make clean      # removes .o, .d, bin/, and test binaries

Works with GCC and Clang. On Windows, use the MSYS2 UCRT64 shell
— that's the toolchain the project is developed against and the
one the tests are validated on.

## 2. Make with Clang

Clang is a drop-in replacement; no files change:

    make CC=clang

## 3. CMake — for IDEs and MSVC

    cmake -B build
    cmake --build build
    ctest --test-dir build

Or open the repo folder directly in Visual Studio, VS Code with
the CMake Tools extension, Xcode, or CLion — they all detect
`CMakeLists.txt` and configure automatically.

### MSVC

Works, with one caveat. `Time.sleep` is built on `nanosleep` on
POSIX and MinGW, but MSVC doesn't have that function. The Windows
build uses `Sleep()` (millisecond granularity) instead, which
means `Time.sleep(0.3)` rounds up to `Time.sleep(1)`. On real
hardware that's what the Windows scheduler delivers anyway, so
the practical behavior is unchanged.

The shim lives inline in `art/features/time/time_builtins.c`
under `#ifdef _MSC_VER`. Nothing else in the project needs a
Windows-specific path.

## Which should I use?

- **Linux or macOS** — `make`. It's the tested path and the
  fastest.
- **Windows with MSYS2 UCRT64** — `make`. Same reason.
- **Windows with Visual Studio** — CMake. Visual Studio's "Open
  Folder" workflow handles it with no extra steps.
- **Anywhere with a favorite IDE** — CMake.

## Reproducing a bug report

If you're filing an issue, include the output of:

    uname -a          # or `systeminfo | findstr /B /C:"OS"`
    gcc --version     # or `clang --version`, `cl` for MSVC
    make              # so the exact compile lines are captured
    make test         # or `ctest --test-dir build --output-on-failure`

The compile lines matter more than they look — most build
failures here have been missing `-I` paths or a file the glob
didn't pick up, and the failing `gcc ...` line shows exactly
which.
