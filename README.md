# LGL Papercutter

[![CI](https://github.com/linuxgamerlife/lgl-papercutter/actions/workflows/ci.yml/badge.svg)](https://github.com/linuxgamerlife/lgl-papercutter/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

> A simple way to modify wallpapers to better fit your desktop.

LGL Papercutter is a Linux desktop application in early development for turning existing
images into high-quality wallpapers matched to a user's display resolution. It
will provide a visual composition window for positioning, zooming, previewing,
and processing wallpapers without uploading images to an external service.

## Project status

LGL Papercutter is currently in **early development**. The repository contains
the editor, composition model, persistent settings, ImageMagick processing
pipeline, automated tests, and Fedora packaging metadata. Processing creates
and verifies a versioned backup before rendering and atomically replacing the
source image. It also supports a non-destructive Save As workflow and in-app
restoration from verified backup history.

## Planned workflow

1. Select an image, multiple images, or the top-level images in a folder.
2. Choose a detected monitor resolution, a saved preset, or custom dimensions.
3. Compose each image inside a fixed-resolution preview window using pan and
   zoom. The image always covers the complete target without distortion.
4. Review the result and any quality warning caused by upscaling.
5. Choose **Accept & Save** to verify a backup and replace the original while
   retaining its filename and location, or **Save As** to create a separate
   processed copy in the last-used destination folder.
6. Browse backup history and restore an earlier original; Papercutter first
   backs up the current file so a restore can itself be reversed.

The first release is planned as a local C++/Qt 6 application for Fedora 44,
distributed as an RPM. It will preserve JPEG, PNG, and WebP source formats and
use ImageMagick for final image processing.

## Project documentation

- [Objectives](docs/OBJECTIVES.md) defines the product goals and v1 boundaries.
- [Tools](docs/TOOLS.md) records the planned technology and packaging stack.
- [Principles](docs/PRINCIPLES.md) defines the safety, UX, quality, and
  engineering standards for implementation.

## License

The project is available under the [MIT License](LICENSE).

## Development scaffold

Development runs inside the Fedora 44 Distrobox named `fedora-dev1`. Enter it
and install the dependencies with:

```bash
distrobox enter fedora-dev1
sudo dnf install cmake gcc-c++ qt6-qtbase-devel ImageMagick
```

Inside the container, the host project is mounted at
`/run/host/development/projects/lgl-papercutter`. Configure and test with:

```bash
cd /run/host/development/projects/lgl-papercutter
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

This workflow has been verified in `fedora-dev1`. Host toolchain availability
is not used to assess the project build.
