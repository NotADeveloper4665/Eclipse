# Eclipse

Eclipse is a Moonlight desktop streaming client with an in-session control center, Tailscale discovery, per-PC stream profiles, and decoder diagnostics. It retains Moonlight's license and upstream notices.

## Development toward Eclipse 0.05

Eclipse 0.05 continues the FSR and Linux device-forwarding work. The FSR and microphone, webcam, and USB forwarding panels now sit inside Basic Settings in Controls. Kyber transport and in-stream recording are planned for this release line; neither is implemented yet. Kyber requires a compatible Kyber host and transport stack, so existing Sunshine/GameStream hosts cannot use it through a client-only toggle. See [setup, implementation, and limits](release-overlay/Eclipse/ECLIPSE-DEVICES.md).

The settings layout now stacks columns in small windows, wraps forwarding buttons, and collapses device controls by default. A dead build option referencing an absent settings test has been replaced by a working UI integration test mode.

## Releases

The [Eclipse 0.04.1 release](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.04.1) introduced FSR and device forwarding. Eclipse 0.05 is in development and has not been tagged as a release.

## Source and checks

Extract `Eclipse-source.zip` and copy `release-overlay/Eclipse/.` over the extracted `Eclipse/` tree. Open `moonlight-qt.pro` in Qt Creator or use qmake. The release workflow assembles the same source and builds an RPM on pushes to `main`. `packaging/` contains the Fedora spec; `.github/workflows/` contains source checks and release packaging.

Focused control-center, forwarding, QML runtime, and Vulkan shader tests are included in the assembled `tests/` directory. See [validation notes](release-overlay/Eclipse/ECLIPSE-VALIDATION.md).
