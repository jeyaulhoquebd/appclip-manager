#include "ui_applist.h"
#include "app_uninstaller.h"
#include <time.h>

/**
 * Data payload passed to uninstall callback.
 */
typedef struct {
    UiAppContext *ctx;
    GtkListBoxRow *row;
    AppInfo *app;
} RowUninstallContext;

/**
 * Timer callback for pulsing the progress bar during active uninstallation.
 */
static gboolean progress_pulse_cb(gpointer user_data)
{
    UiAppContext *ctx = (UiAppContext *)user_data;
    if (ctx && ctx->progress_bar) {
        gtk_progress_bar_pulse(ctx->progress_bar);
        return G_SOURCE_CONTINUE;
    }
    return G_SOURCE_REMOVE;
}

/**
 * Timer callback to auto-hide the floating toast notification after 3 seconds.
 */
static gboolean toast_hide_timer_cb(gpointer user_data)
{
    UiAppContext *ctx = (UiAppContext *)user_data;
    if (ctx && ctx->toast_revealer) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->toast_revealer), FALSE);
    }
    ctx->toast_timer_id = 0;
    return G_SOURCE_REMOVE;
}

/**
 * ui_applist_show_toast:
 * Sets the message, reveals the notification, and arms the 3-second auto-dismiss timer.
 */
void ui_applist_show_toast(UiAppContext *ctx, const char *message)
{
    if (!ctx || !ctx->toast_revealer || !ctx->toast_label) return;

    /* Cancel any active timer */
    if (ctx->toast_timer_id > 0) {
        g_source_remove(ctx->toast_timer_id);
        ctx->toast_timer_id = 0;
    }

    gtk_label_set_text(GTK_LABEL(ctx->toast_label), message ? message : "Operation completed");
    gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->toast_revealer), TRUE);

    ctx->toast_timer_id = g_timeout_add(3000, toast_hide_timer_cb, ctx);
}

/**
 * ui_applist_set_busy_state:
 * Manages spinner, progress bar visibility, and refresh button state.
 */
void ui_applist_set_busy_state(UiAppContext *ctx, gboolean is_busy, const char *action_desc)
{
    if (!ctx) return;

    if (ctx->spinner) {
        if (is_busy) {
            gtk_widget_show(GTK_WIDGET(ctx->spinner));
            gtk_spinner_start(ctx->spinner);
        } else {
            gtk_spinner_stop(ctx->spinner);
            gtk_widget_hide(GTK_WIDGET(ctx->spinner));
        }
    }

    if (ctx->progress_bar) {
        if (is_busy) {
            gtk_widget_show(GTK_WIDGET(ctx->progress_bar));
            if (ctx->pulse_timer_id == 0) {
                ctx->pulse_timer_id = g_timeout_add(100, progress_pulse_cb, ctx);
            }
        } else {
            if (ctx->pulse_timer_id > 0) {
                g_source_remove(ctx->pulse_timer_id);
                ctx->pulse_timer_id = 0;
            }
            gtk_widget_hide(GTK_WIDGET(ctx->progress_bar));
        }
    }

    if (ctx->btn_refresh) {
        gtk_widget_set_sensitive(ctx->btn_refresh, !is_busy);
    }

    if (ctx->statusbar_label && action_desc) {
        gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), action_desc);
    }
}

/**
 * Callback invoked when asynchronous uninstallation concludes.
 */
