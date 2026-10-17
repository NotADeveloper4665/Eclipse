#!/usr/bin/env bash
set -euo pipefail
# Native arm64 test build. Requires Qt 6.8.3, Homebrew FFmpeg/SDL2/Opus/OpenSSL.
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"
test "$(uname -s)" = Darwin
test "$(uname -m)" = arm64
export MACOSX_DEPLOYMENT_TARGET=15.0
export PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig:$(brew --prefix ffmpeg)/lib/pkgconfig:$(brew --prefix)/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
mkdir -p macos-work/source macos-work/build
unzip -q -o Eclipse-source.zip -d macos-work/source
cp -a release-overlay/Eclipse/. macos-work/source/Eclipse/
source_root="$repo_root/macos-work/source/Eclipse"
printf '%s\n' 0.5.8 > "$source_root/app/version.txt"
# Use one consistent FFmpeg installation: upstream prebuilt archives do not
# include libavformat, which Eclipse needs for stream recording.
cd macos-work/build
qmake6 "$source_root/moonlight-qt.pro" CONFIG+=release CONFIG-=debug_and_release \
  CONFIG+=disable-prebuilts CONFIG+=disable-libplacebo CONFIG+=disable-x11 \
  CONFIG+=disable-wayland CONFIG+=disable-libva CONFIG+=disable-libvdpau \
  QMAKE_APPLE_DEVICE_ARCHS=arm64 QMAKE_MACOSX_DEPLOYMENT_TARGET=15.0
make -j3
app="$PWD/app/Eclipse.app"
test -x "$app/Contents/MacOS/Eclipse"
lipo -verify_arch arm64 "$app/Contents/MacOS/Eclipse"
QT_QPA_PLATFORM=offscreen "$app/Contents/MacOS/Eclipse" --version
# Exercise Eclipse's actual muxer against the native FFmpeg used by the app.
mkdir -p "$repo_root/macos-work/recording-test"
pushd "$repo_root/macos-work/recording-test"
qmake6 "$source_root/tests/recording.pro" QMAKE_APPLE_DEVICE_ARCHS=arm64 QMAKE_MACOSX_DEPLOYMENT_TARGET=15.0
make -j3
ffmpeg -hide_banner -loglevel error -f lavfi -i testsrc2=size=320x240:rate=30 -t 2 \
  -c:v libx264 -preset ultrafast -tune zerolatency -x264-params keyint=30 -f h264 video.h264
ffmpeg -hide_banner -loglevel error -f lavfi -i sine=frequency=440:sample_rate=48000 -t 2 \
  -c:a libopus -b:a 96k -f ogg audio.ogg
./recording-smoke video.h264 audio.ogg recording-h264.mkv h264
ffmpeg -hide_banner -loglevel error -i recording-h264.mkv -f null -
popd
macdeployqt "$app" -qmldir="$source_root/app/gui" -always-overwrite
python3 "$repo_root/scripts/verify-macos-bundle.py" "$app"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict --verbose=2 "$app"
mkdir -p "$repo_root/macos-work/deliverables" "$repo_root/macos-work/dmg"
cp -R "$app" "$repo_root/macos-work/dmg/"
ln -s /Applications "$repo_root/macos-work/dmg/Applications"
hdiutil create -volname 'Eclipse macOS Test' -srcfolder "$repo_root/macos-work/dmg" \
  -ov -format UDZO "$repo_root/macos-work/deliverables/Eclipse-macOS-arm64-test.dmg"
# Produce a complete corresponding source snapshot, including the base ZIP contents.
cd "$repo_root/macos-work/source"
zip -qry "$repo_root/macos-work/deliverables/Eclipse-macOS-test-source.zip" Eclipse
