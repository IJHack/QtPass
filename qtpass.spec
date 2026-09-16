# RPM spec for QtPass. Build a source tarball whose top directory is
# qtpass-%{version}, then:
#
#   rpmbuild -tb qtpass-2.0.0.tar.gz
#
# The version below must match qtpass.pri; see the qtpass-releasing skill.

Name: qtpass
Version: 2.0.0
Release: 1%{?dist}
Summary: Multi-platform GUI for pass, the standard Unix password manager
License: GPL-3.0-or-later
URL:	https://qtpass.org/
Source0: %{name}-%{version}.tar.gz

BuildRequires: make
BuildRequires: gcc-c++
# Fedora names; openSUSE calls these qt6-base-devel, qt6-svg-devel, qt6-tools-devel
BuildRequires: qt6-qtbase-devel
BuildRequires: qt6-qtsvg-devel
BuildRequires: qt6-qttools-devel
BuildRequires: desktop-file-utils
BuildRequires: libappstream-glib
Requires: qt6-qtsvg
Recommends: pass
Recommends: gnupg2
Recommends: git

%description
QtPass is a GUI for pass, the standard Unix password manager.
Features:
 - Using pass or git and gpg2 directly
 - Built-in time-based one-time passwords (TOTP)
 - Configurable shoulder surfing protection options
 - Cross platform: Linux, BSD, macOS and Windows
 - Per-folder user selection for multi recipient encryption
 - Multiple profiles

%prep
%setup -q

%build
# Fedora ships qmake-qt6, openSUSE qmake6; the %%qmake_qt6 / %%qmake6 macros
# add the distribution's build flags when they exist.
%{?qmake_qt6:%qmake_qt6 PREFIX=%{_prefix} CONFIG+=release}
%{?!qmake_qt6:%{?qmake6:%qmake6 PREFIX=%{_prefix} CONFIG+=release}}
%{?!qmake_qt6:%{?!qmake6:qmake-qt6 PREFIX=%{_prefix} CONFIG+=release}}
%make_build

%install
%make_install INSTALL_ROOT=%{buildroot}

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/qtpass.desktop
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/qtpass.appdata.xml

%files
%license LICENSE
%doc README.md CHANGELOG.md FAQ.md
%{_bindir}/qtpass
%{_datadir}/applications/qtpass.desktop
%{_datadir}/icons/hicolor/512x512/apps/qtpass-icon.png
%{_datadir}/icons/hicolor/scalable/apps/qtpass-icon.svg
%{_datadir}/metainfo/qtpass.appdata.xml
%{_mandir}/man1/qtpass.1*

%changelog
* Wed Sep 16 2026 Anne Jan Brouwer <qtpass@annejan.com> 2.0.0
- Qt 6 only; the spec builds with qmake-qt6 and installs the desktop file,
  metainfo, icons and man page that main/main.pro now provides

* Sun Sep 13 2026 Anne Jan Brouwer <qtpass@annejan.com> 1.8.0
- Updated QtPass

* Fri Mar 20 2026 Anne Jan Brouwer <qtpass@annejan.com> 1.5.0
- Updated QtPass

* Tue Jan 26 2016 Anne Jan Brouwer <qtpass@annejan.com> 1.1.0
- Updated spec to latest version

* Wed Dec 30 2015 Andrew DeMaria <lostonamountain@gmail.com> 1.0.5.1-2
- Added desktop/icon resources
- Added required build deps for a clean build

* Tue Dec 01 2015 Bram Vandoren <bram.vandoren@ster.kuleuven.be> - 1.0.5-1
- Initial RPM spec
