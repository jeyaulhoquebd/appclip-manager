/**
 * AppClip Manager - System Cleaner UI Panel
 * Header for leftover inspection, selective deletion, and audit management.
 *
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef UI_CLEANER_H
#define UI_CLEANER_H

#include <gtk/gtk.h>
#include "orphan_scanner.h"

typedef struct _UiCleanerContext UiCleanerContext;

typedef void (*CleanerToastCallback)(const char *message, gpointer user_data);

typedef struct {
    GtkWidget *check_button;
    OrphanFileItem *item;
    OrphanAppGroup *parent_group;
} CleanerRowEntry;

struct _UiCleanerContext {
    GtkWindow *window;
    GtkWidget *panel;
    GtkListBox *list_box;
    GtkWidget *spinner;
    GtkWidget *progress_bar;
    GtkWidget *statusbar_label;
    GtkWidget *btn_scan;
    GtkWidget *btn_select_safe;
    GtkWidget *btn_deselect_all;
    GtkWidget *btn_delete_selected;
    GtkWidget *empty_state_box;
    GtkWidget *scrolled_window;
    GList *current_groups; /* GList of OrphanAppGroup* */
    GList *row_entries;    /* GList of CleanerRowEntry* */
    CleanerToastCallback toast_cb;
    gpointer toast_user_data;
    gboolean is_busy;
};

/**
 * Creates the System Cleaner main panel widget and initializes UI context.
 */
GtkWidget *ui_cleaner_create_panel(UiCleanerContext **out_ctx, GtkWindow *parent_window);

/**
 * Sets callback for firing unified floating toast messages.
 */
void ui_cleaner_set_toast_callback(UiCleanerContext *ctx, CleanerToastCallback cb, gpointer user_data);

/**
 * Initiates an asynchronous full scan across uninstalled apps and deep cache.
 */
void ui_cleaner_trigger_scan(UiCleanerContext *ctx);

/**
 * Populates the UI directly with leftover items for an uninstalled package
 * (triggered via post-uninstall review prompt).
 */
void ui_cleaner_show_package_leftovers(UiCleanerContext *ctx,
                                      const char *package_name,
                                      const char *display_name,
                                      GList *items,
                                      guint64 total_bytes);

/**
 * Reloads cleaner data from SQLite and rescans.
 */
void ui_cleaner_reload(UiCleanerContext *ctx);

#endif /* UI_CLEANER_H */
