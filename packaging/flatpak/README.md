# Flatpak

Eclipse's Flatpak is built from `Eclipse-source.zip` plus the contents of `release-overlay/Eclipse`, using the KDE Qt 5.15 runtime. The GitHub release workflow emits an x86-64 `.flatpak` bundle and attaches it to tagged releases.

Install Flatpak for your Linux distribution and configure Flathub, then install the downloaded bundle:

```sh
flatpak install --user ./Eclipse-0.5.1-x86_64.flatpak
flatpak run io.github.notadeveloper4665.Eclipse
```

The app requests network, Wayland/X11, PulseAudio, GPU render-node, and Videos-folder access for streaming, playback, hardware rendering, and recordings. Video acceleration still depends on the host's Flatpak graphics-driver extensions.

Flatpak's sandbox does not expose the host's Tailscale CLI/service, USB/IP tools, or privileged `pkexec` path to Eclipse. You can still connect to a Tailscale host by entering its reachable address manually, but Tailscale discovery and the host-dependent microphone, webcam, and USB forwarding actions are not available from this Flatpak build.
