/**
 * AppClip Manager - Clipboard Monitor Implementation
 * X11 GtkClipboard owner-change listener and Wayland fallback watcher.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "clipboard_monitor.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

struct _ClipboardMonitor {
    GtkClipboard *clipboard;
    gulong owner_change_handler_id;
    ClipboardChangeCallback callback;
    gpointer user_data;

    gboolean running;
    gboolean is_wayland;
    gboolean wayland_missing_tools;
    char *session_type;

    char *last_seen_hash;
    char *ignore_next_hash;

    /* Wayland child watcher process ID (if running) */
    GPid wayland_child_pid;
    guint wayland_poll_timeout_id;
};

static void process_captured_text(ClipboardMonitor *mon, const char *text)
{
    if (!mon || !text || text[0] == '\0') return;

    char *hash = clip_compute_text_hash(text);

    /* Ignore immediate echo if caused by our own "Copy Again" action */
    if (mon->ignore_next_hash && strcmp(mon->ignore_next_hash, hash) == 0) {
        g_free(mon->ignore_next_hash);
        mon->ignore_next_hash = NULL;
        g_free(hash);
        return;
    }

    /* Deduplication check: compare against last seen hash */
    char *db_last_hash = db_get_last_content_hash();
    if ((mon->last_seen_hash && strcmp(mon->last_seen_hash, hash) == 0) ||
        (db_last_hash && strcmp(db_last_hash, hash) == 0)) {
        g_free(db_last_hash);
        g_free(hash);
        return;
    }
    g_free(db_last_hash);

    /* Generate first 100 character preview */
    char *preview = clip_generate_text_preview(text, 100);

    /* Persist to SQLite */
    int new_id = db_insert_text_clip(text, preview, hash);
    if (new_id > 0) {
        g_free(mon->last_seen_hash);
        mon->last_seen_hash = g_strdup(hash);

        if (mon->callback) {
            ClipEntry *entry = clip_entry_new();
            entry->id = new_id;
            entry->type = CLIP_TYPE_TEXT;
            entry->content = g_strdup(text);
            entry->preview = g_strdup(preview);
            entry->content_hash = g_strdup(hash);
            entry->created_at = time(NULL);
            entry->pinned = FALSE;

            mon->callback(entry, mon->user_data);
            clip_entry_free(entry);
        }
    }

    g_free(preview);
    g_free(hash);
}

static void process_captured_image(ClipboardMonitor *mon, GdkPixbuf *pixbuf)
{
    if (!mon || !pixbuf) return;

    char *hash = clip_compute_pixbuf_hash(pixbuf);

    if (mon->ignore_next_hash && strcmp(mon->ignore_next_hash, hash) == 0) {
        g_free(mon->ignore_next_hash);
        mon->ignore_next_hash = NULL;
        g_free(hash);
        return;
    }

    char *db_last_hash = db_get_last_content_hash();
    if ((mon->last_seen_hash && strcmp(mon->last_seen_hash, hash) == 0) ||
        (db_last_hash && strcmp(db_last_hash, hash) == 0)) {
        g_free(db_last_hash);
        g_free(hash);
        return;
    }
    g_free(db_last_hash);

    /* Save image to disk as PNG in ~/.local/share/appclip-manager/clips/ */
    char *file_path = db_save_pixbuf_to_file(pixbuf);
    if (!file_path) {
        g_free(hash);
        return;
    }

    int new_id = db_insert_image_clip(file_path, hash);
    if (new_id > 0) {
        g_free(mon->last_seen_hash);
        mon->last_seen_hash = g_strdup(hash);

        if (mon->callback) {
            ClipEntry *entry = clip_entry_new();
            entry->id = new_id;
            entry->type = CLIP_TYPE_IMAGE;
            entry->file_path = g_strdup(file_path);
            entry->content_hash = g_strdup(hash);
            entry->created_at = time(NULL);
            entry->pinned = FALSE;
            entry->image_width = gdk_pixbuf_get_width(pixbuf);
            entry->image_height = gdk_pixbuf_get_height(pixbuf);

            mon->callback(entry, mon->user_data);
            clip_entry_free(entry);
        }
    }

    g_free(file_path);
    g_free(hash);
}

/**
 * X11 Signal handler for GtkClipboard "owner-change".
 */
static void on_x11_clipboard_owner_change(GtkClipboard *cb, GdkEventOwnerChange *event, gpointer user_data)
{
    (void)event;
    ClipboardMonitor *mon = (ClipboardMonitor *)user_data;
    if (!mon || !mon->running) return;

    /* Check for text first */
    if (gtk_clipboard_wait_is_text_available(cb)) {
        char *text = gtk_clipboard_wait_for_text(cb);
        if (text) {
            process_captured_text(mon, text);
            g_free(text);
            return;
        }
    }

    /* Check for image second */
    if (gtk_clipboard_wait_is_image_available(cb)) {
        GdkPixbuf *pixbuf = gtk_clipboard_wait_for_image(cb);
        if (pixbuf) {
            process_captured_image(mon, pixbuf);
            g_object_unref(pixbuf);
            return;
        }
    }
}

/**
 * Wayland fallback: Query wl-paste periodically or pipe output.
 * Since GtkClipboard's owner-change is not supported on pure Wayland compositors,
 * we invoke wl-paste safely via fork()+execvp() or GSubprocess.
 */
