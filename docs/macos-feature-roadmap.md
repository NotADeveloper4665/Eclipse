# Eclipse macOS: Compatibility Fixes and Native Feature Roadmap

**Target:** Eclipse 0.6.1-dev  
**Scope:** Apple Silicon, macOS 15+, existing Moonlight/Syzygy protocol compatibility  
**Status:** Implementation plan; features are not considered complete until a macOS build and device validation confirm them.

## 1. Immediate 0.6.0-dev defects

### 1.1 QUIC client bind failure

Observed:
- `CryptographyDeprecationWarning`: parsed certificate serial number is not positive.
- `[Errno 49] Can't assign requested address`.

Code inspection found that the macOS client chooses a random `127.90.x.y` address and binds local UDP/TCP proxy sockets to it. macOS does not guarantee arbitrary 127/8 addresses are configured as local interface addresses. The client should bind to `127.0.0.1`; retain the Linux behavior separately. If a required local port is occupied, return a clear port-conflict error instead of a generic QUIC failure.

The certificate warning is a separate problem. Syzygy's current `crypto::gen_creds()` source generates a positive random 159-bit serial, so the warning may come from an older persisted certificate or the Moonlight client identity certificate. Do not silently regenerate paired identities: replacing a certificate can invalidate existing host/client pairing. Add diagnostics that identify which certificate is invalid, then use a deliberate re-pair / host-certificate migration procedure. The warning is not itself the cause of Errno 49.

### 1.2 Tailscale CLI invocation

The macOS device model currently invokes the App Store bundle executable directly without setting the documented CLI-mode environment variable. It only searches PATH and the App Store bundle path, missing the standalone CLI launcher at `/usr/local/bin/tailscale`.

Implementation:
- Prefer the standalone CLI launcher when executable.
- Fall back to `/Applications/Tailscale.app/Contents/MacOS/Tailscale`.
- Set `TAILSCALE_BE_CLI=1` for CLI child processes.
- Preserve bounded process timeouts and show actionable errors for missing CLI, daemon stopped, NeedsLogin, and no peers.
- For automatic Syzygy pairing, keep strict peer identity checks (tailnet membership, not expired/shared peer, matching peer ID and whois identity). Do not require the Linux-style `TUN=true` flag on macOS; macOS networking is managed by the Tailscale app/Network Extension and the peer identity checks are the security boundary. Do not weaken the identity checks.

Official reference: https://tailscale.com/docs/reference/tailscale-cli?tab=macos

### 1.3 AVFoundation device enumeration

The current camera discovery path searches PATH for FFmpeg even though the app bundles FFmpeg. Use the same bundled executable resolver used by microphone discovery. FFmpeg's AVFoundation device listing is written to stderr and has log prefixes; parse only the device-index lines after stripping the FFmpeg log prefix. Preserve distinct audio/video sections. If macOS denies camera or microphone permission, surface the OS permission state / actionable guidance rather than "no device found".

AVFoundation is Apple's native audio/video capture framework; this build uses FFmpeg's AVFoundation input backend. It is not the Linux V4L2/PulseAudio backend.

## 2. Reddit research: requested Mac-focused features

The following themes were found in recent r/MoonlightStreaming and r/macgaming discussions. These are user reports and requests, not representative survey statistics.

| Community request / pain point | Evidence | Feasibility | Proposed priority |
|---|---|---|---|
| Client-side controller remapping: layers, gyro, trackpads, macros, keyboard/mouse mapping | https://www.reddit.com/r/MoonlightStreaming/comments/1xwgjkk/ | High for standard HID controllers; medium/low for Steam Controller raw trackpads/gyro without dedicated HID support or privileged driver | P2 |
| Reliable Mac microphone passthrough with permission handling | https://www.reddit.com/r/MoonlightStreaming/comments/1x3qbtz/ and https://www.reddit.com/r/MoonlightStreaming/comments/1xwgjkk/ | High; AVFoundation capture exists, needs reliable packaging, TCC handling, and host receiver interoperability | P0 |
| Built-in HDR-aware screenshots and recording | https://www.reddit.com/r/MoonlightStreaming/comments/1x3qbtz/ | High for SDR screenshots; medium for HDR because color space, EDR and tone mapping must be preserved or explicitly converted | P2 |
| Reusable profiles, Steam import, library/artwork, recent games/history | https://www.reddit.com/r/MoonlightStreaming/comments/1u9lmfs/ | High for local profiles/history; medium for automatic host library import because it depends on host API support and permissions | P2 |
| Better bitrate/latency telemetry and adaptive bitrate | https://www.reddit.com/r/MoonlightStreaming/comments/1x3qbtz/ | High for telemetry; medium for adaptive bitrate because host support and protocol capabilities vary | P1 |
| AWDL-related stutter diagnostics and safe workaround | https://www.reddit.com/r/MoonlightStreaming/comments/1wg38ba/ and https://www.reddit.com/r/MoonlightStreaming/comments/1w7hc59/ | High for diagnostics and per-session observation; low/unsafe for automatically disabling AWDL because it affects AirDrop/Continuity and macOS can re-enable it | P1 |
| Mac Game Mode integration, native overlay, picture-in-picture | https://www.reddit.com/r/MoonlightStreaming/comments/1wxuc2a/ | High for native macOS UI integration; PiP feasibility depends on renderer/window architecture and protected content behavior | P2 |
| Better keyboard layout / Command-to-Windows-key mapping | https://www.reddit.com/r/MoonlightStreaming/comments/1xwgjkk/ | High; map at client input translation layer with explicit per-profile options | P1 |
| Smooth frame pacing and audio stability on Apple Silicon | https://www.reddit.com/r/MoonlightStreaming/comments/1tzrf0u/ and https://www.reddit.com/r/MoonlightStreaming/comments/1wg38ba/ | High priority engineering area; requires Instruments/Metal/CoreAudio telemetry and real-device measurement, not a one-click tweak | P0 |

