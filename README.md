# Eclipse

Eclipse is a Moonlight desktop streaming client with an in-session control center, Tailscale discovery, per-PC stream profiles, and decoder diagnostics. It retains Moonlight's license and upstream notices.

## Eclipse 0.5.4

Eclipse 0.5.4 adds key-based pairing for Syzygy hosts. Enter a Syzygy host key from the unpaired host's context menu; Eclipse uses it to prove access without sending or storing the key itself. A successful key pairing grants that client full host permissions, so protect the key. This release carries forward 0.5.3's Vulkan Mailbox presentation, FSR-aware recording control, and settings updates. See [release notes](release-notes-0.5.4.md), [recording and stream details](release-overlay/Eclipse/ECLIPSE-STREAMING.md), [device setup and limits](release-overlay/Eclipse/ECLIPSE-DEVICES.md), and [Flatpak installation notes](packaging/flatpak/README.md).

The settings layout now stacks columns in small windows, wraps forwarding buttons, and collapses device controls by default. A dead build option referencing an absent settings test has been replaced by a working UI integration test mode.

## Syzygy pairing phrase pairing

Start Syzygy on Linux and copy the six-word pairing phrase shown in the terminal (or retrieve it with `syzygy -psk`). Add the host by IP or local discovery in Eclipse, click its card, select **Syzygy pairing phrase**, and paste it. No host-side PIN entry or web UI approval is required. Anyone with the passkey can enroll with full permissions. Eclipse saves the paired certificate, rather than the passkey, for subsequent connections. The passkey stays the same across host restarts and can enroll multiple clients.

## Releases

The 0.5.4 source and package builds are running from GitHub Actions. The latest published package remains [Eclipse 0.5.3](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.5.3).

## Source and checks

Extract `Eclipse-source.zip` and copy `release-overlay/Eclipse/.` over the extracted `Eclipse/` tree. Open `moonlight-qt.pro` in Qt Creator or use qmake. The release workflow assembles the same source and builds an RPM on pushes to `main`. `packaging/` contains the Fedora spec; `.github/workflows/` contains source checks and release packaging.

Focused control-center, forwarding, QML runtime, and Vulkan shader tests are included in the assembled `tests/` directory. See [validation notes](release-overlay/Eclipse/ECLIPSE-VALIDATION.md).

With Syzygy's opt-in `-tails` flag or Network auto-authorization setting, pairing
over the Tailscale address automatically enrolls a verified same-tailnet device
with full permissions. No phrase or host-side PIN entry is required. Shared
external nodes are excluded. Other connections use the six-word pairing phrase;
old hexadecimal enrollment keys are retired by the updated host. Existing paired
certificates continue to work. Spaces and hyphens are interchangeable in phrases.

Automatic Tailscale enrollment currently requires a Linux Eclipse client with
`/usr/bin/tailscale` and a root-owned `/run/tailscale/tailscaled.sock`. The client
checks the host against its local daemon before trusting auto-enrollment. Other
platforms can pair using the generated phrase.

For automatic enrollment, enter the host's literal Tailscale IP address.
Hostnames use phrase pairing to avoid trusting a destination that can change
between the identity check and the connection.
