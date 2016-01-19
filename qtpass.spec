# This is an initial attempt to create a RPM spec file for qtpass. Tested on Fedora 23.
# Replace the Version number with the current version number (at the time of writing 1.0.5.1) 
# Check qtpass.pro for the current version. 
# After that
#  tar cvfz qtpass-1.0.5.1.tar.gz qtpass-1.0.5.1
#  rpmbuild -tb qtpass-1.0.5.1.tar.gz
# This should probably be part of a release process.

Name: qtpass		
Version: 1.0.5.1
Release: 2%{?dist}
Summary: QtPass is a multi-platform GUI for pass, the standard unix password manager.	
License: GPLv3
URL:	https://qtpass.org/	
Source0: %{name}-%{version}.tar.gz

BuildRequires: qt5-qtbase-devel
BuildRequires: qt5-linguist
BuildRequires: desktop-file-utils
BuildRequires: xdg-utils
Requires: pass	
Requires: qt5-qtbase	

%description
QtPass is a GUI for pass, the standard unix password manager.
Features:
 - Using pass or git and gpg2 directly
 - Configurable shoulder surfing protection options
 - Cross platform: Linux, BSD, OS X and Windows
 - Per-folder user selection for multi recipient encryption
 - Multiple profiles

%prep
%setup -q

%build
qmake-qt5 PREFIX=%{buildroot}/%{_bindir}
make %{?_smp_mflags}

%install
%make_install
install -Dm 0644 artwork/icon.png %{buildroot}%{_datadir}/icons/hicolor/64x64/apps/qtpass-icon.png
desktop-file-install --dir=%{buildroot}%{_datadir}/applications qtpass.desktop

%files
%doc
%{_bindir}/*
%{_datadir}/applications/qtpass.desktop
%{_datadir}/icons/hicolor/64x64/apps/qtpass-icon.png

%post
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%postun
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi

%posttrans
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :

%changelog
* Wed Dec 30 2015 Andrew DeMaria <lostonamountain@gmail.com> 1.0.5.1-2
- Added desktop/icon resources
- Added required build deps for a clean build

* Tue Dec 01 2015 serstring=Bram Vandoren <bram.vandoren@ster.kuleuven.be> - 1.0.5-1
- Initial RPM spec

