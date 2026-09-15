/**
 * AppClip Manager - Updates UI Module Implementation
 * GtkStack panel presenting available application updates with individual and batch installation.
 *
 * Created by: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "ui_updates.h"
#include <time.h>
#include <string.h>

struct _UiUpdatesContext {
    GtkWindow *parent_window;
    GtkWidget *panel;

    /* Header Controls */
    GtkWidget *btn_check;
    GtkWidget *btn_update_all;
    GtkWidget *spinner;
    GtkWidget *lbl_last_checked;

    /* Progress Banner for Sequential Batch Updates */
    GtkWidget *progress_box;
    GtkWidget *progress_bar;
    GtkWidget *progress_label;

    /* List Display */
    GtkWidget *scrolled_window;
    GtkWidget *list_box;
    GtkWidget *empty_state_box;
    GtkWidget *statusbar_label;

    /* Data store */
    GPtrArray *current_updates;

    /* State flags */
    gboolean is_checking;
    gboolean is_updating_batch;

    /* Batch Execution State */
    guint batch_total;
    guint batch_current_idx;
    guint batch_success_count;
    GList *batch_failures;

    /* Callbacks */
    UiUpdatesToastFunc toast_cb;
    gpointer toast_user_data;

    UiUpdatesBadgeFunc badge_cb;
    gpointer badge_user_data;
};

typedef struct {
    UiUpdatesContext *ctx;
    GtkListBoxRow *row;
    UpdateInfo *info;
    GtkWidget *btn_update;
    GtkWidget *spinner;
} RowUpdateContext;

/* Forward declarations */
static void ui_updates_render_list(UiUpdatesContext *ctx);
static void batch_step_next(UiUpdatesContext *ctx);

/**
 * Format timestamp into human-readable string.
 */
static char *format_timestamp_now(void)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%b %d, %H:%M:%S", tm_info);
    return g_strdup(buf);
}

/**
 * Helper to create an icon widget with theme and file fallbacks.
 */
static GtkWidget *create_update_icon_widget(const char *icon_name)
{
    GtkWidget *image = NULL;
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();

    if (icon_name && *icon_name) {
        /* Case 1: Absolute path to image */
        if (icon_name[0] == '/' && g_file_test(icon_name, G_FILE_TEST_EXISTS)) {
            GError *err = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_name, 44, 44, TRUE, &err);
            if (pixbuf) {
                image = gtk_image_new_from_pixbuf(pixbuf);
                g_object_unref(pixbuf);
                return image;
            }
            if (err) g_error_free(err);
        }

        /* Case 2: Icon name in icon theme */
        if (gtk_icon_theme_has_icon(icon_theme, icon_name)) {
            image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_DND);
            gtk_image_set_pixel_size(GTK_IMAGE(image), 44);
            return image;
        }
    }

    /* Fallback generic update icon */
    image = gtk_image_new_from_icon_name("system-software-update", GTK_ICON_SIZE_DND);
    gtk_image_set_pixel_size(GTK_IMAGE(image), 44);
    return image;
}

/**
 * Callback when individual row upgrade completes.
 */
