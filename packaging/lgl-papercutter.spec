Name:           lgl-papercutter
Version:        0.1.0
Release:        1%{?dist}
Summary:        Compose wallpapers for a chosen desktop resolution

License:        MIT
URL:            https://github.com/linuxgamerlife/lgl-papercutter
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.5
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib
Requires:       qt6-qtbase >= 6.5
Requires:       ImageMagick

%description
LGL Papercutter is a local desktop application for composing and processing
wallpapers to match a selected display resolution.

Images are processed locally with ImageMagick. Originals are copied to the
configured backup folder before a validated output atomically replaces them.

%prep
%autosetup

%build
%cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
%cmake_build

%install
%cmake_install

desktop-file-validate \
    %{buildroot}%{_datadir}/applications/com.linuxgamerlife.lgl-papercutter.desktop
appstream-util validate-relax --nonet \
    %{buildroot}%{_datadir}/metainfo/com.linuxgamerlife.lgl-papercutter.metainfo.xml

%check
%ctest

%files
%license LICENSE
%{_bindir}/lgl-papercutter
%{_datadir}/applications/com.linuxgamerlife.lgl-papercutter.desktop
%{_datadir}/metainfo/com.linuxgamerlife.lgl-papercutter.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/com.linuxgamerlife.lgl-papercutter.svg

%changelog
* Thu Aug 20 2026 LinuxGamerLife - 0.1.0-1
- Initial Fedora 44 development scaffold
