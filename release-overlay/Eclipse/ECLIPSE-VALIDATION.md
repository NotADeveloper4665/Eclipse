# Eclipse validation

Validated on 2026-10-02 against the cloned upstream Moonlight Qt source.

## Completed

- Linux x86-64 release build and link with Qt 5.15.13, SDL 2.30, and FFmpeg 6.1 development libraries.
- Build used `disable-prebuilts`, `disable-libdrm`, `disable-wayland`, `disable-libplacebo`, and `disable-x11`, exercising the SDL software video renderer and FFmpeg decoder. These flags describe the validation environment, not defaults in Eclipse's source.
- Built executable starts and returns command-line help successfully.
- `qmllint` parses the modified `StreamSegue.qml` successfully.
- Focused tests compile the production control-center and preferences code, then verify staged edits, explicit save, reload from actual isolated QSettings files, bandwidth selection, fullscreen callback, two-step disconnect confirmation, Escape/controller B close, input consumption, and stream-event pass-through.
- Mouse targeting and surface sizing checked at 480x320, 800x600, 1920x1080, and 3840x2160.
- All six panels rendered and visually inspected. Preview image is an offscreen render of the production UI. No host is connected, so telemetry accurately shows “Waiting for data.”
- `git diff --check` passes.

## Still requires a live host or native environment

- End-to-end Sunshine streaming, negotiated settings changes, automatic reconnect and error recovery.
- Physical keyboard/mouse/controller capture, remote input neutralization, focus changes and controller hotplug during a session.
- GPU backend rendering, HDR compositing, high-DPI behavior on real displays and hardware overlays.
- Windows and macOS compilation and runtime checks; native macOS sample-layer handling is source-integrated but not native-tested.
- Steam Link's single-overlay compatibility path.

The implementation is reviewable source, not a fully validated release binary. No live-stream performance numbers are claimed.

## Publication

The full Eclipse source is published in `NotADeveloper4665/Eclipse`, with Moonlight license and upstream copyright notices retained. Fedora RPM builds run from the versioned GitHub release workflow.
