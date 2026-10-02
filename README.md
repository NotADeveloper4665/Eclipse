# Eclipse

Eclipse is a Moonlight desktop client variant with a dedicated control center for host connection, display, streaming, audio, and input settings. Its UI follows Moonlight's existing dark Material theme.

## Latest release

**Eclipse 0.02** adds a Tailscale device picker to the PC list and prefers Intel integrated graphics for VA-API decoding on hybrid Linux systems.

[Download Eclipse 0.02 for Fedora 44 x86_64](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.02). The release contains the RPM and a versioned full source archive. The Fedora RPM recommends Fedora's Intel VA-API driver package for Intel hardware.

The first release, [Eclipse 0.01](https://github.com/NotADeveloper4665/Eclipse/releases/tag/eclipse-0.01), remains available.

## Source and builds

The current source snapshot is in [Eclipse-source.zip](Eclipse-source.zip), with the Eclipse 0.02 source archive attached to its release. Extract the archive and open `Eclipse/moonlight-qt.pro` in Qt Creator, or follow `Eclipse/ECLIPSE.md` for build notes.

The snapshot is based on Moonlight Qt and retains its license and upstream notices. See `Eclipse/LICENSE` and `Eclipse/README.md`.

The RPM build specification and release workflow are in `packaging/` and `.github/workflows/`.

## Validation

The Eclipse control center was built and tested on Linux x86_64 with Qt 5.15.13. See `Eclipse/ECLIPSE-VALIDATION.md` for the tested controls and platform limitations.
