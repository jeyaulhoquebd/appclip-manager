/**
 * AppClip Manager - System Cleaner UI Panel Implementation
 * Leftover file inspection, selective deletion, group headers, and safety warnings.
 *
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "ui_cleaner.h"
#include "db.h"
#include "app_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
static void update_delete_button_state(UiCleanerContext *ctx);
static void populate_cleaner_groups(UiCleanerContext *ctx, GList *groups);

static void show_toast(UiCleanerContext *ctx, const char *msg)
{
    if (ctx && ctx->toast_cb) {
        ctx->toast_cb(msg, ctx->toast_user_data);
    }
}

static char *format_time_relative(const char *datetime_str)
{
    if (!datetime_str || *datetime_str == '\0') {
        return g_strdup("Detected in scan");
    }

    /* SQLite datetime format: YYYY-MM-DD HH:MM:SS */
    GDateTime *dt = g_date_time_new_from_iso8601(datetime_str, NULL);
    if (!dt) {
        return g_strdup_printf("Uninstalled: %s", datetime_str);
    }

    GDateTime *now = g_date_time_new_now_local();
    GTimeSpan diff = g_date_time_difference(now, dt);
    g_date_time_unref(dt);
    g_date_time_unref(now);

    gint64 days = diff / G_TIME_SPAN_DAY;
    if (days <= 0) {
        return g_strdup("Uninstalled today");
    } else if (days == 1) {
        return g_strdup("Uninstalled yesterday");
    } else {
        return g_strdup_printf("Uninstalled %lld days ago", (long long)days);
    }
}

/**
 * Preview button handler: opens the path in the default system file manager
 * WITHOUT modifying or deleting any files.
 */
static void on_preview_button_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    OrphanFileItem *item = (OrphanFileItem *)user_data;
    if (!item || !item->path) return;

    char *uri = g_filename_to_uri(item->path, NULL, NULL);
    if (!uri) return;

    GError *err = NULL;
    gboolean ok = g_app_info_launch_default_for_uri(uri, NULL, &err);
    if (!ok) {
        /* If opening the file itself failed (e.g. no associated mime app), try opening parent folder */
        char *parent = g_path_get_dirname(item->path);
        char *parent_uri = g_filename_to_uri(parent, NULL, NULL);
        if (parent_uri) {
            g_clear_error(&err);
            g_app_info_launch_default_for_uri(parent_uri, NULL, &err);
            g_free(parent_uri);
        }
        g_free(parent);
    }

    if (err) {
        g_warning("Could not launch file manager for %s: %s", item->path, err->message);
        g_error_free(err);
    }
    g_free(uri);
}

/**
 * Checkbox toggled handler: updates the "Delete Selected" button label and sensitivity.
 */
static void on_checkbox_toggled(GtkToggleButton *tb, gpointer user_data)
{
    (void)tb;
    UiCleanerContext *ctx = (UiCleanerContext *)user_data;
    update_delete_button_state(ctx);
}

/**
 * Updates the "Delete Selected" button label and sensitivity based on checked boxes.
 */
static void update_delete_button_state(UiCleanerContext *ctx)
{
    if (!ctx || !ctx->btn_delete_selected) return;

    guint selected_count = 0;
    guint64 selected_bytes = 0;
    gboolean has_system = FALSE;

    for (GList *l = ctx->row_entries; l != NULL; l = l->next) {
        CleanerRowEntry *entry = (CleanerRowEntry *)l->data;
        if (entry && entry->check_button && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(entry->check_button))) {
            selected_count++;
            selected_bytes += entry->item->size_bytes;
            if (entry->item->is_system_path) {
                has_system = TRUE;
            }
        }
    }

    char *size_str = orphan_scanner_format_size(selected_bytes);
    char *btn_label = NULL;

    if (selected_count == 0) {
        btn_label = g_strdup("Delete Selected (0 items)");
        gtk_widget_set_sensitive(ctx->btn_delete_selected, FALSE);
    } else {
        btn_label = g_strdup_printf("Delete Selected (%u item%s • %s)%s",
                                    selected_count,
                                    selected_count == 1 ? "" : "s",
                                    size_str,
                                    has_system ? " [Admin Required]" : "");
        gtk_widget_set_sensitive(ctx->btn_delete_selected, TRUE);
    }

    gtk_button_set_label(GTK_BUTTON(ctx->btn_delete_selected), btn_label);
    g_free(btn_label);
    g_free(size_str);
}

