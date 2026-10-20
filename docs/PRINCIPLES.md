# Product and Engineering Principles

[Back to the project overview](../README.md)

## Status

These principles govern the design and implementation of LGL
Papercutter. When a feature choice conflicts with a principle, the principle
should be treated as the default unless the project documentation is updated
deliberately.

## Protect the user's originals

File safety is the strongest invariant in the application. Export never
replaces a source image. Render to a temporary file, validate it, and commit it
only to the separate destination selected by the user. Report failures with the
affected source and destination instead of hiding them.

Cancellation must stop at a safe boundary. An image already committed may
remain processed, while an image not yet committed must retain its source.

## Keep processing private and local

Wallpapers stay on the user's computer. The application will not upload image
content, require an account, depend on a network connection, or collect
telemetry. It will run as the normal user and access only files and folders the
user selects or configures.

## Make composition visual and controllable

The central experience is the composition window, not a list of opaque image
settings. Users should be able to see the target aspect ratio, pan by dragging,
zoom predictably, and reset a composition. Every batch item
will be reviewable rather than framed without consent.

The app may provide useful defaults, but it must not conceal how a source will
be cropped or filled.

## Make preview and export agree

Position and scale should be stored in resolution-independent terms and shared
between the preview renderer and ImageMagick export pipeline. The final file must
match the approved framing at its exact target dimensions. Orientation,
aspect-ratio rounding, transparency, and high-DPI display scaling must not
cause preview/export drift.

## Prefer quality while communicating limits

Preserve the source's JPEG, PNG, or WebP format and use high-quality resampling
and encoding defaults. Respect orientation and colour information. Do not
claim to create image detail that the source does not contain.

Users may upscale without an arbitrary limit because unusual resolutions are
a core use case. Show the effective enlargement factor and a calm quality
warning when substantial upscaling may soften the result; do not block the
operation.

## Keep the interface responsive

Image decoding, preview generation, export, backup copying,
and verification must not block the GTK main loop. Long-running work should
show determinate progress when measurable, expose cancellation, and return a
per-image result. Background tasks must have explicit lifetime and shutdown
rules.

## Fit the Linux desktop

Use GTK 4 (gtkmm) controls with libadwaita's styling base. Follow the system
palette and support both light and dark themes. Prefer readable typography, clear labels,
keyboard-accessible controls, visible focus, and layouts that remain usable at
different window and font sizes.

The planned persistent main window contains a processing queue, a large
composition canvas, a controls panel, and previous/next review navigation.
Avoid duplicating the editor into separate single-image and batch experiences.

## Keep the code testable and portable

Separate composition state, file-safety operations, image processing, and UI
presentation. The processing core must not depend on Fedora-specific APIs.
Confine display discovery, settings storage, and RPM integration to clear
platform boundaries.

Follow the relevant `lgl-system-loadout` engineering conventions: CMake,
C++17, strict warnings, sanitizer configurations, native themes, readable
typography, MIT licensing, and complete Fedora desktop packaging. Do not copy
its privileged-helper architecture because Papercutter needs no privileged
runtime operations.

## Keep scope honest

Documentation and UI text must distinguish implemented behaviour from planned
behaviour. Do not advertise AI enhancement, cloud services, automatic desktop
wallpaper changes, recursive processing, or cross-distribution packages until
those features exist and have defined safety and test requirements.

## Shared terminology

- **Source image:** the unprocessed file selected by the user.
- **Target resolution:** the exact output width and height in pixels.
- **Composition window:** the fixed-aspect viewport representing the target.
- **Cover crop:** proportional scaling that fills the target and crops overflow.
- **Effective upscale factor:** the output scale relative to source pixels.
- **Backup folder:** the configured destination for untouched originals.
- **Processing queue:** the ordered set of images awaiting review or export.

## Related documents

- [Product objectives](OBJECTIVES.md)
- [Planned tools and dependencies](TOOLS.md)