static void on_single_row_completed(gboolean success, const char *err_msg, gpointer user_data)
{
    RowUpdateContext *rctx = (RowUpdateContext *)user_data;
    UiUpdatesContext *ctx = rctx->ctx;
    UpdateInfo *info = rctx->info;

    gtk_spinner_stop(GTK_SPINNER(rctx->spinner));
    gtk_widget_hide(rctx->spinner);
    gtk_widget_show(rctx->btn_update);

    if (success) {
        char *toast_msg = g_strdup_printf("Successfully updated %s", info->display_name);
        if (ctx->toast_cb) {
            ctx->toast_cb(toast_msg, ctx->toast_user_data);
        }
        g_free(toast_msg);

        /* Remove from list_box */
        gtk_widget_destroy(GTK_WIDGET(rctx->row));

        /* Remove from current_updates array */
        for (guint i = 0; i < ctx->current_updates->len; i++) {
            if (g_ptr_array_index(ctx->current_updates, i) == info) {
                g_ptr_array_remove_index(ctx->current_updates, i);
                break;
            }
        }

        /* Update badge and status */
        if (ctx->badge_cb) {
            ctx->badge_cb(ctx->current_updates->len, ctx->badge_user_data);
        }

        if (ctx->current_updates->len == 0) {
            gtk_widget_show(ctx->empty_state_box);
            gtk_widget_set_sensitive(ctx->btn_update_all, FALSE);
            gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), "System is completely up to date.");
        } else {
            char *status = g_strdup_printf("%u updates available", ctx->current_updates->len);
            gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), status);
            g_free(status);
        }
    } else {
        GtkWidget *dialog = gtk_message_dialog_new(
            ctx->parent_window,
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "Could not update %s",
            info->display_name
        );
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(dialog),
            "%s",
            err_msg ? err_msg : "An error occurred during the update process."
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        gtk_widget_set_sensitive(rctx->btn_update, TRUE);
    }

    g_free(rctx);
}

/**
 * Click handler for row "Update" button.
 */
static void on_row_update_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    RowUpdateContext *rctx = (RowUpdateContext *)user_data;

    gtk_widget_set_sensitive(rctx->btn_update, FALSE);
    gtk_widget_hide(rctx->btn_update);
    gtk_widget_show(rctx->spinner);
    gtk_spinner_start(GTK_SPINNER(rctx->spinner));

    update_checker_upgrade_async(rctx->info, on_single_row_completed, rctx);
}

/**
 * Creates a list row for an UpdateInfo item.
 */
static GtkWidget *create_update_row(UpdateInfo *info, UiUpdatesContext *ctx)
{
    GtkWidget *row = gtk_list_box_row_new();
    gtk_widget_set_can_focus(row, TRUE);
    g_object_set_data(G_OBJECT(row), "update-info", info);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);

    /* 1. App Icon */
    GtkWidget *icon_widget = create_update_icon_widget(info->icon_name);
    gtk_widget_set_valign(icon_widget, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), icon_widget, FALSE, FALSE, 0);

    /* 2. App Name and Version Info (Vertical Box) */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);

    /* Name + Package ID Label */
    GtkWidget *lbl_name = gtk_label_new(NULL);
    char *name_markup = g_strdup_printf(
        "<span weight=\"bold\" size=\"medium\">%s</span> <span color=\"#64748b\" size=\"small\">(%s)</span>",
        info->display_name, info->package_name
    );
    gtk_label_set_markup(GTK_LABEL(lbl_name), name_markup);
    g_free(name_markup);
    gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_name, FALSE, FALSE, 0);

    /* Version Transition: Current -> Target */
    GtkWidget *lbl_ver = gtk_label_new(NULL);
    char *ver_markup = g_strdup_printf(
        "<span size=\"small\" color=\"#94a3b8\">Version: </span>"
        "<span font_desc=\"monospace\" size=\"small\" color=\"#e2e8f0\">%s</span> "
        "<span color=\"#38bdf8\">➔</span> "
        "<span font_desc=\"monospace\" size=\"small\" weight=\"bold\" color=\"#10b981\">%s</span>",
        info->current_version ? info->current_version : "Installed",
        info->new_version
    );
    gtk_label_set_markup(GTK_LABEL(lbl_ver), ver_markup);
    g_free(ver_markup);
    gtk_label_set_xalign(GTK_LABEL(lbl_ver), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_ver, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    /* 3. Package Source Badge & Size Pill */
    GtkWidget *meta_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_valign(meta_box, GTK_ALIGN_CENTER);

    if (info->download_size && *info->download_size) {
        GtkWidget *lbl_size = gtk_label_new(info->download_size);
        GtkStyleContext *sz_ctx = gtk_widget_get_style_context(lbl_size);
        gtk_style_context_add_class(sz_ctx, "size-label");
        gtk_box_pack_start(GTK_BOX(meta_box), lbl_size, FALSE, FALSE, 0);
    }

    GtkWidget *lbl_badge = gtk_label_new(package_type_to_string(info->type));
    GtkStyleContext *bctx = gtk_widget_get_style_context(lbl_badge);
    gtk_style_context_add_class(bctx, "badge");
    gtk_style_context_add_class(bctx, package_type_to_badge_class(info->type));
    gtk_box_pack_start(GTK_BOX(meta_box), lbl_badge, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), meta_box, FALSE, FALSE, 8);

    /* 4. Individual Update Button & Row Spinner */
    RowUpdateContext *rctx = g_new0(RowUpdateContext, 1);
    rctx->ctx = ctx;
    rctx->row = GTK_LIST_BOX_ROW(row);
    rctx->info = info;

    GtkWidget *action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_valign(action_box, GTK_ALIGN_CENTER);

    rctx->spinner = gtk_spinner_new();
    gtk_widget_set_no_show_all(rctx->spinner, TRUE);
    gtk_box_pack_start(GTK_BOX(action_box), rctx->spinner, FALSE, FALSE, 4);

    rctx->btn_update = gtk_button_new_with_label("Update");
    GtkWidget *btn_icon = gtk_image_new_from_icon_name("system-software-update-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(rctx->btn_update), btn_icon);
    gtk_button_set_always_show_image(GTK_BUTTON(rctx->btn_update), TRUE);

    GtkStyleContext *btn_ctx = gtk_widget_get_style_context(rctx->btn_update);
    gtk_style_context_add_class(btn_ctx, "suggested-action");

    g_signal_connect(rctx->btn_update, "clicked", G_CALLBACK(on_row_update_clicked), rctx);
    gtk_box_pack_start(GTK_BOX(action_box), rctx->btn_update, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(hbox), action_box, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(row), hbox);
    gtk_widget_show_all(row);

    return row;
}

