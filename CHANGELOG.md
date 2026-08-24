# Changelog

All notable user-facing changes to LGL Papercutter are recorded here.

## 0.3.0 — 2026-08-24

### Added

- Comprehensive in-app Help explaining toolbar buttons, queue selection,
  composition controls, staged per-image state, single and batch Save As,
  numbered output planning, and file-safety behaviour.
- An About dialog with the Papercutter icon, current application version,
  project and issue-tracker link, LinuxGamerLife link, MIT licence information,
  and Ko-fi support link.
- Help and About actions in both the Help menu and the always-visible More menu.

### Changed

- Clarified that the Delete key is a shortcut for removing selected queue
  entries and does not delete source images from the computer.
- Included the standalone help document in source archives and RPM documentation.

## 0.2.0 — 2026-08-24

### Added

- Multi-selection and Select All support for applying a target resolution to
  several queued images at once.
- Independent staged resolution, crop, position, and zoom state for every
  queued image.
- A mixed-resolution state when selected images have different staged targets.
- Batch Save As with an itemised source-to-destination preview.
- Optional sequential output numbering based on the highest numbered file in
  the selected destination folder.
- Right-click queue actions for Save As, Remove from Queue, and Move to Trash.

### Changed

- Replaced the source-overwriting Accept & Save workflow with non-destructive
  Save As exports.
- Improved export confirmation with a resizable, scrollable two-column file
  preview and adjustable column divider.
- Improved integration with KDE theming, desktop portals, and native file
  dialogs when installed as a package.
- Preserved each selected image's extension during numbered batch exports.

### Removed

- Accept & Save, source replacement, backup creation, backup history, and
  restoration controls.
- The redundant toolbar save icon.

## 0.1.0 — 2026-08-20

- Initial Fedora development release with visual wallpaper composition,
  display-resolution targets, zoom and positioning controls, and local
  ImageMagick processing.
