# Planned Tools and Technology

[Back to the project overview](../README.md)

## Status

This is the implemented application stack. The root build system, application,
unit tests, and RPM spec build the maintained GTK application.

## Application stack

| Area | Planned tool | Purpose |
|---|---|---|
| Language | C++17 | Native application development |
| Interface | GTK 4 (gtkmm) + libadwaita | Accessible desktop UI with light/dark theme support |
| Build | CMake | Configuration, dependency discovery, installation, and tests |
| Compiler | GCC | Primary Fedora 44 compiler, with Clang compatibility where practical |
| Processing | ImageMagick 7 | Decode, resize, crop, composite, colour-manage, and encode images |
| Background work | Gio::Subprocess and worker threads | Keep image loading, preview generation, and export off the UI thread |
| Tests | GLib test framework and CTest | Unit, integration, and pipeline verification |
| Packaging | RPM spec | Fedora 44 source and binary package definition |

Enter the Fedora 44 development container and install the development stack
with:

```bash
distrobox enter fedora-dev1
sudo dnf install cmake gcc-c++ gtkmm4.0-devel libadwaita-devel json-glib-devel ImageMagick exiv2
```

GTK owns the interactive preview and application state. ImageMagick will
be the source of truth for final rendering, using explicit crop geometry and a
high-quality resampling filter. The application will invoke `magick` directly
with Gio::Subprocess, pass arguments without a shell, and treat its exit status and
diagnostics as part of each processing result. Preview calculations must use
the same normalized transform model as the export pipeline.

ImageMagick is available across major Linux distributions. Packaging and
dependency names remain distribution-specific, while the processing command
builder stays independent of Fedora.

## Planned project conventions

The initial scaffold should follow the proven conventions in
`lgl-system-loadout` where they apply:

- Require C++17.
- Compile GTK resources with `glib-compile-resources`.
- Compile GCC and Clang builds with `-Wall -Wextra -Werror`.
- Offer AddressSanitizer and UndefinedBehaviorSanitizer through the explicit
  `PAPERCUTTER_ENABLE_SANITIZERS` CMake option.
- Offer a separate ThreadSanitizer build configuration.
- Run the application as the signed-in user; no privileged helper or polkit
  policy is required.
- Use the system GTK theme and palette instead of hard-coded application
  colours.

## Image formats and encoding

The RPM requires ImageMagick 7 for JPEG, PNG, and WebP processing. Export
settings will preserve the source format and favour quality:

- Apply source orientation before composition.
- Convert through a defined working colour space and preserve a valid embedded
  ICC profile when possible.
- Use high-quality downsampling and upsampling.
- Retain transparency for formats that support it.
- Avoid changing an extension without also using the matching encoder.
- Validate decoded dimensions and exported dimensions before replacement.

Exact encoder values should be centralized as named application defaults and
covered by tests rather than scattered through UI code.

## Fedora 44 packaging

The first distribution artifact will be an RPM for Fedora 44. Packaging will
include:

- The application executable under the standard binary directory
- A reverse-DNS desktop entry and matching AppStream component ID
- Hicolor application icons at the standard sizes
- AppStream metadata suitable for graphical software centres
- Runtime dependencies for ImageMagick and exiv2
- Build dependencies for CMake, GCC, gtkmm, libadwaita, and JSON-GLib

The package will not request root privileges at runtime. RPM installation is
the only privileged step and is handled by the user's package manager.

## Developer workflow

The repository provides the following CMake workflow:

```bash
cd /run/host/development/projects/lgl-papercutter
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

These commands are verified inside `fedora-dev1`, where the host workspace is
mounted below `/run/host`. RPM release-source instructions will be added when
the first tagged release is prepared.

## Related documents

- [Product objectives](OBJECTIVES.md)
- [Product and engineering principles](PRINCIPLES.md)
