<div align="center">

# LGL Papercutter

**A simple way to modify wallpapers to make them a better fit for your desktop.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/badge/release-0.3.0-purple)](CHANGELOG.md)
[![Fedora](https://img.shields.io/badge/Fedora-44-blue?logo=fedora&logoColor=white)](https://fedoraproject.org)
[![Qt](https://img.shields.io/badge/Qt-6-green?logo=qt&logoColor=white)](https://www.qt.io)

</div>

---

## Overview

LGL Papercutter is a graphical wallpaper editor for Linux. Add your existing
images, choose your monitor resolution, and visually position each wallpaper
before saving it at exactly the right size.

- Drag, zoom, and preview the final crop before changing any file
- Use detected display sizes, common presets, or a custom resolution
- Process portrait, landscape, and ultrawide wallpapers without distortion
- Keep everything private and local — images are never uploaded
- Export processed copies without changing the source images

---

## How It Works

1. Add one or more JPEG, PNG, or WebP images using the toolbar or drag and drop.
2. Select your target resolution. Select multiple queue entries first to apply
   the resolution to all of them at once.
3. Drag the current image to position it and adjust the zoom until the preview
   looks right. Each image retains its own staged resolution, position, and
   zoom while you move through the queue.
4. Choose **Save As** to preview the exact output names and locations, then
   export the processed wallpapers as new files.

Papercutter never replaces a source during export. It remembers the last folder
you saved to and asks before replacing an existing destination. Queue removal
does not touch source files; the separate Move to Trash action always asks for
confirmation.

---

## Features

| Feature | What it does |
|---|---|
| **Visual composition** | Preview the exact target shape and reposition the image by dragging |
| **Zoom control** | Choose how tightly the wallpaper is cropped |
| **Resolution targets** | Use detected displays, presets, or custom width and height |
| **Staged compositions** | Retain an independent resolution, crop, position, and zoom for every queued image |
| **Multi-image resolution changes** | Apply a target resolution to any selection or the entire queue |
| **Portrait and ultrawide support** | Compose wallpapers for horizontal or vertical displays |
| **Image queue** | Review several wallpapers in one session |
| **Batch export** | Preview and save several composed images into one chosen folder |
| **Numbered outputs** | Optionally continue from the highest numbered filename already in the destination |
| **Drag and drop** | Drop image files or a folder directly into the window |
| **File actions** | Right-click selected queue items to save, export, remove, or move sources to Trash |
| **Duplicate removal** | Find duplicate queued images by their contents |
| **Save As** | Export a separate copy without changing the source |
| **Local processing** | Process images with ImageMagick without accounts, uploads, or telemetry |
| **Built-in help** | Explain every control, staged-state behaviour, export logic, and file-safety rule inside the app |
| **About** | Show the version, project and author links, licence, and Ko-fi support link |

---

## Install

The current release is **0.3.0**.

### Recommended — Fedora COPR

Enable the [LGL Papercutter COPR](https://copr.fedorainfracloud.org/coprs/linuxgamerlife/lgl-papercutter/)
and install the application:

```bash
sudo dnf copr enable linuxgamerlife/lgl-papercutter
sudo dnf install lgl-papercutter
```

Future releases will then be installed through normal Fedora system updates.

### Manual downloads

RPM and AppImage release artifacts are also available from
[GitHub Releases](https://github.com/linuxgamerlife/lgl-papercutter/releases).

The project can also be built directly from source.

---

## What's New in 0.3.0

- Comprehensive built-in Help explaining the controls and complete workflow
- Clear guidance for staged edits, multi-selection, numbered exports, and file safety
- About dialog showing the app version, project page, author, MIT licence, and
  Ko-fi support link
- Help and About entries in both the full menu bar and the always-visible More menu

See [CHANGELOG.md](CHANGELOG.md) for the complete release history.

---

## Build from Source

Install the dependencies on Fedora 44:

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel ImageMagick
```

Clone, build, and test:

```bash
git clone https://github.com/linuxgamerlife/lgl-papercutter.git
cd lgl-papercutter
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Run it directly from the build directory:

```bash
./build/lgl-papercutter
```

---

## File Safety

Save As renders into a temporary file, validates the requested dimensions, and
then commits the new destination. It refuses to use the source path as the
destination, so exporting cannot replace the original image.

Removing an entry from the queue does not change the source file. **Move to
Trash** is the only source-file mutation and always requires confirmation.

---

## License

MIT — see [LICENSE](LICENSE) for details.

---

<div align="center">
Made for <a href="https://fedoraproject.org">Fedora</a> · by <a href="https://www.youtube.com/@linuxgamerlife">LinuxGamerLife</a>
</div>
