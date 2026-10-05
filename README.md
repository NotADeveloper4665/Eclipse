# Eclipse

Eclipse is a Moonlight desktop streaming client with an in-session control center, Tailscale discovery, per-PC stream profiles, and decoder diagnostics. It retains Moonlight's license and upstream notices.

## Development toward Eclipse 0.4.3

Eclipse 0.4.3 builds on the FSR and Linux device-forwarding release with selectable saved stream profiles in the top bar, adjustable minimum-latency frame pacing, and a compact in-stream performance HUD. The FFmpeg path remains the cross-vendor decoder and renderer foundation. FSR is off by default. Device forwarding requires explicit selection and host setup; Windows/Shadow receivers are not implemented. See [setup, implementation, and limits](release-overlay/Eclipse/ECLIPSE-DEVICES.md).

The settings layout now stacks columns in small windows, wraps forwarding buttons, and collapses device controls by default. A dead build option referencing an absent settings test has been replaced by a working UI integration test mode.

## Releases

The [Eclipse 0.04.1 release](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.04.1) introduced FSR and device forwarding. Work toward 0.4.3 adds profile selection, jitter buffering, and compact stream stats; it is not yet tagged as a release.

## Source and checks

Extract `Eclipse-source.zip` and copy `release-overlay/Eclipse/.` over the extracted `Eclipse/` tree. Open `moonlight-qt.pro` in Qt Creator or use qmake. The release workflow assembles the same source and builds an RPM on pushes to `main`. `packaging/` contains the Fedora spec; `.github/workflows/` contains source checks and release packaging.

Focused control-center, forwarding, QML runtime, and Vulkan shader tests are included in the assembled `tests/` directory. See [validation notes](release-overlay/Eclipse/ECLIPSE-VALIDATION.md).
