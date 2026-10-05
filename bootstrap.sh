#!/usr/bin/env bash
#
# ============================================================
# ART bootstrap for Linux and macOS
#
# Installs GCC (or uses Clang), make, and libm if they aren't
# already present, then builds ART and runs the test suite.
#
# Usage:
#     chmod +x bootstrap.sh
#     ./bootstrap.sh
#
# On Debian/Ubuntu the install step uses sudo. On macOS it uses
# whatever Homebrew or the Xcode command line tools provide. If
# you already have gcc and make, everything is a no-op and the
# script just builds.
# ============================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# -------- pretty output --------

BOLD='\033[1m'
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RESET='\033[0m'

say()  { printf "${BOLD}%s${RESET}\n" "$1"; }
ok()   { printf "${GREEN}%s${RESET}\n" "$1"; }
warn() { printf "${YELLOW}%s${RESET}\n" "$1"; }
die()  { printf "${RED}%s${RESET}\n" "$1" >&2; exit 1; }

# -------- platform detection --------

UNAME="$(uname -s)"
case "$UNAME" in
    Linux*)   PLATFORM=linux ;;
    Darwin*)  PLATFORM=macos ;;
    *)        die "Unsupported platform: $UNAME" ;;
esac

say "ART bootstrap"
say "============================================================"
echo

# -------- tool presence --------

have() { command -v "$1" >/dev/null 2>&1; }

need_install=0
have make || need_install=1
have cc || have gcc || have clang || need_install=1

# -------- dependency install --------

if [ "$need_install" -eq 1 ]; then
    echo "[1/3] Installing build dependencies..."

    if [ "$PLATFORM" = "macos" ]; then
        if ! xcode-select -p >/dev/null 2>&1; then
            warn "Xcode command line tools are missing."
            warn "Run: xcode-select --install"
            warn "Then re-run this script."
            exit 1
        fi
        # xcode-select gives us clang + make. Nothing else needed.
    elif [ "$PLATFORM" = "linux" ]; then
        if have apt-get; then
            sudo apt-get update
            sudo apt-get install -y build-essential
        elif have dnf; then
            sudo dnf install -y gcc make
        elif have yum; then
            sudo yum install -y gcc make
        elif have pacman; then
            sudo pacman -S --noconfirm --needed base-devel
        elif have zypper; then
            sudo zypper install -y gcc make
        elif have apk; then
            sudo apk add --no-cache build-base
        else
            die "No known package manager. Install gcc and make manually."
        fi
    fi

    ok "      Done."
else
    echo "[1/3] Build tools already installed ($(command -v cc || command -v gcc || command -v clang), $(command -v make))"
fi

# -------- libm check (Linux-specific) --------
#
# glibc has libm as a separate library. Most distros ship it, but
# minimal containers sometimes don't. Linking will fail with
# "undefined reference to `sqrt'" if it's missing.

if [ "$PLATFORM" = "linux" ]; then
    if ! echo 'int main(){return 0;}' | cc -x c - -lm -o /tmp/art_libm_check 2>/dev/null; then
        warn "libm might be missing. If the build fails on sqrt/pow,"
        warn "install the math library:"
        warn "    Debian/Ubuntu:  sudo apt-get install libc6-dev"
        warn "    Fedora/RHEL:    sudo dnf install glibc-devel"
    fi
    rm -f /tmp/art_libm_check
fi

# -------- build --------

echo
echo "[2/3] Building ART..."
make clean
make

ok "      Build succeeded."

# -------- test --------

echo
echo "[3/3] Running the test suite..."
if make test; then
    ok "      All tests passed."
else
    die "Some tests failed. See output above."
fi

# -------- done --------

echo
say "============================================================"
say "  ART built successfully."
echo
echo "  Binary:  $SCRIPT_DIR/bin/art"
echo
echo "  Try it:"
echo "    ./bin/art examples/warmup.art"
echo "    ./bin/art examples/battle.art"
echo "    ./bin/art"
say "============================================================"
