/**
 * AppClip Manager - Updates UI Module Header
 * GtkStack panel presenting available application updates with individual and batch installation.
 *
 * Created by: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef UI_UPDATES_H
#define UI_UPDATES_H

#include <gtk/gtk.h>
#include "update_checker.h"

typedef struct _UiUpdatesContext UiUpdatesContext;

/**
 * Toast callback signature to display floating toasts in the unified shell.
 */
typedef void (*UiUpdatesToastFunc)(const char *message, gpointer user_data);

/**
 * Badge update callback to update the GtkStack tab label badge.
 */
typedef void (*UiUpdatesBadgeFunc)(guint count, gpointer user_data);

/**
 * Allocates and builds the "Updates" panel container.
 * Returns the top-level GtkWidget to be added to GtkStack.
 */
GtkWidget *ui_updates_create_panel(UiUpdatesContext **out_ctx, GtkWindow *parent_window);

/**
 * Sets callback to trigger unified shell toast notifications.
 */
void ui_updates_set_toast_callback(UiUpdatesContext *ctx, UiUpdatesToastFunc func, gpointer user_data);

/**
 * Sets callback to notify shell of available update count for tab badge.
 */
void ui_updates_set_badge_callback(UiUpdatesContext *ctx, UiUpdatesBadgeFunc func, gpointer user_data);

/**
 * Programmatically triggers an update check (e.g. at startup or on explicit click).
 * If @refresh_apt_cache is TRUE, prompts root via pkexec to refresh repository indexes.
 * If FALSE, runs non-intrusively in the background.
 */
void ui_updates_trigger_check(UiUpdatesContext *ctx, gboolean refresh_apt_cache);

/**
 * Frees resources associated with UiUpdatesContext.
 */
void ui_updates_free(UiUpdatesContext *ctx);

#endif /* UI_UPDATES_H */
