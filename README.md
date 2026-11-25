# Eclipse

Eclipse is a Moonlight desktop streaming client with an in-session control center, Tailscale discovery, per-PC stream profiles, and decoder diagnostics. It retains Moonlight's license and upstream notices.

## Eclipse 0.5.4

Eclipse 0.5.4 adds key-based pairing for Syzygy hosts. Enter a Syzygy host key from the unpaired host's context menu; Eclipse uses it to prove access without sending or storing the key itself. A successful key pairing grants that client full host permissions, so protect the key. This release carries forward 0.5.3's Vulkan Mailbox presentation, FSR-aware recording control, and settings updates. See [release notes](release-notes-0.5.4.md), [recording and stream details](release-overlay/Eclipse/ECLIPSE-STREAMING.md), [device setup and limits](release-overlay/Eclipse/ECLIPSE-DEVICES.md), and [Flatpak installation notes](packaging/flatpak/README.md).

The settings layout now stacks columns in small windows, wraps forwarding buttons, and collapses device controls by default. A dead build option referencing an absent settings test has been replaced by a working UI integration test mode.

## Syzygy pairing phrase pairing

Start Syzygy on Linux and copy the six-word pairing phrase shown in the terminal (or retrieve it with `syzygy -psk`). Add the host by IP or local discovery in Eclipse, click its card, select **Syzygy pairing phrase**, and paste it. No host-side PIN entry or web UI approval is required. Anyone with the passkey can enroll with full permissions. Eclipse saves the paired certificate, rather than the passkey, for subsequent connections. The passkey stays the same across host restarts and can enroll multiple clients.

## Eclipse 0.6.0-dev

The macOS Apple Silicon development build is versioned independently as **0.6.0-dev**. It uses the Eclipse crescent icon artwork in `release-overlay/Eclipse/app/gui/icons/eclipse.svg`; the macOS build generates the native `Eclipse.icns` bundle icon from that source. The build targets macOS 15+ and Apple Silicon. It remains a preview until CI packaging and physical-device validation complete.

## Releases

Download current development builds from the [Eclipse development release](https://github.com/NotADeveloper4665/Eclipse/releases/tag/dev): Fedora 44 x86_64 RPM, x86_64 Flatpak, and complete source. The release updates only after RPM, Flatpak, and source checks pass for the same main-branch commit. `SHA256SUMS` and `dev-build.json` record file integrity and build provenance.

The versioned [Eclipse 0.5.6 preview](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.5.6) remains available separately.

## Source and checks

Extract `Eclipse-source.zip` and copy `release-overlay/Eclipse/.` over the extracted `Eclipse/` tree. Open `moonlight-qt.pro` in Qt Creator or use qmake. The release workflow assembles the same source and builds an RPM on pushes to `main`. `packaging/` contains the Fedora spec; `.github/workflows/` contains source checks and release packaging.

Focused control-center, forwarding, QML runtime, and Vulkan shader tests are included in the assembled `tests/` directory. See [validation notes](release-overlay/Eclipse/ECLIPSE-VALIDATION.md).

With Syzygy's opt-in `-tails` flag or Network auto-authorization setting, pairing
over the Tailscale address automatically enrolls a verified same-tailnet device
with full permissions. No phrase or host-side PIN entry is required. Shared
external nodes are excluded. Other connections use the six-word pairing phrase;
old hexadecimal enrollment keys are retired by the updated host. Existing paired
certificates continue to work. Spaces and hyphens are interchangeable in phrases.

Automatic Tailscale enrollment supports Linux and macOS Eclipse clients. Linux
uses the root-owned `/run/tailscale/tailscaled.sock` and trusted system CLI.
macOS uses the CLI inside the verified, code-signed
`/Applications/Tailscale.app` and asks its local daemon to verify the peer with
`status --json` and `whois --json`. The client checks the exact literal Tailscale
IP, tailnet membership, share status, machine authorization, and stable node
identity before attempting automatic enrollment. If the official app or its
signature check is unavailable, automatic enrollment is refused; phrase pairing
remains available.

For automatic enrollment, enter the host's literal Tailscale IP address.
Hostnames use phrase pairing to avoid trusting a destination that can change
between the identity check and the connection.
