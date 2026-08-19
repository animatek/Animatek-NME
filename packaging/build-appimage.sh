#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-release"
APPDIR="$ROOT_DIR/build-appimage/AppDir"
TOOLS_DIR="$ROOT_DIR/tools"
DIST_DIR="$ROOT_DIR/dist"
VERSION="${VERSION:-0.8.0}"
APPIMAGE="$DIST_DIR/AnimatekNME-${VERSION}-x86_64.AppImage"

LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_EXTRACTED="$TOOLS_DIR/squashfs-root"
LINUXDEPLOY_RUN="$LINUXDEPLOY_EXTRACTED/AppRun"
APPIMAGETOOL="$LINUXDEPLOY_EXTRACTED/plugins/linuxdeploy-plugin-appimage/usr/bin/appimagetool"
RUNTIME="$TOOLS_DIR/runtime-x86_64"
# linuxdeploy shells out to patchelf to set the rpath and dies without it. It is
# not installed on every distro, so fetch it here the way linuxdeploy itself is
# fetched rather than making the build depend on a system package.
PATCHELF_DIR="$TOOLS_DIR/patchelf"
PATCHELF="$PATCHELF_DIR/bin/patchelf"

LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
RUNTIME_URL="https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64"
PATCHELF_URL="https://github.com/NixOS/patchelf/releases/download/0.18.0/patchelf-0.18.0-x86_64.tar.gz"

mkdir -p "$TOOLS_DIR" "$DIST_DIR"

if [[ ! -f "$LINUXDEPLOY" ]]; then
    curl -L "$LINUXDEPLOY_URL" -o "$LINUXDEPLOY"
    chmod +x "$LINUXDEPLOY"
fi

if [[ ! -x "$LINUXDEPLOY_RUN" ]]; then
    (cd "$TOOLS_DIR" && ./linuxdeploy-x86_64.AppImage --appimage-extract >/dev/null)
fi

if [[ ! -f "$RUNTIME" ]]; then
    curl -L "$RUNTIME_URL" -o "$RUNTIME"
fi

if ! command -v patchelf >/dev/null && [[ ! -x "$PATCHELF" ]]; then
    mkdir -p "$PATCHELF_DIR"
    curl -L "$PATCHELF_URL" | tar xz -C "$PATCHELF_DIR"
    chmod +x "$PATCHELF"
fi
if [[ -x "$PATCHELF" ]]; then
    export PATH="$PATCHELF_DIR/bin:$PATH"
fi

if [[ ! -f "$ROOT_DIR/data/images/app-icon-512.png" ]]; then
    magick "$ROOT_DIR/data/images/app-icon.png" -resize 512x512 "$ROOT_DIR/data/images/app-icon-512.png"
fi

rm -rf "$APPDIR"

cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR" -j"$(nproc)"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"

"$LINUXDEPLOY_RUN" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/com.animatek.AnimatekNME.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/512x512/apps/com.animatek.AnimatekNME.png" \
    --exclude-library='libbrotli*' \
    --exclude-library='libbz2*' \
    --exclude-library='libpng16*'

"$APPIMAGETOOL" \
    --runtime-file "$RUNTIME" \
    -n \
    "$APPDIR" \
    "$APPIMAGE"

echo "Created $APPIMAGE"
