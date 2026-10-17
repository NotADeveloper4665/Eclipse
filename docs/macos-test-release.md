# Eclipse 0.5.8 macOS Apple Silicon Test 1

Native arm64 test package for M1 and newer Apple Silicon Macs running macOS 15 or newer. This is a separate prerelease; Fedora RPM and Flatpak downloads remain on their existing tracks.

## Combined implementation

Based on Eclipse main b6ba569 and selected macOS behavior from wjbeckett/artemis afe2de7f2b24a2f6161f5672e8aa38450fa793ef (GPL-3.0, upstream attribution retained).

- Retains Eclipse profiles, pairing, control center, statistics and recording code.
- Integrates Artemis's macOS SDL callback-buffer workaround for Bluetooth HFP audio, while retaining Eclipse's WAN audio queue limits.
- Selects native VideoToolbox/Metal by default, following Artemis's macOS preference. Retains Eclipse's newer Metal/HDR/frame-pacing implementation and its AVSampleBufferDisplayLayer fallback, rather than downgrading to Artemis's older renderer.
- Adds Artemis's CoreAudio/AudioUnit framework linkage.
- Native arm64 app, Eclipse bundle identity and independent Mac settings, bundled Qt/FFmpeg/SDL dependencies, ad-hoc signing and a drag-to-Applications DMG.
- Uses one consistent native Homebrew FFmpeg installation, including libavformat for recording. The standard Moonlight dependency archive omits that recording library.

## Install and test

Download `Eclipse-macOS-arm64-test.dmg`, open it and drag Eclipse to Applications. The app is ad-hoc signed and **not Apple-notarized**. Gatekeeper may require an explicit Open Anyway action in System Settings → Privacy & Security after the first launch attempt. Do not disable Gatekeeper globally.

Start with H.264 or HEVC, hardware decoding and the automatic/native Metal renderer. On M1, AV1 hardware decoding is unavailable; use H.264/HEVC for the hardware path. Check the pipeline/decoder information in stream statistics and the session log.

## Verified versus pending

CI checks native arm64 compilation, version startup, bundle identity, bundled Mach-O architectures and runtime dependency paths, ad-hoc signature verification, DMG creation, checksums and source provenance. This is **not** proof of a successful live M1 streaming session.

Still needs real-device testing: pairing with Syzygy, sustained H.264/HEVC streaming, HDR display behavior, controller/mouse capture, Bluetooth audio, sleep/wake, recording playback and cellular/WAN behavior. Microphone/camera/USB forwarding depend on platform and host support and are not promised as Mac-compatible in this test.

Vulkan/libplacebo and FSR are excluded from this first native Metal build; do not enable FSR. QUIC helper dependencies are not bundled in this test; use the standard GameStream transport. Linux-only Gamescope features are not available on macOS. The existing main UI and overlay layout are preserved; this release does not implement a UI redesign.

Report the Mac model, macOS version, host GPU, codec, stream resolution/FPS, connection type and relevant pipeline log when testing. Include `build-info.json` to identify the exact source.
