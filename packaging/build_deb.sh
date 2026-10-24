#!/bin/bash
#
# build_deb.sh
#
# Copyright (C) 2026, Charles Chiou
#

set -e

if [ $# -lt 4 ]; then
    echo "Usage: $0 <server_binary_path> <cli_binary_path> <version> <output_dir> [arch]" >&2
    exit 1
fi

SERVER_BINARY_PATH="$1"
CLI_BINARY_PATH="$2"
VERSION="$3"
OUTPUT_DIR="$4"
ARCH="${5:-$(dpkg --print-architecture 2>/dev/null || echo "amd64")}"

if [ ! -f "$SERVER_BINARY_PATH" ]; then
    echo "Error: server binary not found: $SERVER_BINARY_PATH" >&2
    exit 1
fi

if [ ! -f "$CLI_BINARY_PATH" ]; then
    echo "Error: CLI binary not found: $CLI_BINARY_PATH" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"

# Ensure build/rom.bin and tapes/*.tape exist before staging
mkdir -p "${REPO_ROOT}/build/tapes"
if [ ! -f "${REPO_ROOT}/build/rom.bin" ] || [ "${REPO_ROOT}/src/rom.asm" -nt "${REPO_ROOT}/build/rom.bin" ]; then
    if ! command -v pasmo >/dev/null 2>&1; then
        echo "Error: pasmo not found; needed to assemble src/rom.asm" >&2
        exit 1
    fi
    pasmo --bin "${REPO_ROOT}/src/rom.asm" "${REPO_ROOT}/build/rom.bin"
fi

for asm in "${REPO_ROOT}"/tapes/[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F].asm; do
    [ -f "$asm" ] || continue
    base="$(basename "$asm" .asm)"
    bin_out="${REPO_ROOT}/build/tapes/${base}.bin"
    tape_out="${REPO_ROOT}/tapes/${base}.tape"
    if [ ! -f "$bin_out" ] || [ ! -f "$tape_out" ] || [ "$asm" -nt "$bin_out" ] || [ "$asm" -nt "$tape_out" ]; then
        if ! command -v pasmo >/dev/null 2>&1; then
            echo "Error: pasmo not found; needed to assemble $asm" >&2
            exit 1
        fi
        pasmo --bin "$asm" "$bin_out"
        (cd "${REPO_ROOT}" && "${CLI_BINARY_PATH}" -c "tape encode $asm $bin_out $tape_out" >/dev/null)
    fi
done

STAGING_DIR="${OUTPUT_DIR}/deb-staging/abc-80_${VERSION}_${ARCH}"
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR/DEBIAN"
mkdir -p "$STAGING_DIR/usr/sbin"
mkdir -p "$STAGING_DIR/usr/bin"
mkdir -p "$STAGING_DIR/lib/systemd/system"
mkdir -p "$STAGING_DIR/etc/default"
mkdir -p "$STAGING_DIR/etc/abc-80/build"
mkdir -p "$STAGING_DIR/etc/abc-80/vintage"
mkdir -p "$STAGING_DIR/etc/abc-80/web"
mkdir -p "$STAGING_DIR/etc/abc-80/tapes"
mkdir -p "$STAGING_DIR/etc/abc-80/exercises/qb"
mkdir -p "$STAGING_DIR/etc/abc-80/programs"
chmod 0755 "$STAGING_DIR/etc/abc-80"
chmod 0755 "$STAGING_DIR/etc/abc-80/build"
chmod 0755 "$STAGING_DIR/etc/abc-80/vintage"
chmod 0755 "$STAGING_DIR/etc/abc-80/web"
chmod 0755 "$STAGING_DIR/etc/abc-80/tapes"
chmod 0755 "$STAGING_DIR/etc/abc-80/exercises"
chmod 0755 "$STAGING_DIR/etc/abc-80/exercises/qb"
chmod 0755 "$STAGING_DIR/etc/abc-80/programs"

cp "$SERVER_BINARY_PATH" "$STAGING_DIR/usr/sbin/abc80_server"
chmod 0755 "$STAGING_DIR/usr/sbin/abc80_server"
cp "$CLI_BINARY_PATH" "$STAGING_DIR/usr/bin/abc80_cli"
chmod 0755 "$STAGING_DIR/usr/bin/abc80_cli"
if command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded "$STAGING_DIR/usr/sbin/abc80_server" || true
    strip --strip-unneeded "$STAGING_DIR/usr/bin/abc80_cli" || true
fi

cp "$SCRIPT_DIR/abc-80.service" "$STAGING_DIR/lib/systemd/system/abc-80.service"
chmod 0644 "$STAGING_DIR/lib/systemd/system/abc-80.service"

cp "$SCRIPT_DIR/abc-80.default" "$STAGING_DIR/etc/default/abc-80"
chmod 0644 "$STAGING_DIR/etc/default/abc-80"

cp "$SCRIPT_DIR/abc-80.cfg" "$STAGING_DIR/etc/abc-80/abc-80.cfg"
chmod 0644 "$STAGING_DIR/etc/abc-80/abc-80.cfg"

cp "${REPO_ROOT}/build/rom.bin" "$STAGING_DIR/etc/abc-80/build/rom.bin"
chmod 0644 "$STAGING_DIR/etc/abc-80/build/rom.bin"

cp "${REPO_ROOT}/vintage/rom.abc" "$STAGING_DIR/etc/abc-80/vintage/rom.abc"
chmod 0644 "$STAGING_DIR/etc/abc-80/vintage/rom.abc"
for vf in "${REPO_ROOT}"/vintage/*.bas "${REPO_ROOT}"/vintage/*.txt "${REPO_ROOT}"/vintage/*.opc; do
    if [ -f "$vf" ]; then
        cp "$vf" "$STAGING_DIR/etc/abc-80/vintage/"
        chmod 0644 "$STAGING_DIR/etc/abc-80/vintage/$(basename "$vf")"
    fi
done

cp -r "${REPO_ROOT}/web/." "$STAGING_DIR/etc/abc-80/web/"
find "$STAGING_DIR/etc/abc-80/web" -type d -exec chmod 0755 {} +
find "$STAGING_DIR/etc/abc-80/web" -type f -exec chmod 0644 {} +

for tf in "${REPO_ROOT}"/tapes/*.tape "${REPO_ROOT}"/tapes/*.bas "${REPO_ROOT}"/tapes/*.asm; do
    if [ -f "$tf" ]; then
        cp "$tf" "$STAGING_DIR/etc/abc-80/tapes/"
        chmod 0644 "$STAGING_DIR/etc/abc-80/tapes/$(basename "$tf")"
    fi
done

if [ -d "${REPO_ROOT}/exercises/qb" ]; then
    for ef in "${REPO_ROOT}"/exercises/qb/*; do
        if [ -f "$ef" ]; then
            cp "$ef" "$STAGING_DIR/etc/abc-80/exercises/qb/"
            chmod 0644 "$STAGING_DIR/etc/abc-80/exercises/qb/$(basename "$ef")"
        fi
    done
fi

cp "$SCRIPT_DIR/conffiles" "$STAGING_DIR/DEBIAN/conffiles"
chmod 0644 "$STAGING_DIR/DEBIAN/conffiles"

cp "$SCRIPT_DIR/postinst" "$STAGING_DIR/DEBIAN/postinst"
chmod 0755 "$STAGING_DIR/DEBIAN/postinst"

cp "$SCRIPT_DIR/prerm" "$STAGING_DIR/DEBIAN/prerm"
chmod 0755 "$STAGING_DIR/DEBIAN/prerm"

cp "$SCRIPT_DIR/postrm" "$STAGING_DIR/DEBIAN/postrm"
chmod 0755 "$STAGING_DIR/DEBIAN/postrm"

SHLIBS=""
if command -v dpkg-shlibdeps >/dev/null 2>&1; then
    TMP_SHLIB="$(mktemp -d)"
    mkdir -p "$TMP_SHLIB/debian"
    cat << 'EOF' > "$TMP_SHLIB/debian/control"
Source: abc-80
Package: abc-80
Architecture: any
Description: dummy
EOF
    SHLIBS=$( (cd "$TMP_SHLIB" && dpkg-shlibdeps -O -e"${STAGING_DIR}/usr/sbin/abc80_server" -e"${STAGING_DIR}/usr/bin/abc80_cli" 2>/dev/null) | sed -n 's/^shlibs:Depends=//p' )
    rm -rf "$TMP_SHLIB"
fi

if [ -z "$SHLIBS" ]; then
    SHLIBS="libc6 (>= 2.38), libconfig++9v5 (>= 1.5-0.4~), libgcc-s1 (>= 3.0), libstdc++6 (>= 13.1)"
fi
DEPS="${SHLIBS}"

INSTALLED_SIZE=$(du -sk "$STAGING_DIR" | awk '{print $1}')

cat << EOF > "$STAGING_DIR/DEBIAN/control"
Package: abc-80
Version: ${VERSION}
Section: otherosfs
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INSTALLED_SIZE}
Depends: ${DEPS}
Maintainer: Charles Chiou <samurai@selfso.com>
Homepage: https://www.selfso.com/software/ubuntu/
Description: ABC-80 Z80 Microcomputer Emulator and Web Front Panel Server
 Cycle-accurate Taiwanese ABC-80 Z80 single-board microcomputer emulator,
 interactive CLI, and WebSocket/HTTP front-panel server daemon.
EOF
chmod 0644 "$STAGING_DIR/DEBIAN/control"

(cd "$STAGING_DIR" && find usr lib etc -type f | sort | while read -r f; do md5sum "$f"; done) > "$STAGING_DIR/DEBIAN/md5sums"
chmod 0644 "$STAGING_DIR/DEBIAN/md5sums"

DEB_FILE="${OUTPUT_DIR}/abc-80_${VERSION}_${ARCH}.deb"
dpkg-deb --root-owner-group --build "$STAGING_DIR" "$DEB_FILE"

echo "Built Debian package: $DEB_FILE"
