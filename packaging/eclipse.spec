Name:           moonlight-eclipse
Version:        0.03.4
Release:        1%{?dist}
Summary:        Moonlight streaming client with the Eclipse control center
License:        GPL-3.0-only
URL:            https://github.com/NotADeveloper4665/Eclipse
Source0:        %{name}-%{version}.tar.gz
Source1:        moonlight-eclipse.desktop

BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  qt5-qtbase-devel
BuildRequires:  qt5-qtdeclarative-devel
BuildRequires:  qt5-qtsvg-devel
BuildRequires:  qt5-qtquickcontrols2-devel
BuildRequires:  openssl-devel
BuildRequires:  SDL2-devel
BuildRequires:  SDL2_ttf-devel
BuildRequires:  ffmpeg-devel
BuildRequires:  libva-devel
BuildRequires:  libvdpau-devel
BuildRequires:  opus-devel
BuildRequires:  pulseaudio-libs-devel
BuildRequires:  alsa-lib-devel
BuildRequires:  libdrm-devel

Requires:       qt5-qtdeclarative
Requires:       qt5-qtquickcontrols2
Requires:       qt5-qtsvg
Recommends:     libva-intel-media-driver

%description
Eclipse is a Moonlight desktop streaming client with an in-session control
center for host connection, display, streaming, audio, and input settings.

%prep
%autosetup -n %{name}-%{version}

%build
qmake-qt5 moonlight-qt.pro CONFIG+=release CONFIG-=debug_and_release
%make_build

%install
install -Dpm0755 app/moonlight %{buildroot}%{_bindir}/moonlight-eclipse
install -Dpm0644 %{SOURCE1} %{buildroot}%{_datadir}/applications/moonlight-eclipse.desktop
install -Dpm0644 app/res/moonlight.svg \
    %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/moonlight-eclipse.svg

%files
%license LICENSE
%doc README.md ECLIPSE.md ECLIPSE-VALIDATION.md
%{_bindir}/moonlight-eclipse
%{_datadir}/applications/moonlight-eclipse.desktop
%{_datadir}/icons/hicolor/scalable/apps/moonlight-eclipse.svg

%changelog
* Sat Oct 03 2026 Eclipse contributors <eclipse@example.invalid> - 0.03.4-1
- Fix main application settings button navigation
- Disable the Qt QML disk cache on Linux to avoid the startup crash

* Fri Oct 02 2026 Eclipse contributors <eclipse@example.invalid> - 0.03.1-1
- Replace in-session resolution and other discrete settings steppers with dropdown menus

* Fri Oct 02 2026 Eclipse contributors <eclipse@example.invalid> - 0.03-1
- Add per-PC stream profiles, stream diagnostics, and clearer settings selectors
- Improve Tailscale status reporting and clean up the resolution and frame-rate controls

* Fri Oct 02 2026 Eclipse contributors <eclipse@example.invalid> - 0.02-1
- Add Tailscale device discovery and Intel VA-API render-node preference

* Fri Oct 02 2026 Eclipse contributors <eclipse@example.invalid> - 0.01-1
- Initial Eclipse release for Fedora Linux
