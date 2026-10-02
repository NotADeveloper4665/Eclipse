# Eclipse

Eclipse is a Moonlight desktop client variant with a dedicated control center for host connection, display, streaming, audio, and input settings. Its UI follows Moonlight's existing dark Material theme.

## Latest release

**Eclipse 0.03.1** replaces the in-session resolution, frame-rate, codec, and audio-channel steppers with dropdown menus.

[Download Eclipse 0.03.1 for Fedora 44 x86_64](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.03.1). The release includes the RPM and the full source archive.

Eclipse 0.03 added per-PC streaming profiles, stream diagnostics, and clearer Tailscale status handling. The [Eclipse 0.03 release](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.03) and [Eclipse 0.02 release](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.02.1) remain available.

## Source and builds

The full Eclipse 0.03.1 source archive is attached to its release. Extract it and open `Eclipse/moonlight-qt.pro` in Qt Creator, or follow `Eclipse/ECLIPSE.md` for build notes.

The source is based on Moonlight Qt and retains its license and upstream notices. See `Eclipse/LICENSE` and `Eclipse/README.md`.

The RPM build specification and release workflow are in `packaging/` and `.github/workflows/`.

## Validation

The in-session quick menu dropdown interactions are covered by the focused control-center test suite. See `Eclipse/ECLIPSE-VALIDATION.md` for platform limitations and other verification details.
