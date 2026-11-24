# Eclipse 0.5.11 — macOS Apple Silicon Preview

Native arm64 build for M1/M2/M3/M4 Macs running macOS 15 or newer. This is a separate macOS preview; Fedora RPM and Flatpak release tracks are unchanged.

## Cross-platform features

- Tailscale peer discovery from the PC list when the official Tailscale app is installed and signed in.
- Verified Syzygy automatic Tailscale pairing through the signed Tailscale.app CLI and local daemon identity query. If verification cannot be completed, use the six-word pairing phrase.
- AVFoundation microphone and camera capture, forwarded over authenticated SSH to the existing Linux host virtual audio/camera receivers. macOS camera/microphone permissions are requested by the app.
- Authenticated QUIC client transport with a bundled, self-contained ARM64 Python/aioquic executable. The Syzygy host still needs QUIC enabled and its UDP listener reachable.
- Existing saved stream profiles, stream recording, performance HUD/control center, VideoToolbox decoding, Metal presentation, CoreAudio and SDL3 runtime.

## Linux-only capabilities

- USB/IP physical device forwarding remains Linux client + Linux host only; it requires Linux kernel USB/IP drivers and PolicyKit.
- Wi-Fi power-save tuning remains Linux-only; macOS does not expose the Linux `iw`/PolicyKit interface used for reversible radio power changes.
- Client FSR remains unavailable in this macOS build because its current renderer uses the Vulkan/libplacebo path, which is not enabled in this packaging configuration. Do not select FSR on macOS.

## Install

Download and extract the ZIP, open the DMG, and drag Eclipse to Applications. This preview is ad-hoc signed and not Apple-notarized. Gatekeeper may require an explicit Open Anyway action in System Settings → Privacy & Security after the first launch attempt. Do not disable Gatekeeper globally. Homebrew, Python and aioquic are not required on the target Mac; Tailscale is an optional separate installation for Tailscale features.

## Scope

The build targets Apple Silicon and macOS 15+. The project test suite and live-stream tests are skipped by request. Bundle architecture/dependency checks and ad-hoc signature verification remain part of packaging integrity. A successful build does not establish that every host/codec/controller/audio combination has been validated on physical hardware. Physical streaming, TCC microphone/camera permissions, Tailscale auto-pairing and QUIC need validation on the target Mac.

Eclipse retains its existing Syzygy pairing implementation. Enter the host's six-word pairing phrase when Tailscale is unavailable or automatic identity verification cannot be completed.
