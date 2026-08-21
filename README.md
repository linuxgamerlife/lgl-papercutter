<div align="center">

# LGL Papercutter

**A simple way to modify wallpapers to make them a better fit for your desktop.**

[![CI](https://github.com/linuxgamerlife/lgl-papercutter/actions/workflows/ci.yml/badge.svg)](https://github.com/linuxgamerlife/lgl-papercutter/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
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
- Back up originals automatically before accepting an edit
- Restore an earlier original from backup history inside the app

---

## How It Works

1. Add one or more JPEG, PNG, or WebP images using the toolbar or drag and drop.
2. Select your target resolution.
3. Drag the image to position it and adjust the zoom until the preview looks
   right.
4. Choose **Accept & Save** to safely update the original wallpaper while
   keeping its existing filename and location.

Before replacing an original, Papercutter creates and verifies a versioned
backup. If you change your mind later, open **Backup History** and restore it.
The current file receives its own recovery backup before a restore takes place.

Use **Save As** when you want a separate processed copy instead. Papercutter
remembers the last folder you saved to and asks before replacing an existing
file.

---

## Features

| Feature | What it does |
|---|---|
| **Visual composition** | Preview the exact target shape and reposition the image by dragging |
| **Zoom control** | Choose how tightly the wallpaper is cropped |
| **Resolution targets** | Use detected displays, presets, or custom width and height |
| **Portrait and ultrawide support** | Compose wallpapers for horizontal or vertical displays |
| **Image queue** | Review several wallpapers in one session |
| **Drag and drop** | Drop image files or a folder directly into the window |
| **Duplicate removal** | Find duplicate queued images by their contents |
| **Safe replacement** | Verify a backup before retaining the original name and replacing the file |
| **Save As** | Export a separate copy without changing the source |
| **Backup history** | Browse, export, and safely restore previous originals |
| **Local processing** | Process images with ImageMagick without accounts, uploads, or telemetry |

---

## Install

LGL Papercutter is currently in early development. A Fedora RPM will be made
available through [GitHub Releases](https://github.com/linuxgamerlife/lgl-papercutter/releases)
when the first public build is ready.

For now, build the application from source.

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

**Accept & Save** never intentionally writes over an original until its backup
has been copied and verified with SHA-256. Processed output is validated for the
requested dimensions and committed through a same-folder temporary file.

Backups are never deliberately reused or overwritten. Each version is recorded
with its original path, timestamp, file details, and verification hash.

Even with these protections, keep a separate backup of important or
irreplaceable images when using pre-release software.

---

## License

MIT — see [LICENSE](LICENSE) for details.

---

<div align="center">
Made for <a href="https://fedoraproject.org">Fedora</a> · by <a href="https://www.youtube.com/@linuxgamerlife">LinuxGamerLife</a>
</div>