/**
 * Rebuilds list box rows from current_updates array.
 */
static void ui_updates_render_list(UiUpdatesContext *ctx)
{
    /* Clear existing rows */
    GList *children = gtk_container_get_children(GTK_CONTAINER(ctx->list_box));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    if (!ctx->current_updates || ctx->current_updates->len == 0) {
        gtk_widget_show(ctx->empty_state_box);
        gtk_widget_set_sensitive(ctx->btn_update_all, FALSE);
        gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), "Your system is up to date.");
        if (ctx->badge_cb) ctx->badge_cb(0, ctx->badge_user_data);
        return;
    }

    gtk_widget_hide(ctx->empty_state_box);
    gtk_widget_set_sensitive(ctx->btn_update_all, TRUE);

    for (guint i = 0; i < ctx->current_updates->len; i++) {
        UpdateInfo *info = g_ptr_array_index(ctx->current_updates, i);
        GtkWidget *row = create_update_row(info, ctx);
        gtk_container_add(GTK_CONTAINER(ctx->list_box), row);
    }

    char *status = g_strdup_printf("%u updates available across all sources", ctx->current_updates->len);
    gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), status);
    g_free(status);

    if (ctx->badge_cb) {
        ctx->badge_cb(ctx->current_updates->len, ctx->badge_user_data);
    }
}

/**
 * Background update check callback.
 */
static void on_check_completed(GPtrArray *results, const char *status_msg, gpointer user_data)
{
    UiUpdatesContext *ctx = (UiUpdatesContext *)user_data;
    ctx->is_checking = FALSE;

    gtk_spinner_stop(GTK_SPINNER(ctx->spinner));
    gtk_widget_hide(ctx->spinner);
    gtk_widget_set_sensitive(ctx->btn_check, TRUE);

    char *ts = format_timestamp_now();
    char *lbl_text = g_strdup_printf("Last checked: %s", ts);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_last_checked), lbl_text);
    g_free(lbl_text);
    g_free(ts);

    if (ctx->current_updates) {
        g_ptr_array_unref(ctx->current_updates);
    }
    ctx->current_updates = results;

    ui_updates_render_list(ctx);

    if (status_msg && ctx->toast_cb) {
        ctx->toast_cb(status_msg, ctx->toast_user_data);
    }
}

/**
 * Starts checking for updates.
 */