static gboolean wayland_poll_clipboard(gpointer user_data)
{
    ClipboardMonitor *mon = (ClipboardMonitor *)user_data;
    if (!mon || !mon->running) return G_SOURCE_REMOVE;

    /* Run `wl-paste -n` to query current text */
    char *argv[] = { "wl-paste", "-n", NULL };
    char *stdout_text = NULL;
    GError *err = NULL;

    if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                     NULL, NULL, &stdout_text, NULL, NULL, &err)) {
        if (stdout_text && stdout_text[0] != '\0') {
            process_captured_text(mon, stdout_text);
        }
        g_free(stdout_text);
    } else {
        if (err) g_error_free(err);
    }

    return G_SOURCE_CONTINUE;
}

ClipboardMonitor *clipboard_monitor_new(ClipboardChangeCallback callback, gpointer user_data)
{
    ClipboardMonitor *mon = g_new0(ClipboardMonitor, 1);
    mon->callback = callback;
    mon->user_data = user_data;
    mon->running = FALSE;
    mon->last_seen_hash = NULL;
    mon->ignore_next_hash = NULL;
    mon->wayland_child_pid = 0;
    mon->wayland_poll_timeout_id = 0;

    /* 1. Detect Session Type from $XDG_SESSION_TYPE */
    const char *env_sess = g_getenv("XDG_SESSION_TYPE");
    if (env_sess && env_sess[0] != '\0') {
        mon->session_type = g_ascii_strdown(env_sess, -1);
    } else {
        mon->session_type = g_strdup("x11");
    }

    mon->is_wayland = (g_strcmp0(mon->session_type, "wayland") == 0);

    /* 2. If Wayland, check if wl-paste binary exists */
    if (mon->is_wayland) {
        char *wl_paste_bin = g_find_program_in_path("wl-paste");
        if (!wl_paste_bin) {
            mon->wayland_missing_tools = TRUE;
            g_message("Wayland session detected without 'wl-clipboard'. Please install with: sudo apt install wl-clipboard");
        } else {
            mon->wayland_missing_tools = FALSE;
            g_free(wl_paste_bin);
        }
    } else {
        mon->wayland_missing_tools = FALSE;
    }

    mon->clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    return mon;
}

void clipboard_monitor_start(ClipboardMonitor *mon)
{
    if (!mon || mon->running) return;
    mon->running = TRUE;

    /* Initialize last seen hash from existing DB entries to prevent re-adding on launch */
    mon->last_seen_hash = db_get_last_content_hash();

    if (mon->is_wayland) {
        /* WAYLAND PATH:
         * If wl-paste is available, set up watcher timer.
         * Also attach to GtkClipboard as a secondary mechanism.
         */
        if (!mon->wayland_missing_tools) {
            mon->wayland_poll_timeout_id = g_timeout_add(1000, wayland_poll_clipboard, mon);
        }

        /* GtkClipboard fallback */
        mon->owner_change_handler_id = g_signal_connect(
            mon->clipboard,
            "owner-change",
            G_CALLBACK(on_x11_clipboard_owner_change),
            mon
        );
    } else {
        /* X11 PATH:
         * Connect to owner-change signal on GDK_SELECTION_CLIPBOARD
         */
        mon->owner_change_handler_id = g_signal_connect(
            mon->clipboard,
            "owner-change",
            G_CALLBACK(on_x11_clipboard_owner_change),
            mon
        );
    }
}

void clipboard_monitor_stop(ClipboardMonitor *mon)
{
    if (!mon || !mon->running) return;
    mon->running = FALSE;

    if (mon->owner_change_handler_id > 0) {
        if (g_signal_handler_is_connected(mon->clipboard, mon->owner_change_handler_id)) {
            g_signal_handler_disconnect(mon->clipboard, mon->owner_change_handler_id);
        }
        mon->owner_change_handler_id = 0;
    }

    if (mon->wayland_poll_timeout_id > 0) {
        g_source_remove(mon->wayland_poll_timeout_id);
        mon->wayland_poll_timeout_id = 0;
    }
}

void clipboard_monitor_free(ClipboardMonitor *mon)
{
    if (!mon) return;

    clipboard_monitor_stop(mon);

    if (mon->session_type) g_free(mon->session_type);
    if (mon->last_seen_hash) g_free(mon->last_seen_hash);
    if (mon->ignore_next_hash) g_free(mon->ignore_next_hash);

    g_free(mon);
}

gboolean clipboard_monitor_is_wayland(ClipboardMonitor *mon)
{
    return mon ? mon->is_wayland : FALSE;
}

gboolean clipboard_monitor_is_wayland_missing_tools(ClipboardMonitor *mon)
{
    return mon ? mon->wayland_missing_tools : FALSE;
}

const char *clipboard_monitor_get_session_type(ClipboardMonitor *mon)
{
    return mon ? mon->session_type : "unknown";
}

void clipboard_monitor_ignore_next(ClipboardMonitor *mon, const char *hash)
{
    if (!mon) return;
    if (mon->ignore_next_hash) g_free(mon->ignore_next_hash);
    mon->ignore_next_hash = hash ? g_strdup(hash) : NULL;
}