static void on_uninstall_completed_cb(gboolean success, const char *err_message, gpointer user_data)
{
    RowUninstallContext *rctx = (RowUninstallContext *)user_data;
    UiAppContext *ctx = rctx->ctx;
    GtkListBoxRow *row = rctx->row;
    AppInfo *app = rctx->app;

    /* Stop busy indicator */
    ui_applist_set_busy_state(ctx, FALSE, NULL);

    if (success) {
        /* Remove row from GUI */
        gtk_widget_destroy(GTK_WIDGET(row));

        /* Show 3-second toast */
        ui_applist_show_toast(ctx, "App uninstalled successfully");

        /* Update total count in statusbar */
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_buf[64];
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

        GList *children = gtk_container_get_children(GTK_CONTAINER(ctx->list_box));
        guint remaining_count = g_list_length(children);
        g_list_free(children);

        char *status_text = g_strdup_printf("Total Applications: %u  •  Last updated: %s",
                                            remaining_count, time_buf);
        gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), status_text);
        g_free(status_text);

        /* Update empty state if 0 remaining */
        if (remaining_count == 0 && ctx->empty_state_box) {
            gtk_widget_show(ctx->empty_state_box);
        }

        /* ---------------------------------------------------------------------
         * Post-Uninstall System Cleaner Trigger (Requirement 3):
         * Offer user the option: "Scan for leftover files? [Scan Now] [Skip]"
         * --------------------------------------------------------------------- */
        extern void appclip_show_cleaner_for_package(const char *package_name, const char *display_name);

        GtkWidget *clean_dialog = gtk_message_dialog_new(
            GTK_WINDOW(ctx->window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_NONE,
            "Scan for leftover files?"
        );
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(clean_dialog),
            "Application '%s' has been uninstalled. Would you like to scan and remove any residual configuration, cache, and log files?",
            app->name ? app->name : app->pkg_id
        );
        gtk_dialog_add_button(GTK_DIALOG(clean_dialog), "_Skip", GTK_RESPONSE_REJECT);
        GtkWidget *btn_scan = gtk_dialog_add_button(GTK_DIALOG(clean_dialog), "_Scan Now", GTK_RESPONSE_ACCEPT);
        GtkStyleContext *bstyle = gtk_widget_get_style_context(btn_scan);
        gtk_style_context_add_class(bstyle, "suggested-action");

        gint res = gtk_dialog_run(GTK_DIALOG(clean_dialog));
        gtk_widget_destroy(clean_dialog);

        if (res == GTK_RESPONSE_ACCEPT) {
            appclip_show_cleaner_for_package(app->pkg_id, app->name);
        }
    } else {
        /* Display captured stderr in an error dialog */
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(ctx->window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "Could not uninstall %s",
            app->name ? app->name : app->pkg_id
        );
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(dialog),
            "%s",
            err_message ? err_message : "An unknown error occurred during uninstallation."
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    g_free(rctx);
}

/**
 * Handler for the "Uninstall" button click on a list row.
 */
void ui_applist_on_uninstall_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    GtkListBoxRow *row = GTK_LIST_BOX_ROW(user_data);
    UiAppContext *ctx = (UiAppContext *)g_object_get_data(G_OBJECT(row), "ui-context");
    AppInfo *app = (AppInfo *)g_object_get_data(G_OBJECT(row), "app-info");

    if (!ctx || !app) return;

    /* Create confirmation dialog */
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(ctx->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_NONE,
        "Are you sure you want to uninstall %s?",
        app->name ? app->name : app->pkg_id
    );

    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dialog),
        "Package ID: %s\nPackaging: %s%s%s\n\nThis action cannot be undone.",
        app->pkg_id ? app->pkg_id : "N/A",
        package_type_to_string(app->type),
        app->size ? "\nEstimated Size: " : "",
        app->size ? app->size : ""
    );

    gtk_dialog_add_button(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_CANCEL);
    GtkWidget *btn_confirm = gtk_dialog_add_button(GTK_DIALOG(dialog), "_Uninstall", GTK_RESPONSE_ACCEPT);
    GtkStyleContext *btn_ctx = gtk_widget_get_style_context(btn_confirm);
    gtk_style_context_add_class(btn_ctx, "destructive-action");

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_ACCEPT) {
        /* Indicate busy state */
        char *busy_msg = g_strdup_printf("Uninstalling %s...", app->name ? app->name : app->pkg_id);
        ui_applist_set_busy_state(ctx, TRUE, busy_msg);
        g_free(busy_msg);

        RowUninstallContext *rctx = g_new0(RowUninstallContext, 1);
        rctx->ctx = ctx;
        rctx->row = row;
        rctx->app = app;

        app_uninstaller_run_async(app, on_uninstall_completed_cb, rctx);
    }
}