/**
 * Creates a group header row in the GtkListBox.
 */
static GtkWidget *create_group_header_row(OrphanAppGroup *group)
{
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);

    /* Group Icon */
    GtkWidget *icon = gtk_image_new_from_icon_name("user-trash-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);

    /* Group Title & Relative Time */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

    char *markup = g_markup_printf_escaped("<b>%s</b> <span size='smaller' color='#94a3b8'>(%s)</span>",
                                           group->display_name ? group->display_name : group->package_name,
                                           group->package_name ? group->package_name : "");
    GtkWidget *lbl_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_title), markup);
    gtk_label_set_xalign(GTK_LABEL(lbl_title), 0.0);
    g_free(markup);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_title, FALSE, FALSE, 0);

    char *rel_time = format_time_relative(group->uninstalled_date);
    GtkWidget *lbl_sub = gtk_label_new(rel_time);
    g_free(rel_time);
    GtkStyleContext *sub_ctx = gtk_widget_get_style_context(lbl_sub);
    gtk_style_context_add_class(sub_ctx, "app-subtitle");
    gtk_label_set_xalign(GTK_LABEL(lbl_sub), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_sub, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), vbox, TRUE, TRUE, 0);

    /* Total Size Badge */
    char *sz_str = orphan_scanner_format_size(group->total_size_bytes);
    GtkWidget *badge = gtk_label_new(sz_str);
    g_free(sz_str);
    GtkStyleContext *bctx = gtk_widget_get_style_context(badge);
    gtk_style_context_add_class(bctx, "pkg-badge");
    gtk_style_context_add_class(bctx, "badge-apt");
    gtk_box_pack_end(GTK_BOX(box), badge, FALSE, FALSE, 4);

    gtk_container_add(GTK_CONTAINER(row), box);
    return row;
}

/**
 * Creates an individual file/directory row with checkbox, path, size, and preview.
 */
