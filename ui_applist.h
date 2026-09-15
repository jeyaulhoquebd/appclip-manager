#ifndef UI_APPLIST_H
#define UI_APPLIST_H

#include "app_model.h"
#include <gtk/gtk.h>

/* Forward declaration */
typedef struct _UiAppContext UiAppContext;

/**
 * UiAppContext:
 * Encapsulates the UI state, widgets, and application controls.
 */
struct _UiAppContext {
    GtkApplicationWindow *window;
    GtkListBox *list_box;
    GtkSearchEntry *search_entry;
    GtkSpinner *spinner;
    GtkProgressBar *progress_bar;
    GtkWidget *statusbar_label;
    GtkWidget *toast_revealer;
    GtkWidget *toast_label;
    GtkWidget *empty_state_box;
    GtkWidget *btn_refresh;
    GPtrArray *current_store;
    guint toast_timer_id;
    guint pulse_timer_id;
};

/**
 * ui_applist_create_row:
 * Creates and populates a GtkListBoxRow representing an AppInfo item.
 */
GtkWidget *ui_applist_create_row(AppInfo *app, UiAppContext *ctx);

/**
 * ui_applist_filter_func:
 * Implements case-insensitive search filtering for gtk_list_box_set_filter_func.
 */
gboolean ui_applist_filter_func(GtkListBoxRow *row, gpointer user_data);

/**
 * ui_applist_populate:
 * Clears and rebuilds the GtkListBox rows from a GPtrArray of AppInfo items.
 */
void ui_applist_populate(UiAppContext *ctx, GPtrArray *apps);

/**
 * ui_applist_show_toast:
 * Displays a non-intrusive floating toast notification for 3 seconds.
 */
void ui_applist_show_toast(UiAppContext *ctx, const char *message);

/**
 * ui_applist_set_busy_state:
 * Controls the spinner and progress bar state during background operations.
 */
void ui_applist_set_busy_state(UiAppContext *ctx, gboolean is_busy, const char *action_desc);

/**
 * ui_applist_on_uninstall_clicked:
 * Action handler triggered when the row's "Uninstall" button is clicked.
 */
void ui_applist_on_uninstall_clicked(GtkButton *button, gpointer user_data);

#endif /* UI_APPLIST_H */