void ui_updates_trigger_check(UiUpdatesContext *ctx, gboolean refresh_apt_cache)
{
    if (!ctx || ctx->is_checking || ctx->is_updating_batch) return;

    ctx->is_checking = TRUE;
    gtk_widget_set_sensitive(ctx->btn_check, FALSE);
    gtk_widget_set_sensitive(ctx->btn_update_all, FALSE);
    gtk_widget_show(ctx->spinner);
    gtk_spinner_start(GTK_SPINNER(ctx->spinner));

    gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), "Checking repositories for available updates...");

    update_checker_check_async(refresh_apt_cache, on_check_completed, ctx);
}

static void on_btn_check_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    UiUpdatesContext *ctx = (UiUpdatesContext *)user_data;
    /* Explicit click -> refresh APT repository index */
    ui_updates_trigger_check(ctx, TRUE);
}

/* -----------------------------------------------------------------------------
 * Sequential "Update All" Batch Execution
 * ----------------------------------------------------------------------------- */

static void on_batch_single_completed(gboolean success, const char *err_msg, gpointer user_data)
{
    UiUpdatesContext *ctx = (UiUpdatesContext *)user_data;
    UpdateInfo *info = g_ptr_array_index(ctx->current_updates, ctx->batch_current_idx);

    if (success) {
        ctx->batch_success_count++;

        /* Find and destroy the list box row */
        GList *children = gtk_container_get_children(GTK_CONTAINER(ctx->list_box));
        for (GList *l = children; l != NULL; l = l->next) {
            GtkWidget *w = GTK_WIDGET(l->data);
            if (g_object_get_data(G_OBJECT(w), "update-info") == info) {
                gtk_widget_destroy(w);
                break;
            }
        }
        g_list_free(children);

        /* Remove from current_updates array */
        g_ptr_array_remove_index(ctx->current_updates, ctx->batch_current_idx);
        /* Do NOT increment batch_current_idx since array shifted */
    } else {
        char *fail_entry = g_strdup_printf("• %s: %s", info->display_name,
                                           err_msg ? err_msg : "Unknown error");
        ctx->batch_failures = g_list_append(ctx->batch_failures, fail_entry);
        ctx->batch_current_idx++;
    }

    if (ctx->badge_cb) {
        ctx->badge_cb(ctx->current_updates->len, ctx->badge_user_data);
    }

    /* Proceed to next package in the sequence */
    batch_step_next(ctx);
}

static void batch_step_next(UiUpdatesContext *ctx)
{
    if (ctx->batch_current_idx >= ctx->current_updates->len) {
        /* All updates finished! */
        ctx->is_updating_batch = FALSE;
        gtk_widget_hide(ctx->progress_box);
        gtk_widget_set_sensitive(ctx->btn_check, TRUE);
        gtk_widget_set_sensitive(ctx->btn_update_all, ctx->current_updates->len > 0);

        if (ctx->batch_failures != NULL) {
            /* Assemble summary failure dialog */
            GString *summary = g_string_new(NULL);
            g_string_printf(summary, "%u of %u apps updated successfully.\n\nFailures encountered:\n",
                            ctx->batch_success_count, ctx->batch_total);

            for (GList *l = ctx->batch_failures; l != NULL; l = l->next) {
                g_string_append_printf(summary, "%s\n", (char *)l->data);
                g_free(l->data);
            }
            g_list_free(ctx->batch_failures);
            ctx->batch_failures = NULL;

            GtkWidget *dialog = gtk_message_dialog_new(
                ctx->parent_window,
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_CLOSE,
                "Batch Update Completed with Warnings"
            );
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", summary->str);
            g_string_free(summary, TRUE);

            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        } else {
            if (ctx->toast_cb) {
                char *toast = g_strdup_printf("All %u updates installed successfully!", ctx->batch_total);
                ctx->toast_cb(toast, ctx->toast_user_data);
                g_free(toast);
            }
        }

        if (ctx->current_updates->len == 0) {
            gtk_widget_show(ctx->empty_state_box);
            gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), "Your system is up to date.");
        } else {
            char *st = g_strdup_printf("%u updates remaining", ctx->current_updates->len);
            gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), st);
            g_free(st);
        }
        return;
    }

    UpdateInfo *next_info = g_ptr_array_index(ctx->current_updates, ctx->batch_current_idx);
    guint completed = ctx->batch_total - ctx->current_updates->len;
    gdouble fraction = (gdouble)completed / (gdouble)ctx->batch_total;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ctx->progress_bar), fraction);

    char *lbl_msg = g_strdup_printf("Updating %u of %u: %s...",
                                    completed + 1, ctx->batch_total, next_info->display_name);
    gtk_label_set_text(GTK_LABEL(ctx->progress_label), lbl_msg);
    g_free(lbl_msg);

    update_checker_upgrade_async(next_info, on_batch_single_completed, ctx);
}

