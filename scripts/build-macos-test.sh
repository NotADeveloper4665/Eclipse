#!/usr/bin/env bash
set -euxo pipefail
run_bounded() {
  python3 - "$@" <<'PYTHON'
import subprocess, sys
subprocess.run(sys.argv[2:], check=True, timeout=int(sys.argv[1]))
PYTHON
}

# Native Apple Silicon release build. Requires Qt 6.8.3 and Homebrew
# FFmpeg/SDL2-compat/SDL3/Opus/OpenSSL.
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"
test "$(uname -s)" = Darwin
test "$(uname -m)" = arm64
export MACOSX_DEPLOYMENT_TARGET=15.0
export ECLIPSE_VERSION="${ECLIPSE_VERSION:-0.6.1-dev}"
export PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig:$(brew --prefix ffmpeg)/lib/pkgconfig:$(brew --prefix)/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
mkdir -p macos-work/source macos-work/build
unzip -q -o Eclipse-source.zip -d macos-work/source
cp -a release-overlay/Eclipse/. macos-work/source/Eclipse/
source_root="$repo_root/macos-work/source/Eclipse"
printf '%s\n' "$ECLIPSE_VERSION" > "$source_root/app/version.txt"

# Use one consistent native FFmpeg installation, including libavformat
# required by Eclipse's recording implementation.
cd macos-work/build
qmake6 "$source_root/moonlight-qt.pro" CONFIG+=release CONFIG-=debug_and_release \
  CONFIG+=disable-prebuilts CONFIG+=disable-libplacebo CONFIG+=disable-x11 \
  CONFIG+=disable-wayland CONFIG+=disable-libva CONFIG+=disable-libvdpau \
  QMAKE_APPLE_DEVICE_ARCHS=arm64 QMAKE_MACOSX_DEPLOYMENT_TARGET=15.0
make -j3

app="$PWD/app/Eclipse.app"
test -x "$app/Contents/MacOS/Eclipse"
lipo "$app/Contents/MacOS/Eclipse" -verify_arch arm64

# Bundle Qt/QML plugins and native runtime libraries into Eclipse.app.
# Ship the FFmpeg CLI used by AVFoundation forwarding, then let
# macdeployqt resolve its non-system dylib dependencies into the app bundle.
cp -Lf "$(brew --prefix ffmpeg)/bin/ffmpeg" "$app/Contents/MacOS/ffmpeg"
chmod 755 "$app/Contents/MacOS/ffmpeg"
run_bounded 600 macdeployqt "$app" -qmldir="$source_root/app/gui" \
  -executable="$app/Contents/MacOS/ffmpeg" -always-overwrite
if [ -d "$app/Contents/PlugIns/sqldrivers" ]; then
  find "$app/Contents/PlugIns/sqldrivers" -type f -name '*.dylib' ! -name 'libqsqlite.dylib' -delete
fi

frameworks="$app/Contents/Frameworks"
# Homebrew's sdl2 formula is sdl2-compat on this runner. That shim is SDL2
# ABI-compatible but dynamically loads SDL3. macdeployqt does not reliably
# discover this runtime edge, so bundle SDL3 explicitly as well as the shim.
sdl3_lib="$(brew --prefix sdl3)/lib"
compgen -G "$sdl3_lib/libSDL3*.dylib" >/dev/null
for dylib in "$sdl3_lib"/libSDL3*.dylib; do
  [ -f "$dylib" ] || continue
  cp -Lf "$dylib" "$frameworks/$(basename "$dylib")"
done
# Ensure the app's executable rpath is available to transitive dependencies.
install_name_tool -add_rpath '@executable_path/../Frameworks' "$app/Contents/MacOS/Eclipse" 2>/dev/null || true

# FFmpeg/HTTP dependencies can also retain transitive Homebrew references.
brotli_lib="$(brew --prefix brotli)/lib"
for dylib in libbrotlicommon.1.dylib libbrotlicommon.dylib; do
  if [ -f "$brotli_lib/$dylib" ]; then
    cp -Lf "$brotli_lib/$dylib" "$frameworks/$dylib"
  fi
done
# Bundle authenticated QUIC as a self-contained ARM64 executable. The
# PyInstaller one-file binary carries Python, aioquic and cryptography so the
# installed app does not depend on Homebrew or a system Python environment.
python="$(brew --prefix python@3.13)/bin/python3.13"
venv="$repo_root/macos-work/quic-venv"
"$python" -m venv "$venv"
"$venv/bin/python" -m pip install --disable-pip-version-check --upgrade pip pyinstaller aioquic Pillow
"$venv/bin/python" "$repo_root/scripts/generate-eclipse-icon.py" "$app/Contents/Resources/Eclipse.icns"
mkdir -p "$app/Contents/Resources"
"$venv/bin/pyinstaller" --clean --onefile --target-arch arm64 --name syzygy-quic \
  --distpath "$app/Contents/MacOS" \
  --workpath "$repo_root/macos-work/pyinstaller-work" \
  --specpath "$repo_root/macos-work/pyinstaller-spec" \
  "$source_root/app/streaming/quic/syzygy-quic.py"
run_bounded 120 python3 "$repo_root/scripts/fix-macos-library-paths.py" "$app"
# These are packaging-integrity checks, not application test suites.
run_bounded 300 python3 "$repo_root/scripts/verify-macos-bundle.py" "$app"
run_bounded 180 codesign --force --deep --sign - "$app"
run_bounded 120 codesign --verify --deep --strict --verbose=2 "$app"

mkdir -p "$repo_root/macos-work/deliverables" "$repo_root/macos-work/dmg"
cp -R "$app" "$repo_root/macos-work/dmg/"
ln -s /Applications "$repo_root/macos-work/dmg/Applications"
hdiutil create -volname "Eclipse $ECLIPSE_VERSION" -srcfolder "$repo_root/macos-work/dmg" \
  -ov -format UDZO "$repo_root/macos-work/deliverables/Eclipse-$ECLIPSE_VERSION-macOS-arm64.dmg"

cd "$repo_root/macos-work/source"
zip -qry "$repo_root/macos-work/deliverables/Eclipse-$ECLIPSE_VERSION-macOS-source.zip" Eclipse
