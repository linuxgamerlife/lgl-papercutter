Name:           lgl-papercutter
Version:        0.4.1
Release:        1%{?dist}
Summary:        Compose wallpapers for a chosen desktop resolution

License:        MIT
URL:            https://github.com/linuxgamerlife/lgl-papercutter
# Generated from the checked-out commit by `make source` or `make srpm`.
# COPR SCM builds do not require a Git tag.
Source0:        %{name}-%{version}.tar.gz

# The root CMakeLists.txt builds the maintained GTK 4 application from src/.

BuildRequires:  cmake >= 3.20
BuildRequires:  cmake-rpm-macros
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(gtkmm-4.0) >= 4.20
BuildRequires:  pkgconfig(libadwaita-1) >= 1.6
BuildRequires:  pkgconfig(json-glib-1.0)
BuildRequires:  glib2-devel
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib
BuildRequires:  ImageMagick
BuildRequires:  exiv2
Requires:       ImageMagick
Requires:       exiv2

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
%doc README.md CHANGELOG.md HELP.md
%{_bindir}/lgl-papercutter
%{_datadir}/applications/com.linuxgamerlife.lgl-papercutter.desktop
%{_datadir}/metainfo/com.linuxgamerlife.lgl-papercutter.metainfo.xml
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/icons/hicolor/64x64/apps/%{name}.png
%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
%{_datadir}/icons/hicolor/256x256/apps/%{name}.png

%changelog
* Tue Oct 13 2026 LinuxGamerLife - 0.4.1-1
- Adopt the LGL GTK app standard
- Add a system title bar mode (Ctrl+Shift+M or Use System Title Bar in the
  main menu) that lets the compositor draw the title bar
- Open and close the main menu by tapping Alt
- Install the icon at 48, 64, 128 and 256 px
- Build the GTK app from the repository root

* Tue Oct 13 2026 LinuxGamerLife - 0.4.0-3
- Draw square window corners so compositors that round and border windows
  leave no gap

* Tue Oct 13 2026 LinuxGamerLife - 0.4.0-2
- Build with libadwaita so gtk.css palettes such as Noctalia theme every control,
  whichever GTK theme is installed
- Follow the desktop dark mode and accent colour, with a fallback for desktops
  without a settings portal

* Mon Oct 12 2026 LinuxGamerLife - 0.4.0-1
- Migrate to GTK 4 (gtkmm) for better theming and look on GTK desktops
- Use libadwaita and follow the desktop theme live: palette, light/dark, accent, fonts
- Redesign the window with a header bar, queue thumbnails, and settings cards
- Default to the display the window opens on and embed the official icon

* Mon Oct 12 2026 LinuxGamerLife - 0.3.1-1
- Add per-image JPEG and WebP output quality with estimated output file size
- Add editable Description, Artist, Copyright, and Comment metadata
- Add a zoom Reset button, a 5120x1440 preset, and gap-aware export numbering
- Default to the system primary monitor and keep indexed PNGs palette-compressed
- Support Qt 6.4 and ImageMagick 6 source builds

* Mon Aug 24 2026 LinuxGamerLife - 0.3.0-1
- Add in-app Help covering controls, staged editing, exports, and file safety
- Add an About dialog with project, author, license, and donation links
- Make Help and About available from both the menu bar and More menu

* Mon Aug 24 2026 LinuxGamerLife - 0.2.0-1
- Add non-destructive batch Save As with numbered export planning
- Preserve independent staged compositions and support multi-image resolution changes
- Improve native desktop integration and export confirmation previews

* Thu Aug 20 2026 LinuxGamerLife - 0.1.0-1
- Initial Fedora 44 development scaffold