static GtkWidget *create_item_row(UiCleanerContext *ctx, OrphanAppGroup *group, OrphanFileItem *item)
{
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);

    /* -------------------------------------------------------------------------
     * Checkbox:
     * Checked by default for user paths (~/.config, ~/.cache, ~/.local).
     * UNCHECKED by default for system paths (/etc, /var/log) requiring admin.
     * ------------------------------------------------------------------------- */
    GtkWidget *check = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), !item->is_system_path);
    g_signal_connect(check, "toggled", G_CALLBACK(on_checkbox_toggled), ctx);
    gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 4);

    /* Path Type / Location Badge */
    const char *badge_text = "DATA";
    const char *badge_class = "badge-flatpak";

    if (item->is_system_path) {
        badge_text = "SYSTEM (ADMIN)";
        badge_class = "badge-snap";
    } else if (strstr(item->path, "/.config")) {
        badge_text = "CONFIG";
        badge_class = "badge-apt";
    } else if (strstr(item->path, "/.cache")) {
        badge_text = "CACHE";
        badge_class = "badge-snap";
    }

    GtkWidget *badge = gtk_label_new(badge_text);
    GtkStyleContext *bctx = gtk_widget_get_style_context(badge);
    gtk_style_context_add_class(bctx, "pkg-badge");
    gtk_style_context_add_class(bctx, badge_class);
    gtk_box_pack_start(GTK_BOX(box), badge, FALSE, FALSE, 0);

    /* Path representation with tooltip */
    GtkWidget *path_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

    GtkWidget *lbl_path = gtk_label_new(item->path);
    gtk_label_set_ellipsize(GTK_LABEL(lbl_path), PANGO_ELLIPSIZE_START);
    gtk_label_set_xalign(GTK_LABEL(lbl_path), 0.0);
    gtk_widget_set_tooltip_text(lbl_path, item->path);
    GtkStyleContext *pctx = gtk_widget_get_style_context(lbl_path);
    gtk_style_context_add_class(pctx, "app-title");
    gtk_box_pack_start(GTK_BOX(path_vbox), lbl_path, FALSE, FALSE, 0);

    /* Formatted Date & Type */
    char date_buf[64] = "Unknown date";
    if (item->last_modified > 0) {
        struct tm *tm_info = localtime(&item->last_modified);
        strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M", tm_info);
    }

    char *meta_str = g_strdup_printf("%s • Modified: %s",
                                     item->type == ORPHAN_TYPE_DIRECTORY ? "Directory" : "File",
                                     date_buf);
    GtkWidget *lbl_meta = gtk_label_new(meta_str);
    g_free(meta_str);
    gtk_label_set_xalign(GTK_LABEL(lbl_meta), 0.0);
    GtkStyleContext *mctx = gtk_widget_get_style_context(lbl_meta);
    gtk_style_context_add_class(mctx, "size-label");
    gtk_box_pack_start(GTK_BOX(path_vbox), lbl_meta, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), path_vbox, TRUE, TRUE, 4);

    /* Size Label */
    char *sz_str = orphan_scanner_format_size(item->size_bytes);
    GtkWidget *lbl_size = gtk_label_new(sz_str);
    g_free(sz_str);
    GtkStyleContext *sctx = gtk_widget_get_style_context(lbl_size);
    gtk_style_context_add_class(sctx, "size-label");
    gtk_box_pack_end(GTK_BOX(box), lbl_size, FALSE, FALSE, 8);

    /* Preview Button: inspect in system file manager */
    GtkWidget *btn_preview = gtk_button_new_from_icon_name("document-open-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(btn_preview, "Inspect item in system file manager");
    g_signal_connect(btn_preview, "clicked", G_CALLBACK(on_preview_button_clicked), item);
    gtk_box_pack_end(GTK_BOX(box), btn_preview, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(row), box);

    /* Track row entry */
    CleanerRowEntry *entry = g_new0(CleanerRowEntry, 1);
    entry->check_button = check;
    entry->item = item;
    entry->parent_group = group;
    ctx->row_entries = g_list_append(ctx->row_entries, entry);

    return row;
}

static void clear_rows(UiCleanerContext *ctx)
{
    if (!ctx) return;

    GList *children = gtk_container_get_children(GTK_CONTAINER(ctx->list_box));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    g_list_free_full(ctx->row_entries, g_free);
    ctx->row_entries = NULL;

    if (ctx->current_groups) {
        orphan_app_group_list_free(ctx->current_groups);
        ctx->current_groups = NULL;
    }
}

static void populate_cleaner_groups(UiCleanerContext *ctx, GList *groups)
{
    clear_rows(ctx);
    ctx->current_groups = groups;

    if (!groups) {
        if (ctx->empty_state_box) gtk_widget_show(ctx->empty_state_box);
        if (ctx->statusbar_label) gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), "No leftover files detected. System is clean!");
        update_delete_button_state(ctx);
        return;
    }

    if (ctx->empty_state_box) gtk_widget_hide(ctx->empty_state_box);

    guint total_items = 0;
    guint64 grand_total = 0;

    for (GList *g = groups; g != NULL; g = g->next) {
        OrphanAppGroup *group = (OrphanAppGroup *)g->data;
        if (!group || !group->items) continue;

        /* Add Header Row */
        GtkWidget *header = create_group_header_row(group);
        gtk_container_add(GTK_CONTAINER(ctx->list_box), header);

        /* Add Item Rows */
        for (GList *it = group->items; it != NULL; it = it->next) {
            OrphanFileItem *item = (OrphanFileItem *)it->data;
            GtkWidget *irow = create_item_row(ctx, group, item);
            gtk_container_add(GTK_CONTAINER(ctx->list_box), irow);
            total_items++;
            grand_total += item->size_bytes;
        }
    }

    gtk_widget_show_all(GTK_WIDGET(ctx->list_box));

    char *sz_str = orphan_scanner_format_size(grand_total);
    char *status = g_strdup_printf("%u residual file%s / folder%s (%s reclaimable)",
                                   total_items, total_items == 1 ? "" : "s",
                                   total_items == 1 ? "" : "s", sz_str);
    if (ctx->statusbar_label) gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), status);
    g_free(status);
    g_free(sz_str);

    update_delete_button_state(ctx);
}