/**
 * Helper to build an icon widget with proper fallback resolution.
 */
static GtkWidget *create_app_icon_widget(const char *icon_name)
{
    GtkWidget *image = NULL;
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();

    if (icon_name && *icon_name) {
        /* Case 1: Absolute path to image */
        if (icon_name[0] == '/' && g_file_test(icon_name, G_FILE_TEST_EXISTS)) {
            GError *err = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_name, 48, 48, TRUE, &err);
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

    /* Fallback generic executable icon */
    image = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DND);
    gtk_image_set_pixel_size(GTK_IMAGE(image), 44);
    return image;
}

/**
 * ui_applist_create_row:
 * Constructs the styled GtkListBoxRow according to user requirements.
 */
GtkWidget *ui_applist_create_row(AppInfo *app, UiAppContext *ctx)
{
    GtkWidget *row = gtk_list_box_row_new();
    gtk_widget_set_can_focus(row, TRUE);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);

    /* 1. App Icon */
    GtkWidget *icon_widget = create_app_icon_widget(app->icon_name);
    gtk_widget_set_valign(icon_widget, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), icon_widget, FALSE, FALSE, 0);

    /* 2. Text Details Box (Vertical) */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);

    /* Display Name (Bold) */
    GtkWidget *name_label = gtk_label_new(NULL);
    char *escaped_name = g_markup_escape_text(app->name ? app->name : "Unnamed Application", -1);
    char *name_markup = g_strdup_printf("<span font_weight=\"bold\" font_size=\"medium\">%s</span>", escaped_name);
    gtk_label_set_markup(GTK_LABEL(name_label), name_markup);
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);
    g_free(escaped_name);
    g_free(name_markup);
    gtk_box_pack_start(GTK_BOX(vbox), name_label, FALSE, FALSE, 0);

    /* Subtitle row: Badge + Size + Details */
    GtkWidget *sub_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    /* Package Type Badge */
    GtkWidget *badge_label = gtk_label_new(package_type_to_string(app->type));
    GtkStyleContext *bcontext = gtk_widget_get_style_context(badge_label);
    gtk_style_context_add_class(bcontext, "pkg-badge");
    gtk_style_context_add_class(bcontext, package_type_to_badge_class(app->type));
    gtk_box_pack_start(GTK_BOX(sub_box), badge_label, FALSE, FALSE, 0);

    /* Size Info if available */
    if (app->size && *app->size) {
        GtkWidget *size_label = gtk_label_new(app->size);
        GtkStyleContext *scontext = gtk_widget_get_style_context(size_label);
        gtk_style_context_add_class(scontext, "size-label");
        gtk_box_pack_start(GTK_BOX(sub_box), size_label, FALSE, FALSE, 0);
    }

    /* Package ID subtitle (in subtle font) */
    if (app->pkg_id && *app->pkg_id && g_strcmp0(app->pkg_id, app->name) != 0) {
        GtkWidget *pkg_label = gtk_label_new(NULL);
        char *escaped_pkg = g_markup_escape_text(app->pkg_id, -1);
        char *pkg_markup = g_strdup_printf("<span color=\"#64748b\" font_size=\"small\">(%s)</span>", escaped_pkg);
        gtk_label_set_markup(GTK_LABEL(pkg_label), pkg_markup);
        gtk_label_set_ellipsize(GTK_LABEL(pkg_label), PANGO_ELLIPSIZE_END);
        g_free(escaped_pkg);
        g_free(pkg_markup);
        gtk_box_pack_start(GTK_BOX(sub_box), pkg_label, FALSE, FALSE, 0);
    }

    gtk_box_pack_start(GTK_BOX(vbox), sub_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    /* 3. Uninstall Button */
    GtkWidget *btn_uninstall = gtk_button_new_with_label("Uninstall");
    gtk_widget_set_valign(btn_uninstall, GTK_ALIGN_CENTER);
    GtkStyleContext *btn_context = gtk_widget_get_style_context(btn_uninstall);
    gtk_style_context_add_class(btn_context, "destructive-action");
    gtk_style_context_add_class(btn_context, "btn-uninstall");

    /* Disable uninstall for standalone desktop entries without backing package */
    if (app->type == PACKAGE_TYPE_DESKTOP_ENTRY) {
        gtk_widget_set_tooltip_text(btn_uninstall, "Desktop entry launcher");
    } else {
        gtk_widget_set_tooltip_text(btn_uninstall, "Uninstall this application from the system");
    }

    g_signal_connect(btn_uninstall, "clicked", G_CALLBACK(ui_applist_on_uninstall_clicked), row);
    gtk_box_pack_end(GTK_BOX(hbox), btn_uninstall, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(row), hbox);

    /* Attach payload and search keys */
    g_object_set_data(G_OBJECT(row), "ui-context", ctx);
    g_object_set_data(G_OBJECT(row), "app-info", app);

    /* Pre-compute lowercase searchable text string */
    char *raw_search = g_strdup_printf("%s %s %s %s",
                                       app->name ? app->name : "",
                                       app->pkg_id ? app->pkg_id : "",
                                       package_type_to_string(app->type),
                                       app->comment ? app->comment : "");
    char *search_key = g_utf8_strdown(raw_search, -1);
    g_free(raw_search);
    g_object_set_data_full(G_OBJECT(row), "search-key", search_key, g_free);

    gtk_widget_show_all(row);
    return row;
}

