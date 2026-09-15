/**
 * AppClip Manager - Clip Detail Viewer Header
 * Full content viewer dialog for text and image clipboard entries.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef UI_CLIP_DETAIL_H
#define UI_CLIP_DETAIL_H

#include <gtk/gtk.h>
#include "clip_model.h"

/**
 * Displays the full, untruncated clip content in a dedicated modal dialog.
 * 
 * For text clips:
 * - Shows complete text in a GtkTextView inside a GtkScrolledWindow
 * - Monospace font detection for code/syntax patterns
 * - Full date/time timestamp, character count, and word count in header
 * - "Copy to Clipboard", "Edit" toggle with "Save Changes", "Save As File...", "Close"
 * - Keyboard shortcuts: Ctrl+C, Ctrl+S, Escape
 * - Unsaved changes prompt before closing
 * 
 * For image clips:
 * - Full-resolution image preview in a GtkScrolledWindow
 * - Zoom controls: Zoom In, Zoom Out, Fit to Window, 100% Original
 * - Header shows image dimensions, disk size, and timestamp
 * - "Copy to Clipboard", "Save As...", "Open With...", "Close"
 * - Keyboard shortcuts: Ctrl+C, Ctrl+S, Escape
 * 
 * Returns TRUE if the clip content was modified in SQLite, FALSE otherwise.
 */
gboolean show_clip_detail_dialog(GtkWindow *parent, int clip_id, ClipType type);

#endif /* UI_CLIP_DETAIL_H */
