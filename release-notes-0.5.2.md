# Eclipse 0.5.2

Eclipse 0.5.2 adds finer control over guest input, adaptive bitrate adjustment, and direct desktop launch.

- **Guest input permissions:** Allow or block keyboard, mouse and touch, and controller input independently. View-only mode disables all three categories for the next stream.
- **Dynamic adaptive bitrate:** Optionally reduce the stream bitrate when measured network round-trip time or jitter rises, then restore it gradually after the connection stabilizes. Quality, balanced, and low-latency modes tune the response. This requires a Sunshine host that supports runtime bitrate changes; other hosts keep their configured bitrate.
- **Launch straight to Desktop:** Skip the app selection screen and launch the host app named “Desktop” when available.

Guest permissions are enforced by the Eclipse client and do not change host-side authorization. This release includes the 0.5.1 recording and Flatpak features.