/**
 * ui_applist_filter_func:
 * Substring matching for search filtering.
 */
gboolean ui_applist_filter_func(GtkListBoxRow *row, gpointer user_data)
{
    UiAppContext *ctx = (UiAppContext *)user_data;
    if (!ctx || !ctx->search_entry) return TRUE;

    const char *text = gtk_entry_get_text(GTK_ENTRY(ctx->search_entry));
    if (!text || *text == '\0') {
        return TRUE;
    }

    const char *search_key = (const char *)g_object_get_data(G_OBJECT(row), "search-key");
    if (!search_key) return TRUE;

    char *query_down = g_utf8_strdown(text, -1);
    gboolean matches = (strstr(search_key, query_down) != NULL);
    g_free(query_down);

    return matches;
}

/**
 * ui_applist_populate:
 * Populates the list box with rows for all apps in @apps.
 */
void ui_applist_populate(UiAppContext *ctx, GPtrArray *apps)
{
    if (!ctx || !ctx->list_box) return;

    /* Clear existing rows */
    GList *children = gtk_container_get_children(GTK_CONTAINER(ctx->list_box));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    if (ctx->current_store && ctx->current_store != apps) {
        app_store_free(ctx->current_store);
    }
    ctx->current_store = apps;

    if (!apps || apps->len == 0) {
        if (ctx->empty_state_box) {
            gtk_widget_show(ctx->empty_state_box);
        }
        if (ctx->statusbar_label) {
            gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), "No applications found.");
        }
        return;
    }

    if (ctx->empty_state_box) {
        gtk_widget_hide(ctx->empty_state_box);
    }

    /* Populate rows */
    for (guint i = 0; i < apps->len; i++) {
        AppInfo *app = (AppInfo *)g_ptr_array_index(apps, i);
        GtkWidget *row = ui_applist_create_row(app, ctx);
        gtk_list_box_insert(ctx->list_box, row, -1);
    }

    /* Update statusbar */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

    char *status_text = g_strdup_printf("Total Applications: %u  •  Last updated: %s",
                                        apps->len, time_buf);
    if (ctx->statusbar_label) {
        gtk_label_set_text(GTK_LABEL(ctx->statusbar_label), status_text);
    }
    g_free(status_text);
}
