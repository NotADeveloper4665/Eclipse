#!/usr/bin/env bash
set -euxo pipefail
run_bounded() {
  python3 - "$@" <<'PYTHON'
import subprocess, sys
subprocess.run(sys.argv[2:], check=True, timeout=int(sys.argv[1]))
PYTHON
}

# Native Apple Silicon release build. Requires Qt 6.8.3 and Homebrew
# FFmpeg/SDL2/Opus/OpenSSL. Build/package only; no test suite is run here.
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"
test "$(uname -s)" = Darwin
test "$(uname -m)" = arm64
export MACOSX_DEPLOYMENT_TARGET=15.0
export ECLIPSE_VERSION="${ECLIPSE_VERSION:-0.5.9}"
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

# Bundle Qt/QML plugins and all native runtime libraries into Eclipse.app.
run_bounded 600 macdeployqt "$app" -qmldir="$source_root/app/gui" -always-overwrite
# Keep SQLite for QML local storage; remove optional SQL drivers requiring
# separate database installations.
if [ -d "$app/Contents/PlugIns/sqldrivers" ]; then
  find "$app/Contents/PlugIns/sqldrivers" -type f -name '*.dylib' ! -name 'libqsqlite.dylib' -delete
fi
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

# Ship the exact overlaid source used for this binary alongside the installer.
cd "$repo_root/macos-work/source"
zip -qry "$repo_root/macos-work/deliverables/Eclipse-$ECLIPSE_VERSION-macOS-source.zip" Eclipse
