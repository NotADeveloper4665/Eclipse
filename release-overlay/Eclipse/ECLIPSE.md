# Eclipse

A Moonlight-derived desktop streaming client with an in-session Quick Menu inspired by Shadow PC's layout. Its overlay uses Moonlight's Material dark canvas and surfaces, indigo primary and pink accent, with the system UI font. The original Moonlight copyright notices, GPL license, protocol and streaming implementation are retained. Eclipse is not affiliated with Shadow.

## Controls

- Open/close: Alt+Super+O (Windows/Linux), Alt+Command+O (macOS), or Ctrl+Alt+Shift+O.
- Controller: hold Back/Select and press Start.
- Navigate: Tab/Shift+Tab or D-pad up/down; activate with Enter/A.
- Change values: left/right; close with Escape/B.
- Mouse/touch: select navigation, buttons, setting values or bandwidth segments.

The menu contains General, Video & Display, Audio, Controllers & Input, Network and Help panels, a top action bar, and a right-side telemetry panel. Resolution, frame rate, video codec and audio channel selections use dropdown menus; toggles remain direct controls. It scales to the streaming window, including high-DPI displays. The stream continues playing behind it.

Mute, fullscreen, cursor release, performance HUD and Ctrl+Alt+Del act immediately. Resolution, FPS, codec, HDR, V-Sync, pacing, audio channels, absolute mouse, multiple controllers and bitrate are saved for the next connection. Reconnect saves the draft and waits for the old session to finish cleanup before starting a replacement. Disconnect leaves the host app running and requires a second click to confirm.

The usage panel reads actual decoder telemetry: rendered FPS, RTT, decoding time, network frame loss and measured bandwidth, with compact history graphs. Network frame loss is deliberately not labeled packet loss. The editable maximum bitrate is a configured limit; the Usage stats bandwidth value is measured traffic. Unsupported telemetry shows “Waiting for data.” USB and microphone forwarding are not provided by this protocol.

Remote keyboard/mouse/controller input is suppressed while the menu is open. Opening releases held keys, mouse buttons and controller state; closing swallows menu key/button releases before restoring the previous cursor capture state. Gamepad mouse-emulation timers are suppressed while the menu is active.

## Build

Follow the dependencies in [README.md](README.md). The source ZIP includes the populated submodules; skip the first command when building from that ZIP. For a Git checkout:

```sh
git submodule update --init --recursive
mkdir build
cd build
qmake6 ../moonlight-qt.pro
make -j4
./app/moonlight
```

Qt 5.12+ is also supported using `qmake` instead of `qmake6`. The executable name and settings namespace remain Moonlight-compatible; the application display name and Quick Menu identify Eclipse.

## Implementation

Moonlight suspends its Qt event loop during streaming. `app/streaming/controlcenter.*` therefore rasterizes the UI using QPainter into a dedicated `OverlayControlCenter` surface consumed by the existing SDL, EGL, Vulkan, Direct3D, VAAPI, VDPAU, DRM and macOS renderers. The native macOS sample-layer renderer uses an NSImageView for this surface. Steam Link shares its one overlay slot with status messages, prioritizing the menu.

## Verification scope

See the validation report delivered with this source. A live Sunshine host is required to verify end-to-end streaming, reconnects, real input capture, and hardware renderer behavior. Windows/macOS code requires native builds; Linux compilation cannot establish their correctness.

Run the focused UI checks in a separate build directory:

```sh
mkdir test-build
cd test-build
qmake6 ../tests/controlcenter.pro
make -j4
QT_QPA_PLATFORM=offscreen ./controlcenter-test
```
