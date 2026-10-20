# Product Objectives

[Back to the project overview](../README.md)

## Status

This document records the current product contract for LGL Papercutter.

## Primary objective

LGL Papercutter turns existing images into high-quality wallpapers that
match a user's chosen desktop resolution. The complete workflow will run
locally and give the user direct control over composition before any file is
changed.

## Target users

The first release is intended for Linux desktop users who maintain their own
wallpaper collection and want images to fit ultrawide, widescreen, or custom
display resolutions without relying on a cloud editor or command line.

## Core workflow

The application supports both a single image and a reviewable batch. A
batch may be created by selecting multiple files or by loading the supported
images directly inside one folder. Subfolders will not be scanned in v1.

For every queued image, the user will:

1. Select a target from connected displays, saved resolution presets, or
   manually entered width and height values.
2. Compose the source behind a fixed-aspect **composition window** whose shape
   represents the target resolution.
3. Drag to position the image and zoom with a slider or mouse wheel.
4. Review the full-frame crop and approve the image before moving to the next
   queued item.
5. Start processing after all desired compositions are ready.

Zooming will permit any enlargement required to fill the target. The editor
will display the effective upscale factor and warn when enlargement may make
the result look soft, but it will not prevent export.

## Resolution and composition

Connected displays will be offered as convenient targets. Users may override
detection with custom pixel dimensions and save frequently used dimensions as
presets. The preview and export pipeline must interpret position, scale, and
crop identically so the saved wallpaper matches the approved composition. The
source is always scaled proportionally until it covers the complete target;
overflow is cropped according to the user's pan and zoom settings. The output
must never introduce letterbox gaps or distort the source aspect ratio.

## File handling

JPEG, PNG, and WebP are the supported v1 source formats. Save As writes a new
processed copy, remembers the user's last destination folder, and refuses to
use the source path as its destination. Export never replaces a source image.

## Success criteria

The first release succeeds when a user can:

- Load supported wallpapers without altering them.
- Target a detected or custom desktop resolution.
- Preview and control the composition of every selected image.
- Produce an image with the exact requested pixel dimensions.
- Save a separate processed copy without altering the source.
- Process a batch without the interface freezing and understand which images
  succeeded, failed, or were cancelled.
- Complete the workflow entirely offline and without administrator access.

## Not planned for v1

- AI upscaling or generative image extension
- Cloud storage, processing, accounts, telemetry, or synchronisation
- Automatically setting the active desktop wallpaper
- Recursive folder scanning
- Unattended automatic framing of an entire batch
- Arbitrary image rotation or general-purpose photo editing
- Packages for operating systems or Linux distributions other than the Fedora
  44 RPM

The core design should remain portable so additional Linux packages can be
added later without replacing the editor or processing model.

## Related documents

- [Planned tools and dependencies](TOOLS.md)
- [Product and engineering principles](PRINCIPLES.md)
