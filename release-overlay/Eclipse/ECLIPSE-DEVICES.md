# Client scaling and device forwarding

These features ship in Eclipse 0.4.2. See the project README for release and build links.

## FSR 1 on the client

Settings → Client FSR scaling offers Off, Quality (1.5×), Balanced (1.7×), and Performance (2×). The normal resolution selector becomes the desired output resolution. The host receives a smaller, even-sized stream request; the client keeps its output window at the target size and runs EASU and RCAS using libplacebo/Vulkan. The selected preset is saved globally or with a PC's streaming profile.

This is spatial FSR 1, not temporal FSR 2/3 or frame generation. The bundled shader is agyild's luma-plane port of AMD FSR 1.0.2, with the AMD MIT notice retained. The RCAS pass is guarded when no upscale occurs, so resizing below the source size does not disable the hook. Chroma uses libplacebo's normal reconstruction. The initial implementation supports SDR 4:2:0 streams. HDR and YUV 4:4:4 combinations are rejected with a message. FSR is off by default. Vulkan initialization failure ends the attempt rather than silently substituting ordinary scaling on a reduced stream. A runtime hook error also ends the stream with guidance to disable FSR.

The Fedora build requires libplacebo-devel and vulkan-loader-devel. A working Vulkan driver is required on the client. Hardware decoding remains available through libplacebo's frame mapping; FSR does not provide a new codec or decoder.

Shader source: https://gist.github.com/agyild/82219c545228d70c5604f865ce0b0ce5
Libplacebo hooks: https://libplacebo.org/custom-shaders/

## Host connection

Settings → Microphone, webcam, and USB forwarding → Show forwarding controls. Enter an SSH config alias or `user@host`. These paths currently support Linux clients and Linux hosts, including a Tailscale address. They do not implement a Windows/Shadow receiver or change Sunshine's GameStream protocol.

Use an SSH key and a previously verified host key. First connect using a terminal to establish and verify the host key. The UI uses BatchMode and strict host-key checking; it never asks for or saves an SSH password. Host forwarding is independent of the video session. Click Stop to end it. Forwarding sessions belong to the application, so navigating from Settings back to the PCs and starting a video stream keeps them connected. Exiting Eclipse stops them. It does not automatically capture or share devices when a stream starts.

## Microphone (including built-in microphones)

Install FFmpeg with PulseAudio input/output and libopus on both machines; install `pactl` (Fedora: pulseaudio-utils) for source discovery and receiver setup. On the host, use the same account as the desktop audio session. PipeWire-Pulse or PulseAudio must already be running for that account and accessible from SSH.

Choose Default microphone or a discovered source, then Start forwarding. Eclipse captures mono 48 kHz audio and encodes Opus at 64 kbit/s using 10 ms frames. It pipes this through SSH to a dynamically named host null sink plus remapped source. Select **Eclipse Microphone** in the host application. A stop or normal SSH exit removes only the virtual modules created by that share. Forced termination or an unreachable host may require manually unloading the remaining Eclipse modules with `pactl`.

This forwards audio; it does not take over the physical microphone. Source lists omit audio monitor sources to reduce accidental loopback selection. Default microphone follows the client's configured default, which may still be customized outside Eclipse.

## Webcam (including accessible built-in V4L2 cameras)

The client enumerates readable V4L2 capture devices and excludes metadata-only nodes. FFmpeg captures 640×480 at 30 FPS and forwards MJPEG in Matroska over SSH. Devices that cannot capture that mode report the FFmpeg error. Native libcamera-only devices are not implemented.

The Linux host needs FFmpeg and a writable v4l2loopback device. After installing the appropriate module for its kernel, an administrator can create it with:

```sh
sudo modprobe v4l2loopback video_nr=42 card_label="Eclipse Webcam" exclusive_caps=1
```

Give the SSH user normal access to that virtual device. The field defaults to `/dev/video42`; update it if necessary. The receiver verifies the selected node's name contains `Eclipse` or `loopback`, avoiding writes to an ordinary physical camera. Select **Eclipse Webcam** in the host application. Stop ends the producer; the administrator-created virtual device remains available. Concurrent producers should use different virtual devices.

FFmpeg capture/output reference: https://ffmpeg.org/ffmpeg-devices.html
Virtual-camera setup: https://github.com/v4l2loopback/v4l2loopback
PipeWire virtual source: https://docs.pipewire.org/page_pulse_module_remap_source.html

## USB/IP (physical devices, USB microphones, USB webcams)

This exports a device plugged into the Eclipse client to the host. The host imports it. The three selectors list USB audio devices, USB cameras, and all non-hub USB devices. USB audio may include output-only devices; interface class discovery cannot prove that a device contains a microphone. Composite devices appear in more than one list but can be exported only once. Each forwarding control can share one device, and distinct controls use different remote tunnel ports.

Client prerequisites: `usbip`, `usbipd`, PolicyKit (`pkexec`), and OpenSSH client. An administrator loads the export driver and runs the daemon before sharing:

```sh
sudo modprobe usbip-host
sudo usbipd -D -4
```

Keep the daemon's TCP 3240 port restricted to loopback in the client's firewall. Eclipse connects to it through an encrypted SSH reverse tunnel; there is no need to expose it publicly or to the tailnet. Eclipse does not change firewall rules or start a system daemon.

Host prerequisites: Linux USB/IP tools and the import driver:

```sh
sudo modprobe vhci-hcd
```

The SSH account must have administrator-configured permission to run the required `usbip attach` and `usbip detach` commands using noninteractive sudo. The account must also be allowed SSH reverse forwarding. Eclipse uses a randomly selected remote loopback port between 20000 and 59999 and `ExitOnForwardFailure`; a conflicting port causes a reported failure and restores the local device. The daemon on the client uses TCP 3240.

Share triggers a local PolicyKit prompt for `usbip bind`. Stop sharing triggers another prompt for `usbip unbind`. The physical device is unavailable locally while shared. The receiver tracks and detaches only the port created for this share. Bind, attach, SSH, timeout, and restore failures are displayed. A denied unbind leaves Stop sharing available for retry. Unplugging a shared device ends its forwarding.

After a forced app kill, recover a still-exported local device with:

```sh
sudo usbip unbind --busid=1-2
```

Replace the bus ID with the displayed selection. Inspect `usbip port` on the host and detach any orphaned import port. Actual USB audio/video transfer depends on kernel, device, and network behavior and needs hardware testing; no latency or universal isochronous-device compatibility is claimed.

Kernel USB/IP usage: https://github.com/torvalds/linux/blob/master/tools/usb/usbip/README

## Checks

- All three edited/new settings QML files pass Qt 5 syntax parsing.
- The application compiles and links locally with Vulkan/libplacebo enabled.
- The FSR smoke test renders actual planar YUV data through the bundled EASU/RCAS shader on software Vulkan at Quality/Balanced/Performance ratios, then downscales and upscales again. It checks for disabled hooks, downloads pixels, and verifies upscale output differs from ordinary scaling.
- The forwarding tests exercise discovery, input validation, unplug-before-share, bind/tunnel/unbind lifecycle, restoration after SSH failure, and a real Opus media pipe using synthetic capture and a simulated SSH receiver.
- A build-only UI smoke mode loads the production main window, invokes its Settings route, verifies the settings page and forwarding controllers, and activates the FSR selector. This mode is excluded from shipping binaries.
- Real microphone playback, physical webcam capture, USB hardware transfer, host driver installation, and client/host streaming latency are not validated in this workspace.
