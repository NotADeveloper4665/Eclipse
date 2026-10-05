# Eclipse 0.5.3

This release adds a Vulkan presentation option for smoother, lower-latency display on supported client surfaces, and clarifies the settings page.

- **Mailbox presentation (Fast-Sync):** Enable it alongside V-Sync in Basic Settings. When the Vulkan renderer's display surface advertises Mailbox mode, Eclipse displays the newest completed frame at each refresh. Unsupported surfaces fall back to FIFO V-Sync. Gamescope may expose Mailbox mode. The option applies to the next stream and is off by default.
- **Recording with FSR:** The Quick Menu blocks starting a recording when client FSR upscaling is active. Turn FSR off and start a new stream to record incoming encoded video and audio.
- **Settings organization:** FSR and device forwarding remain under Basic Settings, adaptive bitrate is under Streaming, and direct desktop launch is under Host. Explanations for latency, forwarding, recording, and custom resolutions have been updated.

RPM and Flatpak packages are built by GitHub Actions. The Flatpak sandbox cannot access the host Tailscale CLI, USB/IP tools, or privileged forwarding helpers; see [Flatpak installation notes](packaging/flatpak/README.md).
