# Eclipse 0.5.10 — macOS Apple Silicon Preview

Native arm64 build for M1/M2/M3/M4 Macs running macOS 15 or newer. This is a separate macOS preview; Fedora RPM and Flatpak release tracks are unchanged.

The application bundle includes the SDL2 compatibility library and its SDL3 runtime dependency, alongside Qt/QML, FFmpeg, CoreAudio and the other required runtime libraries. SDL3 is bundled inside the app; Homebrew is not required on the target Mac.

## Install

Download and extract the ZIP, open the DMG, and drag Eclipse to Applications. This preview is ad-hoc signed and not Apple-notarized. Gatekeeper may require an explicit Open Anyway action in System Settings → Privacy & Security after the first launch attempt. Do not disable Gatekeeper globally.

## Scope

The build targets Apple Silicon and macOS 15+. The project test suite and live-stream tests are skipped by request. Bundle architecture/dependency checks and ad-hoc signature verification remain part of packaging integrity. A successful build does not establish that every host/codec/controller/audio combination has been validated on physical hardware.

Eclipse retains its existing Syzygy pairing implementation. Enter the host's six-word pairing phrase when pairing.
