# Eclipse

Eclipse is a Moonlight desktop streaming client with an in-session control center, Tailscale discovery, per-PC stream profiles, and decoder diagnostics. It retains Moonlight's license and upstream notices.

## Eclipse 0.5.3

Eclipse 0.5.3 adds optional Vulkan Mailbox presentation (Fast-Sync) with FIFO fallback when the display surface does not support it. Recording is unavailable while client FSR is active, and the settings page has clearer descriptions and grouping. The 0.5.2 release added guest input permissions, adaptive bitrate for compatible Sunshine hosts, and direct desktop launch. See [release notes](release-notes-0.5.3.md), [recording and stream details](release-overlay/Eclipse/ECLIPSE-STREAMING.md), [device setup and limits](release-overlay/Eclipse/ECLIPSE-DEVICES.md), and [Flatpak installation notes](packaging/flatpak/README.md).

The settings layout now stacks columns in small windows, wraps forwarding buttons, and collapses device controls by default. A dead build option referencing an absent settings test has been replaced by a working UI integration test mode.

## Releases

Download the current [Eclipse 0.5.3 release](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.5.3). The [Eclipse 0.04.1 release](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.04.1) introduced FSR and device forwarding.

## Source and checks

Extract `Eclipse-source.zip` and copy `release-overlay/Eclipse/.` over the extracted `Eclipse/` tree. Open `moonlight-qt.pro` in Qt Creator or use qmake. The release workflow assembles the same source and builds an RPM on pushes to `main`. `packaging/` contains the Fedora spec; `.github/workflows/` contains source checks and release packaging.

Focused control-center, forwarding, QML runtime, and Vulkan shader tests are included in the assembled `tests/` directory. See [validation notes](release-overlay/Eclipse/ECLIPSE-VALIDATION.md).