/* =========================================================================
 * Quick Selection Handlers
 * ========================================================================= */

static void on_select_safe_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    UiCleanerContext *ctx = (UiCleanerContext *)user_data;

    for (GList *l = ctx->row_entries; l != NULL; l = l->next) {
        CleanerRowEntry *entry = (CleanerRowEntry *)l->data;
        if (entry && entry->check_button && entry->item) {
            /* Check safe user paths, uncheck system paths */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entry->check_button), !entry->item->is_system_path);
        }
    }
    update_delete_button_state(ctx);
}

static void on_deselect_all_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    UiCleanerContext *ctx = (UiCleanerContext *)user_data;

    for (GList *l = ctx->row_entries; l != NULL; l = l->next) {
        CleanerRowEntry *entry = (CleanerRowEntry *)l->data;
        if (entry && entry->check_button) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entry->check_button), FALSE);
        }
    }
    update_delete_button_state(ctx);
}

/* =========================================================================
 * Final Deletion Handlers with Confirmation & Admin Warnings
 * ========================================================================= */

static void on_delete_selected_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    UiCleanerContext *ctx = (UiCleanerContext *)user_data;
    if (!ctx) return;

    /* Collect checked items */
    GList *selected_entries = NULL;
    gboolean has_system_files = FALSE;
    guint64 total_bytes = 0;

    for (GList *l = ctx->row_entries; l != NULL; l = l->next) {
        CleanerRowEntry *entry = (CleanerRowEntry *)l->data;
        if (entry && entry->check_button && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(entry->check_button))) {
            selected_entries = g_list_append(selected_entries, entry);
            total_bytes += entry->item->size_bytes;
            if (entry->item->is_system_path) {
                has_system_files = TRUE;
            }
        }
    }

    guint count = g_list_length(selected_entries);
    if (count == 0) {
        show_toast(ctx, "No items selected for deletion");
        return;
    }

    /* -------------------------------------------------------------------------
     * SAFETY MECHANISM 6.d: CONFIRMATION DIALOG
     * Shows exact list of paths to be deleted and requires explicit user accept.
     * ------------------------------------------------------------------------- */
    GString *manifest = g_string_new(NULL);
    guint preview_limit = 8;
    guint idx = 0;

    for (GList *l = selected_entries; l != NULL; l = l->next) {
        CleanerRowEntry *entry = (CleanerRowEntry *)l->data;
        if (idx < preview_limit) {
            g_string_append_printf(manifest, " • %s%s\n",
                                   entry->item->path,
                                   entry->item->is_system_path ? " (System / Admin)" : "");
        }
        idx++;
    }
    if (count > preview_limit) {
        g_string_append_printf(manifest, " ... and %u more item%s\n",
                               count - preview_limit,
                               (count - preview_limit == 1) ? "" : "s");
    }

    char *size_str = orphan_scanner_format_size(total_bytes);

    /* -------------------------------------------------------------------------
     * SAFETY MECHANISM 6.e: EXTRA SYSTEM WARNING
     * Strong warning if any /etc or /var/log paths are among selected items.
     * ------------------------------------------------------------------------- */
    GString *msg = g_string_sized_new(512);
    g_string_printf(msg,
        "You are about to permanently delete %u leftover item%s (%s):\n\n%s\n"
        "This operation cannot be undone.",
        count, count == 1 ? "" : "s", size_str, manifest->str);

    if (has_system_files) {
        g_string_append(msg,
            "\n\n⚠️ <b>CRITICAL SYSTEM WARNING:</b>\n"
            "One or more selected items are located in system-wide directories (/etc or /var/log).\n"
            "Removing them requires administrator privileges via PolicyKit (pkexec) and may "
            "affect other user accounts or system configurations. Only proceed if you are certain.");
    }

    g_string_free(manifest, TRUE);
    g_free(size_str);

    GtkWidget *dialog = gtk_message_dialog_new_with_markup(
        ctx->window,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        has_system_files ? GTK_MESSAGE_WARNING : GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE,
        "<big><b>Permanently delete %u leftover item%s?</b></big>",
        count, count == 1 ? "" : "s"
    );
    gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(dialog), "%s", msg->str);
    g_string_free(msg, TRUE);

    gtk_dialog_add_button(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_CANCEL);
    GtkWidget *btn_confirm = gtk_dialog_add_button(GTK_DIALOG(dialog), "_Delete Permanently", GTK_RESPONSE_ACCEPT);
    GtkStyleContext *bstyle = gtk_widget_get_style_context(btn_confirm);
    gtk_style_context_add_class(bstyle, "destructive-action");

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response != GTK_RESPONSE_ACCEPT) {
        g_list_free(selected_entries);
        return;
    }

    /* Execute deletion */
    guint deleted_count = 0;
    guint64 freed_bytes = 0;
    GList *failed_paths = NULL;
    GHashTable *affected_apps = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    for (GList *l = selected_entries; l != NULL; l = l->next) {
        CleanerRowEntry *entry = (CleanerRowEntry *)l->data;
        char *err_msg = NULL;

        gboolean ok = orphan_scanner_delete_path(entry->item->path, entry->item->is_system_path, &err_msg);
        orphan_scanner_log_deletion(entry->item->path, entry->item->size_bytes,
                                    entry->item->is_system_path, ok, err_msg);

        if (ok) {
            deleted_count++;
            freed_bytes += entry->item->size_bytes;
            if (entry->item->matched_pkg && *entry->item->matched_pkg) {
                g_hash_table_add(affected_apps, g_strdup(entry->item->matched_pkg));
            }
        } else {
            failed_paths = g_list_append(failed_paths, g_strdup_printf("%s: %s", entry->item->path,
                                                                       err_msg ? err_msg : "unknown error"));
            g_free(err_msg);
        }
    }

    g_list_free(selected_entries);

    /* Mark database records cleaned for affected apps */
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, affected_apps);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const char *pkg_name = (const char *)key;
        db_mark_uninstalled_app_cleaned(0, pkg_name);
    }
    g_hash_table_destroy(affected_apps);

    /* Results feedback */
    char *freed_str = orphan_scanner_format_size(freed_bytes);
    if (failed_paths == NULL) {
        char *toast_msg = g_strdup_printf("Successfully cleaned %u item%s (Freed %s)",
                                          deleted_count, deleted_count == 1 ? "" : "s", freed_str);
        show_toast(ctx, toast_msg);
        g_free(toast_msg);
    } else {
        GtkWidget *err_dialog = gtk_message_dialog_new(
            ctx->window,
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_CLOSE,
            "Cleaned %u items (%s freed), but %u item%s could not be removed:",
            deleted_count, freed_str, g_list_length(failed_paths),
            g_list_length(failed_paths) == 1 ? "" : "s"
        );
        GString *err_text = g_string_new(NULL);
        for (GList *fl = failed_paths; fl != NULL; fl = fl->next) {
            g_string_append_printf(err_text, " • %s\n", (char *)fl->data);
        }
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(err_dialog), "%s", err_text->str);
        g_string_free(err_text, TRUE);
        g_list_free_full(failed_paths, g_free);

        gtk_dialog_run(GTK_DIALOG(err_dialog));
        gtk_widget_destroy(err_dialog);
    }
    g_free(freed_str);

    /* Rescan to update view */
    ui_cleaner_trigger_scan(ctx);
}

