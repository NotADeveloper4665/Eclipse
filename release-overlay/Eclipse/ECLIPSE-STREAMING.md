# Eclipse 0.4.3 streaming controls

## Saved stream profiles

The PC list toolbar adds a profile selector immediately before the Tailscale button. `Per-PC settings` keeps the existing behavior: each machine uses its own saved settings when available, otherwise it uses global settings. Selecting a saved profile applies that profile to new sessions regardless of the destination PC. Existing per-PC profile save/update and clear actions remain available in each PC's context menu. Renaming a PC also refreshes its saved profile label.

Profiles retain their machine-specific resolution, frame rate, bitrate, audio layout, FSR preset, and minimum-latency value. The active selector choice is saved locally. It never changes an already-running session.

## Minimum latency

The minimum-latency slider buffers decoded frames on a steady release schedule from 0 to 50 ms. The scheduler measures each frame's queue residence and gradually shifts the schedule toward the chosen minimum. Release timing is not driven by the arrival time of each individual frame, which helps decouple display cadence from network or host capture jitter. Higher values add latency; zero leaves the existing pacing path unchanged.

The scheduler increases FFmpeg's decoder frame allowance to account for the added queue, bounds the queue, and drops old frames under sustained overload. It does not alter the stream protocol or host capture settings. Actual results depend on host frame pacing, refresh rate, hardware decoder buffers, and network jitter; physical hardware testing remains necessary.

The setting is exposed in the application settings page and the CLI as `--minimum-latency <0-50>`. It is saved globally and included in saved per-PC profiles.

## Stream recording

The General page of the in-stream Quick Menu has Start stream recording and Stop recording controls. Recordings are saved as timestamped Matroska (`.mkv`) files under the user's Videos/Eclipse folder (or `~/Videos/Eclipse` when the system does not report a Videos folder). The client copies the incoming H.264, HEVC, or AV1 video and Opus audio packets into the file; it does not decode and re-encode them. Audio is recorded even when local playback is muted.

Recording is unavailable while client FSR upscaling is enabled. Select Off under Settings → Basic Settings → Client upscaling (FSR), then start a new stream to record.

Muxing and file writes run on a bounded background queue so disk stalls do not block video decode or audio playback. If the writer falls behind, recording stops with an error and the stream continues. Stopping recording finalizes the Matroska index; ending a streaming session also drains and closes the file. The saved file path and recorder state appear in the Quick Menu.

## Compact performance HUD

The Ctrl+Alt+Shift+S overlay renders three lines in a translucent rounded card with an accent bar: stream resolution, codec, and FPS; ping, decode time, and render time; then host processing time, network loss, and pacer drops attributed to jitter. The verbose statistics text remains available to logs and the control-center stats snapshot. Windows uses Segoe UI when installed; other platforms use the bundled ModeSeven font.
