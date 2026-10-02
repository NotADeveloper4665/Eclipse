# Eclipse

Eclipse is a Moonlight desktop client variant with a dedicated control center for host connection, display, streaming, audio, and input settings. Its UI follows Moonlight's existing dark Material theme.

## Latest release

**Eclipse 0.03** improves streaming setup with saved per-PC profiles, clearer Tailscale connection status, stream diagnostics, and labeled resolution and frame-rate dropdowns. It also includes the Tailscale device picker and Intel VA-API preference introduced in 0.02.

[Download Eclipse 0.03 for Fedora 44 x86_64](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.03). The release contains the RPM and a versioned full source archive.

The previous [Eclipse 0.02 release](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.02.1) and [Eclipse 0.01](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.01) remain available.

## Source and builds

The full Eclipse 0.03 source archive is attached to its release. Extract it and open `Eclipse/moonlight-qt.pro` in Qt Creator, or follow `Eclipse/ECLIPSE.md` for build notes.

The source is based on Moonlight Qt and retains its license and upstream notices. See `Eclipse/LICENSE` and `Eclipse/README.md`.

The RPM build specification and release workflow are in `packaging/` and `.github/workflows/`.

## Validation

The Eclipse control center was built and tested on Linux x86_64 with Qt 5.15.13. See `Eclipse/ECLIPSE-VALIDATION.md` for the tested controls and platform limitations.
