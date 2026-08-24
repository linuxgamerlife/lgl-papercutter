Name:           lgl-papercutter
Version:        0.2.0
Release:        1%{?dist}
Summary:        Compose wallpapers for a chosen desktop resolution

License:        MIT
URL:            https://github.com/linuxgamerlife/lgl-papercutter
# Generated from the checked-out commit by `make source` or `make srpm`.
# COPR SCM builds do not require a Git tag.
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  cmake-rpm-macros
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel >= 6.5
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib
BuildRequires:  ImageMagick
Requires:       qt6-qtbase >= 6.5
Requires:       ImageMagick

%description
LGL Papercutter is a local desktop application for composing and processing
wallpapers to match a selected display resolution.

Images are processed locally with ImageMagick and exported as new files. Source
images are not replaced by the export workflow.

%prep
%autosetup -n %{name}-%{version}

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
%doc README.md CHANGELOG.md
%{_bindir}/lgl-papercutter
%{_datadir}/applications/com.linuxgamerlife.lgl-papercutter.desktop
%{_datadir}/metainfo/com.linuxgamerlife.lgl-papercutter.metainfo.xml
%{_datadir}/icons/hicolor/512x512/apps/lgl-papercutter.png

%changelog
* Mon Aug 24 2026 LinuxGamerLife - 0.2.0-1
- Add non-destructive batch Save As with numbered export planning
- Preserve independent staged compositions and support multi-image resolution changes
- Improve native desktop integration and export confirmation previews

* Thu Aug 20 2026 LinuxGamerLife - 0.1.0-1
- Initial Fedora 44 development scaffold
