/**
 * AppClip Manager - Clipboard UI Component Header
 * GtkListBox representation for clipboard history, row actions, and filtering.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef UI_CLIPLIST_H
#define UI_CLIPLIST_H

#include <gtk/gtk.h>
#include "clip_model.h"
#include "clipboard_monitor.h"
#include "settings.h"

typedef struct _UiClipContext UiClipContext;

struct _UiClipContext {
    GtkWidget *container;           /* Root GtkBox of the clipboard panel */
    GtkWindow *parent_window;       /* Parent GtkWindow for dialogs and toasts */
    GtkSearchEntry *search_entry;   /* Search filtering input */
    GtkListBox *list_box;           /* Scrollable list box containing clip rows */
    GtkWidget *infobar_wayland;     /* Wayland missing wl-clipboard banner */
    GtkWidget *btn_clear_history;   /* Clear history button */
    GtkWidget *lbl_counts;          /* Status label showing clip counts */
    GtkWidget *empty_state_box;     /* Shown when list is empty */

    ClipboardMonitor *monitor;      /* Active clipboard monitor */
    AppSettings settings;           /* Current settings */

    /* Toast notification callback/hook */
    void (*show_toast)(const char *message, gpointer user_data);
    gpointer toast_user_data;
};

/**
 * Creates and initializes the complete Clipboard History UI panel.
 * Can be embedded directly into a GtkNotebook tab or a standalone window.
 */
GtkWidget *ui_cliplist_create_panel(UiClipContext **out_ctx, GtkWindow *parent_window);

/**
 * Loads and populates all clips from SQLite into the GtkListBox.
 */
void ui_cliplist_reload(UiClipContext *ctx);

/**
 * Adds a newly captured clip entry at the top of the GtkListBox.
 */
void ui_cliplist_add_entry(UiClipContext *ctx, const ClipEntry *entry);

/**
 * Sets a toast notification callback for showing user feedback ("Copied to clipboard", etc.).
 */
void ui_cliplist_set_toast_callback(UiClipContext *ctx, void (*toast_cb)(const char *, gpointer), gpointer user_data);

/**
 * Frees the UiClipContext.
 */
void ui_cliplist_free(UiClipContext *ctx);

#endif /* UI_CLIPLIST_H */
