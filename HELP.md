# LGL Papercutter Help

LGL Papercutter prepares wallpaper copies for a chosen screen resolution. Editing is staged inside the app: your source images are unchanged until you explicitly use **Move to Trash**.

## Main controls

- **Add Images** opens your desktop file picker and adds JPEG, PNG, or WebP images to the queue. You can also drag images or folders onto the window.
- **Remove selected** removes the selected entries from the queue without deleting their source files. Pressing the **Delete** key is a shortcut for **Remove selected**; it only removes queue entries and does not delete images from your computer.
- **Remove duplicates** compares image contents and removes later duplicate entries from the queue. It does not delete files.
- **More** contains **Save As…** and the menu-bar visibility setting.
- **Save As…** exports the selected staged image or images as new files.

The full menu bar also provides **Select All** (`Ctrl+A`), **Add wallpaper folder…**, **Quit**, and this Help/About information. Use **Show Menu Bar** (`Ctrl+Shift+M`) from the More menu if the menu bar is hidden.

## Queue and selection

Click an image to make it the current image. Use `Ctrl` or `Shift` while clicking to select several images, or use **File > Select All**.

Each queued image keeps its own staged target resolution, zoom, and position. Switching to another image does not discard those changes.

Right-click a queued image for:

- **Save As…** — export the selected image or images.
- **Move to Trash** — move the original source files to the desktop Trash after confirmation. This is the only queue command that changes the source files.
- **Remove from Queue** — stop working with the selected entries without touching their files.

## Composition controls

- **Target resolution** chooses a detected display size or saved preset.
- The **width × height** boxes set a custom output size.
- Changing the target resolution while several images are selected applies it to all selected images. If selected images currently use different sizes, the menu shows **Mixed resolutions**.
- **Zoom** changes only the current image. Drag the image on the canvas to position its crop. You can also use the mouse wheel over the canvas to zoom.
- The scale information warns when an image must be enlarged and helps indicate likely output quality.

All changes remain staged per image until you export them.

## Saving one image

Select one image and choose **Save As…**. Pick a new filename and location in the desktop file picker. Papercutter will not overwrite the original source. If another file already uses that destination, you decide whether to replace it.

A confirmation window shows the exact source and destination before the export starts.

## Saving several images

Select multiple queue entries and choose **Save As…**, then choose a destination folder.

If that folder contains files whose complete names before the extension are numbers, Papercutter finds the highest number and offers to continue the sequence. For example, if `0312.jpg` is the highest numbered file, new exports can begin at `0313`. Existing zero padding is retained. If no numbered filename exists, each image keeps its current filename.

Before anything is written, the confirmation window lists every source filename beside its planned destination. Drag the divider between the two columns when longer filenames need more room.

Exports are written as new files and validated before completion. Failed or skipped items are listed in a batch report; successful items remain saved.

## File safety summary

- Staging, zooming, cropping, resizing, queue removal, and duplicate removal do not modify source files.
- **Save As…** creates processed copies at destinations you approve.
- **Move to Trash** is the only action that removes source files, and it asks for confirmation first.
