# This is an initial attempt to create a RPM spec file for qtpass. Tested on Fedora 23.
# Replace the Version number with the current version number (at the time of writing 1.0.5.1) 
# Check qtpass.pro for the current version. 
# After that
#  tar cvfz qtpass-1.0.5.1.tar.gz qtpass-1.0.5.1
#  rpmbuild -tb qtpass-1.0.5.1.tar.gz
# This should probably be part of a release process.

Name: qtpass		
Version: 1.0.5.1
Release: 1%{?dist}
Summary: QtPass is a multi-platform GUI for pass, the standard unix password manager.	
License: GPLv3
URL:	https://qtpass.org/	
Source0: %{name}-%{version}.tar.gz

BuildRequires:	qt5-qtbase-devel
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

%files
%doc
%{_bindir}/*

%changelog
* Tue Dec 01 2015 serstring=Bram Vandoren <bram.vandoren@ster.kuleuven.be> - 1.0.5-1
- Initial RPM spec