/* =========================================================================
 * Asynchronous Full Scan Trigger & Callback
 * ========================================================================= */

static void on_full_scan_completed(GList *groups, guint64 total_bytes, const char *status_msg, gpointer user_data)
{
    UiCleanerContext *ctx = (UiCleanerContext *)user_data;
    if (!ctx) return;

    ctx->is_busy = FALSE;
    if (ctx->spinner) gtk_spinner_stop(ctx->spinner);
    if (ctx->progress_bar) gtk_widget_hide(GTK_WIDGET(ctx->progress_bar));
    if (ctx->btn_scan) gtk_widget_set_sensitive(ctx->btn_scan, TRUE);

    populate_cleaner_groups(ctx, groups);

    (void)total_bytes;
    (void)status_msg;
}

void ui_cleaner_trigger_scan(UiCleanerContext *ctx)
{
    if (!ctx || ctx->is_busy) return;

    ctx->is_busy = TRUE;
    if (ctx->spinner) gtk_spinner_start(ctx->spinner);
    if (ctx->progress_bar) gtk_widget_show(GTK_WIDGET(ctx->progress_bar));
    if (ctx->btn_scan) gtk_widget_set_sensitive(ctx->btn_scan, FALSE);
    if (ctx->statusbar_label) gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), "Scanning cache, config, and system directories for leftovers...");

    /* Load uncleaned uninstalled apps from SQLite database */
    GList *uncleaned = db_get_uncleaned_uninstalled_apps();

    /* Gather installed apps from scanner for deep cache cross-referencing */
    GPtrArray *installed = app_scanner_scan_all_sync();

    orphan_scanner_scan_all_async(uncleaned, installed, on_full_scan_completed, ctx);

    uninstalled_app_record_list_free(uncleaned);
    if (installed) g_ptr_array_unref(installed);
}