### Research takeaways

- The clearest Mac-specific differentiators are native controller/input handling, AVFoundation microphone capture, HDR-aware capture, native Game Mode/window behavior, and Apple Silicon frame pacing.
- Users repeatedly report AWDL-related stutter, but forcibly disabling AWDL is not an acceptable default: it disrupts AirDrop and Continuity. Start with diagnostics and a reversible opt-in experiment only if technically safe.
- A native Steam Controller/gyro remapper is attractive but is a larger project than ordinary controller mapping. Do not promise raw Steam Controller feature parity until HID access and macOS permissions are proven.
- Some requested features already exist in competing Mac forks. Eclipse should prioritize reliability and integration with Syzygy/Tailscale rather than feature-count inflation.

## 3. Roadmap

### P0 — Make the current Mac release dependable
1. QUIC local bind uses `127.0.0.1` on macOS; retain Linux path.
2. Identify the invalid certificate source and provide a safe repair/re-pair path without silently rotating identities.
3. Tailscale CLI discovery and CLI-mode environment; preserve strict peer identity checks.
4. AVFoundation bundled FFmpeg enumeration and permission diagnostics.
5. Build an actionable connection diagnostics panel that distinguishes discovery, pairing, certificate, socket-bind, QUIC handshake, media and permission failures.

### P1 — Mac reliability and input
1. Frame pacing and CoreAudio diagnostics (decode, render, present and audio queue timing).
2. Display refresh-rate and HDR/EDR capability detection; show effective stream/display mode.
3. Keyboard layout and Command/Option mapping profiles.
4. Tailscale connection health and route selection visibility.
5. AWDL state/latency diagnostics only; no automatic interface disabling.

### P2 — Mac-native differentiators
1. HDR-aware screenshot and short clip capture, with explicit color-space metadata.
2. Native macOS menu bar quick controls and session status.
3. Game Mode integration and PiP investigation.
4. Per-game/per-display profiles and optional Steam library metadata import.
5. Controller mapping editor; start with supported HID devices, then separately investigate Steam Controller raw sensors/trackpads.

## 4. Acceptance criteria

- macOS QUIC no longer attempts to bind an unconfigured random loopback IP.
- Tailscale CLI works with both standalone and App Store macOS installations, and automatic pairing still rejects unverified peers.
- AVFoundation lists real camera and microphone devices from the bundled FFmpeg binary; permission denial is distinguishable from no hardware.
- Invalid certificate serials are traced to a specific identity and repaired only through an explicit, pairing-safe procedure.
- Linux behavior remains unchanged.
- GitHub Actions build is green; physical Mac validation is still required for Tailscale daemon variants, camera/microphone permissions, QUIC reachability, HDR and frame pacing.

## 5. Sources

- Tailscale CLI for macOS, including `TAILSCALE_BE_CLI=1`: https://tailscale.com/docs/reference/tailscale-cli?tab=macos
- Mac-only Moonlight client feature discussion: https://www.reddit.com/r/MoonlightStreaming/comments/1x3qbtz/
- Controller remapping and microphone passthrough request: https://www.reddit.com/r/MoonlightStreaming/comments/1xwgjkk/
- Profiles, Steam library and artwork request: https://www.reddit.com/r/MoonlightStreaming/comments/1u9lmfs/
- AWDL / Apple Silicon micro-stutter reports: https://www.reddit.com/r/MoonlightStreaming/comments/1wg38ba/ and https://www.reddit.com/r/MoonlightStreaming/comments/1tzrf0u/
- AWDL workaround automation discussion: https://www.reddit.com/r/MoonlightStreaming/comments/1w7hc59/
- Native Mac client features (Game Mode, PiP, mic passthrough, adaptive triggers): https://www.reddit.com/r/MoonlightStreaming/comments/1wxuc2a/
