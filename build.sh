#!/usr/bin/env bash
set -euo pipefail

# tfml build script - produces a statically linked binary using musl libc
# Requirements: musl-gcc or musl-tools (Debian/Ubuntu)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
THEMES_DIR="$SCRIPT_DIR/themes"
OUT_DIR="$SCRIPT_DIR/bin"
OUT_BIN="$OUT_DIR/tfm"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
err()   { echo -e "${RED}[ERROR]${NC} $*"; }

find_cc() {
    if command -v musl-gcc &>/dev/null; then
        echo "musl-gcc"
        return 0
    fi
    if [ -x /usr/local/musl/bin/musl-gcc ]; then
        echo "/usr/local/musl/bin/musl-gcc"
        return 0
    fi
    for arch in x86_64-linux-musl aarch64-linux-musl arm-linux-musleabihf; do
        local cc="${arch}-gcc"
        if command -v "$cc" &>/dev/null; then
            echo "$cc"
            return 0
        fi
    done
    return 1
}

install_musl_debian() {
    warn "musl toolchain not found. Attempting to install musl-tools..."
    if ! command -v apt-get &>/dev/null; then
        err "apt-get not found. Please install musl-gcc manually."
        exit 1
    fi
    if [ "$(id -u)" -ne 0 ]; then
        err "Need root to install musl-tools. Run: sudo apt-get install -y musl-tools"
        err "Or install musl-gcc manually and re-run this script."
        exit 1
    fi
    apt-get update -qq && apt-get install -y -qq musl-tools
    if ! command -v musl-gcc &>/dev/null; then
        err "musl-gcc still not found after installation."
        exit 1
    fi
}

build() {
    local cc="$1" static="$2"

    info "Compiler: $cc"
    info "Source dir: $SRC_DIR"
    info "Output: $OUT_BIN"

    mkdir -p "$OUT_DIR"

    local cflags="-std=gnu11 -Wall -Wextra -O2"
    local ldflags="-lpthread"

    if [ "$static" = "1" ]; then
        cflags="$cflags -static"
        ldflags="-static $ldflags"
        info "Linking mode: static (musl)"
    fi

    local src_files=("$SRC_DIR"/*.c)
    if [ ${#src_files[@]} -eq 0 ]; then
        err "No source files found in $SRC_DIR"
        exit 1
    fi

    echo "$cc $cflags ${src_files[*]} -o $OUT_BIN $ldflags"
    $cc $cflags "${src_files[@]}" -o "$OUT_BIN" $ldflags

    if command -v strip &>/dev/null; then
        strip "$OUT_BIN" 2>/dev/null || true
    fi

    local size
    size=$(stat -c%s "$OUT_BIN" 2>/dev/null || stat -f%z "$OUT_BIN" 2>/dev/null || echo "?")
    local size_kb=$(( size / 1024 ))
    info "Binary size: ${size_kb} KB"

    # Verify it's actually static
    if command -v file &>/dev/null; then
        local ftype
        ftype=$(file "$OUT_BIN")
        info "File type: $ftype"
        if echo "$ftype" | grep -q "statically linked"; then
            info "Verified: statically linked"
        elif echo "$ftype" | grep -q "dynamically linked"; then
            warn "Binary is dynamically linked - musl may not be installed"
        fi
    fi

    # Check ldd
    if command -v ldd &>/dev/null; then
        info "Dynamic dependencies:"
        ldd "$OUT_BIN" 2>&1 || echo "  (none - statically linked)"
    fi

    echo ""
    info "Build complete: $OUT_BIN"
}

main() {
    echo ""
    info "=============================================="
    info "  tfml static build script (musl-based)"
    info "=============================================="
    echo ""

    local cc static=1

    cc=$(find_cc) || true

    if [ -z "$cc" ]; then
        # Try to install musl on Debian
        install_musl_debian
        cc=$(find_cc) || true
        if [ -z "$cc" ]; then
            err "Cannot find musl-gcc. Falling back to system gcc."
            warn "The resulting binary will NOT be statically linked."
            cc="gcc"
            static=0
        fi
    fi

    build "$cc" "$static"

    # Copy theme file alongside binary for bootstrap
    if [ -f "$THEMES_DIR/default.json" ]; then
        mkdir -p "$OUT_DIR/themes"
        cp "$THEMES_DIR/default.json" "$OUT_DIR/themes/default.json"
    fi
}

main "$@"