static void on_btn_update_all_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    UiUpdatesContext *ctx = (UiUpdatesContext *)user_data;

    if (!ctx->current_updates || ctx->current_updates->len == 0 || ctx->is_updating_batch) {
        return;
    }

    ctx->is_updating_batch = TRUE;
    ctx->batch_total = ctx->current_updates->len;
    ctx->batch_current_idx = 0;
    ctx->batch_success_count = 0;
    ctx->batch_failures = NULL;

    gtk_widget_set_sensitive(ctx->btn_check, FALSE);
    gtk_widget_set_sensitive(ctx->btn_update_all, FALSE);

    gtk_widget_show(ctx->progress_box);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ctx->progress_bar), 0.0);

    batch_step_next(ctx);
}

/**
 * Creates and initializes the UI updates panel.
 */
GtkWidget *ui_updates_create_panel(UiUpdatesContext **out_ctx, GtkWindow *parent_window)
{
    UiUpdatesContext *ctx = g_new0(UiUpdatesContext, 1);
    ctx->parent_window = parent_window;
    ctx->current_updates = update_store_new();

    ctx->panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* 1. Header Action Bar */
    GtkWidget *header_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(header_bar), 12);

    /* Check for Updates Button */
    ctx->btn_check = gtk_button_new_with_label("Check for Updates");
    GtkWidget *img_check = gtk_image_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(ctx->btn_check), img_check);
    gtk_button_set_always_show_image(GTK_BUTTON(ctx->btn_check), TRUE);
    g_signal_connect(ctx->btn_check, "clicked", G_CALLBACK(on_btn_check_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(header_bar), ctx->btn_check, FALSE, FALSE, 0);

    /* Spinner */
    ctx->spinner = gtk_spinner_new();
    gtk_widget_set_no_show_all(ctx->spinner, TRUE);
    gtk_box_pack_start(GTK_BOX(header_bar), ctx->spinner, FALSE, FALSE, 2);

    /* Last Checked Timestamp Label */
    ctx->lbl_last_checked = gtk_label_new("Last checked: Never");
    GtkStyleContext *ts_ctx = gtk_widget_get_style_context(ctx->lbl_last_checked);
    gtk_style_context_add_class(ts_ctx, "size-label");
    gtk_box_pack_start(GTK_BOX(header_bar), ctx->lbl_last_checked, FALSE, FALSE, 4);

    /* Update All Button */
    ctx->btn_update_all = gtk_button_new_with_label("Update All");
    GtkWidget *img_upd_all = gtk_image_new_from_icon_name("software-update-available-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(ctx->btn_update_all), img_upd_all);
    gtk_button_set_always_show_image(GTK_BUTTON(ctx->btn_update_all), TRUE);

    GtkStyleContext *upd_ctx = gtk_widget_get_style_context(ctx->btn_update_all);
    gtk_style_context_add_class(upd_ctx, "suggested-action");
    gtk_widget_set_sensitive(ctx->btn_update_all, FALSE);
    g_signal_connect(ctx->btn_update_all, "clicked", G_CALLBACK(on_btn_update_all_clicked), ctx);

    gtk_box_pack_end(GTK_BOX(header_bar), ctx->btn_update_all, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctx->panel), header_bar, FALSE, FALSE, 0);

    /* 2. Sequential Update Progress Banner (Hidden by default) */
    ctx->progress_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(ctx->progress_box), 8);
    gtk_widget_set_no_show_all(ctx->progress_box, TRUE);

    ctx->progress_label = gtk_label_new("Updating applications...");
    gtk_label_set_xalign(GTK_LABEL(ctx->progress_label), 0.0);
    gtk_box_pack_start(GTK_BOX(ctx->progress_box), ctx->progress_label, FALSE, FALSE, 0);

    ctx->progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(ctx->progress_box), ctx->progress_bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctx->panel), ctx->progress_box, FALSE, FALSE, 0);

    /* 3. Main Stack / Scrolled Window Container */
    GtkWidget *list_overlay = gtk_overlay_new();
    gtk_box_pack_start(GTK_BOX(ctx->panel), list_overlay, TRUE, TRUE, 0);

    ctx->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ctx->scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(list_overlay), ctx->scrolled_window);

    ctx->list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(ctx->list_box), GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(ctx->scrolled_window), ctx->list_box);

    /* 4. Empty State: "Your system is up to date! ✓" */
    ctx->empty_state_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_halign(ctx->empty_state_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(ctx->empty_state_box, GTK_ALIGN_CENTER);

    GtkWidget *check_icon = gtk_image_new_from_icon_name("emblem-ok-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(check_icon), 64);
    GtkStyleContext *cicon_ctx = gtk_widget_get_style_context(check_icon);
    gtk_style_context_add_class(cicon_ctx, "badge-flatpak"); /* Green accent */
    gtk_box_pack_start(GTK_BOX(ctx->empty_state_box), check_icon, FALSE, FALSE, 0);

    GtkWidget *lbl_empty_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_empty_title), "<span weight=\"bold\" size=\"large\">Your system is up to date! ✓</span>");
    gtk_box_pack_start(GTK_BOX(ctx->empty_state_box), lbl_empty_title, FALSE, FALSE, 0);

    GtkWidget *lbl_empty_desc = gtk_label_new("All APT packages, Snaps, and Flatpaks are on their latest versions.");
    GtkStyleContext *edesc_ctx = gtk_widget_get_style_context(lbl_empty_desc);
    gtk_style_context_add_class(edesc_ctx, "size-label");
    gtk_box_pack_start(GTK_BOX(ctx->empty_state_box), lbl_empty_desc, FALSE, FALSE, 0);

    gtk_overlay_add_overlay(GTK_OVERLAY(list_overlay), ctx->empty_state_box);

    /* 5. Statusbar */
    GtkWidget *statusbar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(statusbar_box), 8);

    ctx->statusbar_label = gtk_label_new("Ready");
    gtk_label_set_xalign(GTK_LABEL(ctx->statusbar_label), 0.0);
    GtkStyleContext *st_ctx = gtk_widget_get_style_context(ctx->statusbar_label);
    gtk_style_context_add_class(st_ctx, "size-label");
    gtk_box_pack_start(GTK_BOX(statusbar_box), ctx->statusbar_label, TRUE, TRUE, 4);

    gtk_box_pack_start(GTK_BOX(ctx->panel), statusbar_box, FALSE, FALSE, 0);

    if (out_ctx) *out_ctx = ctx;
    return ctx->panel;
}

void ui_updates_set_toast_callback(UiUpdatesContext *ctx, UiUpdatesToastFunc func, gpointer user_data)
{
    if (!ctx) return;
    ctx->toast_cb = func;
    ctx->toast_user_data = user_data;
}

void ui_updates_set_badge_callback(UiUpdatesContext *ctx, UiUpdatesBadgeFunc func, gpointer user_data)
{
    if (!ctx) return;
    ctx->badge_cb = func;
    ctx->badge_user_data = user_data;
}

void ui_updates_free(UiUpdatesContext *ctx)
{
    if (!ctx) return;
    if (ctx->current_updates) {
        g_ptr_array_unref(ctx->current_updates);
        ctx->current_updates = NULL;
    }
    g_free(ctx);
}
