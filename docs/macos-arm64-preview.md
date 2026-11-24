# Eclipse 0.5.9 — macOS Apple Silicon Preview

Native arm64 build for M1/M2/M3/M4 Macs running macOS 15 or newer. This is a separate macOS preview; Fedora RPM and Flatpak release tracks are unchanged.

## Included

- Eclipse desktop client, existing profiles, pairing, control center, statistics and recording.
- Native VideoToolbox hardware decoding and Metal presentation defaults.
- Bundled Qt/QML runtime, FFmpeg, SDL and required audio/runtime libraries.
- CoreAudio/AudioUnit support and the macOS Bluetooth HFP callback-buffer workaround.
- Drag-to-Applications DMG installer.
- Matching source snapshot, build provenance and SHA-256 checksums.

The native build uses a single Homebrew FFmpeg installation, including libavformat required by recording. Vulkan/libplacebo, FSR, QUIC helper dependencies and Linux-only Gamescope integrations are not included in this macOS preview. Use the standard GameStream transport. On M1, use H.264 or HEVC for hardware decoding; AV1 hardware decoding is not available.

## Install

Download the DMG, open it, and drag Eclipse to Applications. This preview is ad-hoc signed and is not notarized by Apple. Gatekeeper may require an explicit Open Anyway action in System Settings → Privacy & Security after the first launch attempt. Do not disable Gatekeeper globally.

## Build scope

This workflow produces a native arm64 application bundle and installer. Per request, it does not run the project test suite or live-stream tests. Bundle architecture/dependency checks and ad-hoc signature verification remain part of packaging integrity. A successful build does not establish that every host/codec/controller/audio combination has been validated on physical hardware.

## Syzygy

The existing Eclipse Syzygy pairing implementation is retained. Add the host by IP or local discovery, select **Syzygy pairing phrase**, and enter the host's six-word phrase. The paired certificate is retained for subsequent connections.