void ui_cleaner_show_package_leftovers(UiCleanerContext *ctx,
                                      const char *package_name,
                                      const char *display_name,
                                      GList *items,
                                      guint64 total_bytes)
{
    if (!ctx || !package_name || !items) return;

    OrphanAppGroup *group = orphan_app_group_new(package_name, display_name, 0, "Just Uninstalled");
    group->items = items;
    group->total_size_bytes = total_bytes;

    GList *groups = g_list_append(NULL, group);
    populate_cleaner_groups(ctx, groups);

    char *sz_str = orphan_scanner_format_size(total_bytes);
    char *status = g_strdup_printf("Reviewing leftover files for %s (%s total)",
                                   display_name ? display_name : package_name, sz_str);
    if (ctx->statusbar_label) gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), status);
    g_free(status);
    g_free(sz_str);
}

void ui_cleaner_reload(UiCleanerContext *ctx)
{
    ui_cleaner_trigger_scan(ctx);
}

/* =========================================================================
 * Panel Construction
 * ========================================================================= */

static void on_scan_button_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    UiCleanerContext *ctx = (UiCleanerContext *)user_data;
    ui_cleaner_trigger_scan(ctx);
}

GtkWidget *ui_cleaner_create_panel(UiCleanerContext **out_ctx, GtkWindow *parent_window)
{
    UiCleanerContext *ctx = g_new0(UiCleanerContext, 1);
    ctx->window = parent_window;
    *out_ctx = ctx;

    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    ctx->panel = panel;

    /* 1. Header Toolbar */
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(header_box), 12);

    /* Left Info */
    GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *lbl_heading = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_heading), "<b>System Cleaner</b> — Leftover Cache & Config Removal");
    gtk_label_set_xalign(GTK_LABEL(lbl_heading), 0.0);
    gtk_box_pack_start(GTK_BOX(info_box), lbl_heading, FALSE, FALSE, 0);

    GtkWidget *lbl_desc = gtk_label_new("Inspect and safely purge leftover configuration, cache, and state files from uninstalled apps.");
    gtk_label_set_xalign(GTK_LABEL(lbl_desc), 0.0);
    GtkStyleContext *dctx = gtk_widget_get_style_context(lbl_desc);
    gtk_style_context_add_class(dctx, "app-subtitle");
    gtk_box_pack_start(GTK_BOX(info_box), lbl_desc, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(header_box), info_box, TRUE, TRUE, 0);

    /* Action Buttons */
    GtkWidget *actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    ctx->btn_select_safe = gtk_button_new_with_label("Select Safe");
    gtk_widget_set_tooltip_text(ctx->btn_select_safe, "Select user config, cache, and data files (excludes system admin files)");
    g_signal_connect(ctx->btn_select_safe, "clicked", G_CALLBACK(on_select_safe_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(actions_box), ctx->btn_select_safe, FALSE, FALSE, 0);

    ctx->btn_deselect_all = gtk_button_new_with_label("Deselect All");
    g_signal_connect(ctx->btn_deselect_all, "clicked", G_CALLBACK(on_deselect_all_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(actions_box), ctx->btn_deselect_all, FALSE, FALSE, 0);

    ctx->btn_scan = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_label(GTK_BUTTON(ctx->btn_scan), " Rescan Leftovers");
    gtk_button_set_always_show_image(GTK_BUTTON(ctx->btn_scan), TRUE);
    gtk_widget_set_tooltip_text(ctx->btn_scan, "Scan for residual files across uninstalled applications and deep cache");
    g_signal_connect(ctx->btn_scan, "clicked", G_CALLBACK(on_scan_button_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(actions_box), ctx->btn_scan, FALSE, FALSE, 0);

    ctx->spinner = GTK_SPINNER(gtk_spinner_new());
    gtk_box_pack_start(GTK_BOX(actions_box), GTK_WIDGET(ctx->spinner), FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(header_box), actions_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), header_box, FALSE, FALSE, 0);

    /* Progress Bar */
    ctx->progress_bar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(ctx->progress_bar));
    gtk_widget_set_no_show_all(GTK_WIDGET(ctx->progress_bar), TRUE);
    gtk_box_pack_start(GTK_BOX(panel), GTK_WIDGET(ctx->progress_bar), FALSE, FALSE, 0);

    /* 2. Main Scrolled Window with GtkListBox */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    ctx->scrolled_window = scrolled;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *overlay = gtk_overlay_new();
    ctx->list_box = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(ctx->list_box, GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(ctx->list_box));
    gtk_container_add(GTK_CONTAINER(overlay), scrolled);

    /* Empty state placeholder */
    GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    ctx->empty_state_box = empty_box;
    gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(empty_box, GTK_ALIGN_CENTER);

    GtkWidget *empty_icon = gtk_image_new_from_icon_name("emblem-default-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(empty_icon), 64);
    gtk_box_pack_start(GTK_BOX(empty_box), empty_icon, FALSE, FALSE, 0);

    GtkWidget *lbl_empty_title = gtk_label_new("No Leftover Files Found");
    GtkStyleContext *et_ctx = gtk_widget_get_style_context(lbl_empty_title);
    gtk_style_context_add_class(et_ctx, "app-title");
    gtk_box_pack_start(GTK_BOX(empty_box), lbl_empty_title, FALSE, FALSE, 0);

    GtkWidget *lbl_empty_desc = gtk_label_new("Your system configuration, cache, and state directories are clean.");
    GtkStyleContext *ed_ctx = gtk_widget_get_style_context(lbl_empty_desc);
    gtk_style_context_add_class(ed_ctx, "size-label");
    gtk_box_pack_start(GTK_BOX(empty_box), lbl_empty_desc, FALSE, FALSE, 0);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), empty_box);
    gtk_box_pack_start(GTK_BOX(panel), overlay, TRUE, TRUE, 0);

    /* 3. Bottom Action Bar */
    GtkWidget *bottom_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(bottom_bar), 10);

    ctx->statusbar_label = gtk_label_new("Ready to scan");
    gtk_label_set_xalign(GTK_LABEL(ctx->statusbar_label), 0.0);
    GtkStyleContext *st_ctx = gtk_widget_get_style_context(ctx->statusbar_label);
    gtk_style_context_add_class(st_ctx, "size-label");
    gtk_box_pack_start(GTK_BOX(bottom_bar), ctx->statusbar_label, TRUE, TRUE, 0);

    /* Prominent "Delete Selected" Button */
    ctx->btn_delete_selected = gtk_button_new_with_label("Delete Selected (0 items)");
    GtkStyleContext *del_ctx = gtk_widget_get_style_context(ctx->btn_delete_selected);
    gtk_style_context_add_class(del_ctx, "destructive-action");
    gtk_widget_set_sensitive(ctx->btn_delete_selected, FALSE);
    g_signal_connect(ctx->btn_delete_selected, "clicked", G_CALLBACK(on_delete_selected_clicked), ctx);
    gtk_box_pack_end(GTK_BOX(bottom_bar), ctx->btn_delete_selected, FALSE, FALSE, 0);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(panel), sep, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), bottom_bar, FALSE, FALSE, 0);

    return panel;
}

void ui_cleaner_set_toast_callback(UiCleanerContext *ctx, CleanerToastCallback cb, gpointer user_data)
{
    if (!ctx) return;
    ctx->toast_cb = cb;
    ctx->toast_user_data = user_data;
}
