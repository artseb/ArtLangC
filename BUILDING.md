# Building ART

Four ways. Pick whichever matches what you have installed.

## One-command install

If you don't have a C toolchain yet, use the bootstrap script
for your platform. It installs the compiler and make, builds
ART, and runs the test suite.

**Windows:**

    bootstrap.bat

Right-click and choose "Run as administrator". Installs MSYS2,
GCC, make, builds, tests. About 600 MB the first time, a few
seconds on re-runs.

**Linux / macOS:**

    chmod +x bootstrap.sh
    ./bootstrap.sh

Uses apt/dnf/pacman/zypper/apk on Linux, Xcode tools on macOS.
No-op if GCC and make are already present.

## What do I have?

    gcc --version       # GCC
    clang --version     # Clang
    make --version      # Make
    cmake --version     # CMake

If none of those print a version, use the bootstrap script above
or install manually:

| Platform | GCC + make | Clang | CMake |
|---|---|---|---|
| Windows | MSYS2 UCRT64: `pacman -S mingw-w64-ucrt-x86_64-gcc make` | same package, `clang` variant | `pacman -S mingw-w64-ucrt-x86_64-cmake` |
| macOS   | `xcode-select --install` | same (Xcode tools include clang) | `brew install cmake` |
| Debian / Ubuntu | `sudo apt install build-essential` | `sudo apt install clang` | `sudo apt install cmake` |
| Fedora / RHEL  | `sudo dnf install gcc make` | `sudo dnf install clang` | `sudo dnf install cmake` |
| Arch | `sudo pacman -S base-devel` | `sudo pacman -S clang` | `sudo pacman -S cmake` |
| Alpine | `sudo apk add build-base` | `sudo apk add clang` | `sudo apk add cmake` |

## 1. Make — the default

    make            # builds bin/art and every test binary
    make test       # runs them all
    make clean      # removes .o, .d, bin/, and test binaries

Works with GCC and Clang on Linux, macOS, and Windows (MSYS2).

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

## 4. Running on Linux without a Linux machine

If you develop on Windows and don't have a Linux box, two
options for verifying the Linux build:

### WSL

Windows Subsystem for Linux runs a real Ubuntu install. From an
admin PowerShell:

    wsl --install

Reboot, open the Ubuntu app, and you're on Linux. Clone or copy
the repo into the Linux filesystem (`/home/<user>/...`, not
`/mnt/c/...` — the `/mnt` path works but is slow) and run:

    ./bootstrap.sh

### GitHub Actions

Every push and pull request is tested on Linux, macOS, and
Windows by `.github/workflows/test.yml`. Push a branch and check
the Actions tab — a green checkmark means the documented build
instructions work on that platform. No local setup.

## Platform notes

### Linux

`-lm` is passed automatically by both the Makefile and CMake.
If the link fails with `undefined reference to sqrt`, libm is
missing:

    sudo apt install libc6-dev          # Debian / Ubuntu
    sudo dnf install glibc-devel        # Fedora / RHEL
    sudo pacman -S glibc                # Arch

### macOS

Both Intel and Apple Silicon work. Clang is the default
compiler; `make` picks it up automatically via `CC` if GCC
isn't installed. The `-lm` flag is a no-op on macOS (math is
in libc), so the Makefile's link line works either way.

### Windows

Use the MSYS2 UCRT64 shell, not the standard `cmd.exe`. The
toolchain the project is developed and tested against is
`mingw-w64-ucrt-x86_64-gcc`, installed from the UCRT64
environment.

The `bootstrap.bat` script handles the whole path: it installs
MSYS2 to `C:\msys64`, pulls the UCRT64 toolchain, and builds.

### MSVC

Works via CMake, with one caveat. `Time.sleep` is built on
`nanosleep` on POSIX and MinGW, but MSVC doesn't have that
function. The Windows build uses `Sleep()` (millisecond
granularity) instead, which means `Time.sleep(0.3)` rounds up
to `Time.sleep(1)`. On real hardware that's what the Windows
scheduler delivers anyway, so the practical behavior is
unchanged.

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
