/**
 * AppClip Manager - Clipboard Monitoring Subsystem
 * Monitors X11 and Wayland clipboards with deduplication and asynchronous notifications.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef CLIPBOARD_MONITOR_H
#define CLIPBOARD_MONITOR_H

#include <gtk/gtk.h>
#include "clip_model.h"

typedef struct _ClipboardMonitor ClipboardMonitor;

/**
 * Callback invoked when a new clip is captured and inserted into SQLite.
 * entry: The newly created ClipEntry pointer (borrowed, do not free).
 * user_data: Context provided at monitor creation.
 */
typedef void (*ClipboardChangeCallback)(ClipEntry *entry, gpointer user_data);

/**
 * Allocates a new ClipboardMonitor instance.
 */
ClipboardMonitor *clipboard_monitor_new(ClipboardChangeCallback callback, gpointer user_data);

/**
 * Starts monitoring the clipboard according to current session type (X11 or Wayland).
 */
void clipboard_monitor_start(ClipboardMonitor *mon);

/**
 * Stops clipboard monitoring and kills any spawned child watcher processes.
 */
void clipboard_monitor_stop(ClipboardMonitor *mon);

/**
 * Frees the ClipboardMonitor instance.
 */
void clipboard_monitor_free(ClipboardMonitor *mon);

/**
 * Returns TRUE if running in a Wayland session ($XDG_SESSION_TYPE == "wayland").
 */
gboolean clipboard_monitor_is_wayland(ClipboardMonitor *mon);

/**
 * Returns TRUE if running in Wayland AND wl-paste binary is missing from PATH.
 */
gboolean clipboard_monitor_is_wayland_missing_tools(ClipboardMonitor *mon);

/**
 * Returns the detected session type string ("x11", "wayland", or "unknown").
 */
const char *clipboard_monitor_get_session_type(ClipboardMonitor *mon);

/**
 * Temporarily pause capture (e.g. when "Copy Again" is invoked inside the app,
 * to prevent immediate self-echoing).
 */
void clipboard_monitor_ignore_next(ClipboardMonitor *mon, const char *hash);

#endif /* CLIPBOARD_MONITOR_H */
