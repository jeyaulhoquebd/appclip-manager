/**
 * AppClip Manager - Unified Application Shell
 * Combines Part 1 (Installed Applications Manager) and Part 2 (Clipboard History)
 * into a single cohesive desktop application using GTK3 and SQLite3.
 *
 * Created by: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include <gtk/gtk.h>
#include <sqlite3.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* Module headers */
#include "app_model.h"
#include "app_scanner.h"
#include "app_uninstaller.h"
#include "ui_applist.h"

#include "clip_model.h"
#include "db.h"
#include "clipboard_monitor.h"
#include "ui_cliplist.h"
#include "settings.h"

#include "update_checker.h"
#include "ui_updates.h"
#include "autostart.h"
#include "orphan_scanner.h"
#include "ui_cleaner.h"

#define APP_ID "dev.jeyaulhoque.appclip-manager"
#define APP_TITLE "AppClip Manager"
#define APP_VERSION "1.0.0"
#define APP_COMMENTS "Manage your installed apps, clipboard history, and app updates on Ubuntu."
#define AUTHOR_NAME "Jeyaul Hoque"
#define AUTHOR_WEBSITE "https://jeyaulhoque.pages.dev/"

/* Command line options:
 * Global Hotkey Enhancement Note:
 * Modern Linux desktops (Wayland and X11) do not expose a uniform, secure API
 * for individual client applications to grab global desktop shortcuts (e.g. Ctrl+Shift+V)
 * across the entire compositor session. The standard, recommended architecture
 * in GNOME, KDE, and Sway is configuring a desktop keyboard shortcut pointing to:
 *     appclip-manager --show-clipboard
 * This invokes the application and immediately focuses/raises the Clipboard tab.
 */
static gboolean opt_clipboard = FALSE;
static gboolean opt_minimized = FALSE;

static const GOptionEntry CMD_ENTRIES[] = {
    { "clipboard", 'c', 0, G_OPTION_ARG_NONE, &opt_clipboard, "Open directly to the Clipboard History tab", NULL },
    { "show-clipboard", 0, 0, G_OPTION_ARG_NONE, &opt_clipboard, "Open directly to the Clipboard History tab (ideal for Global Hotkey mapping Ctrl+Shift+V)", NULL },
    { "minimized", 'm', 0, G_OPTION_ARG_NONE, &opt_minimized, "Start minimized in the background (used for autostart)", NULL },
    { "tray", 0, 0, G_OPTION_ARG_NONE, &opt_minimized, "Alias for --minimized", NULL },
    { NULL }
};

/* Forward declaration */
typedef struct _UnifiedAppContext UnifiedAppContext;

struct _UnifiedAppContext {
    GtkApplication *gtk_app;
    UiAppContext *app_ctx;
    UiClipContext *clip_ctx;
    UiUpdatesContext *updates_ctx;
    UiCleanerContext *cleaner_ctx;
    GtkWidget *stack;
    GtkWidget *stack_switcher;
    GtkWidget *header_bar;
    GtkWidget *btn_refresh_apps;
    GtkWidget *updates_panel;
    GtkWidget *cleaner_panel;
    GtkWidget *toast_revealer;
    GtkWidget *toast_label;
    guint toast_timeout_id;
    gboolean is_holding_for_minimized;
};

static UnifiedAppContext *global_uctx = NULL;

#ifdef WITH_TRAY_ICON
#include <libappindicator/app-indicator.h>

/**
 * System Tray implementation using AppIndicator:
 * Displays dynamic menu with "Show AppClip Manager", separator,
 * and up to 5 most recent clipboard history entries.
 */
static void on_tray_show_window(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    GtkWindow *window = GTK_WINDOW(user_data);
    gtk_window_present(window);
}

static void on_tray_clip_clicked(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    ClipEntry *clip = (ClipEntry *)user_data;
    if (clip && clip->content) {
        GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        gtk_clipboard_set_text(cb, clip->content, -1);
        gtk_clipboard_store(cb);
    }
}

static AppIndicator *setup_system_tray(GtkWindow *window)
{
    AppIndicator *indicator = app_indicator_new(
        "appclip-manager-tray",
        "edit-paste-symbolic",
        APP_INDICATOR_CATEGORY_APPLICATION_STATUS
    );
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);

    GtkWidget *menu = gtk_menu_new();

    GtkWidget *item_show = gtk_menu_item_new_with_label("Show AppClip Manager");
    g_signal_connect(item_show, "activate", G_CALLBACK(on_tray_show_window), window);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_show);

    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep);

    /* Fetch 5 most recent clips for quick access from tray */
    GList *recent = db_get_recent_clips(5, NULL);
    for (GList *l = recent; l != NULL; l = l->next) {
        ClipEntry *c = (ClipEntry *)l->data;
        const char *label_text = c->preview ? c->preview : "(Image clip)";
        GtkWidget *item = gtk_menu_item_new_with_label(label_text);
        g_signal_connect(item, "activate", G_CALLBACK(on_tray_clip_clicked), c);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }

    GtkWidget *sep2 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep2);

    GtkWidget *item_quit = gtk_menu_item_new_with_label("Quit");
    g_signal_connect_swapped(item_quit, "activate", G_CALLBACK(gtk_widget_destroy), window);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_quit);

    gtk_widget_show_all(menu);
    app_indicator_set_menu(indicator, GTK_MENU(menu));
    return indicator;
}
#endif

/**
 * Custom CSS styling for modern badges, list rows, toast, and action buttons.
 */
static const char *CUSTOM_CSS =
    "list row {"
    "   border-bottom: 1px solid alpha(#94a3b8, 0.25);"
    "   padding: 4px;"
    "   transition: background-color 150ms ease-in-out;"
    "}"
    "list row:hover {"
    "   background-color: alpha(#3b82f6, 0.08);"
    "}"
    "list row:selected {"
    "   background-color: alpha(#3b82f6, 0.16);"
    "}"
    ".pkg-badge {"
    "   font-size: 11px;"
    "   font-weight: bold;"
    "   padding: 2px 8px;"
    "   border-radius: 12px;"
    "}"
    ".badge-apt {"
    "   background-color: #dbeafe;"
    "   color: #1e40af;"
    "   border: 1px solid #bfdbfe;"
    "}"
    ".badge-snap {"
    "   background-color: #ffedd5;"
    "   color: #9a3412;"
    "   border: 1px solid #fed7aa;"
    "}"
    ".badge-flatpak {"
    "   background-color: #dcfce7;"
    "   color: #166534;"
    "   border: 1px solid #bbf7d0;"
    "}"
    ".badge-desktop {"
    "   background-color: #f1f5f9;"
    "   color: #475569;"
    "   border: 1px solid #cbd5e1;"
    "}"
    ".app-title {"
    "   font-weight: bold;"
    "   font-size: 13px;"
    "}"
    ".app-subtitle {"
    "   color: #64748b;"
    "   font-size: 11px;"
    "}"
    ".size-label {"
    "   color: #64748b;"
    "   font-size: 11px;"
    "}"
    ".pinned-badge {"
    "   color: #f59e0b;"
    "   font-size: 14px;"
    "   font-weight: bold;"
    "}"
    ".toast-container {"
    "   background-color: rgba(15, 23, 42, 0.94);"
    "   color: #f8fafc;"
    "   border-radius: 20px;"
    "   padding: 8px 18px;"
    "   margin-bottom: 24px;"
    "   box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.3);"
    "   font-size: 12px;"
    "   font-weight: 500;"
    "}";

static void apply_application_css(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, CUSTOM_CSS, -1, NULL);

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

/**
 * Helper to locate logo.png from either local ./assets/ or installed /usr/share/ path.
 */
static GdkPixbuf *load_app_logo(int width, int height)
{
    const char *paths[] = {
        "assets/icons/appclip-manager-512x512.png",
        "assets/icons/appclip-manager-128x128.png",
        "assets/logo.png",
        "./assets/logo.png",
        "/usr/share/appclip-manager/logo.png",
        "/usr/share/icons/hicolor/128x128/apps/appclip-manager.png",
        "/usr/share/icons/hicolor/512x512/apps/appclip-manager.png",
        NULL
    };

    for (int i = 0; paths[i] != NULL; i++) {
        if (g_file_test(paths[i], G_FILE_TEST_EXISTS)) {
            GError *err = NULL;
            GdkPixbuf *pix = gdk_pixbuf_new_from_file_at_scale(paths[i], width, height, TRUE, &err);
            if (pix) return pix;
            if (err) g_error_free(err);
        }
    }
    return NULL;
}

/**
 * Toast timer auto-hide callback.
 */
static gboolean hide_toast_timer(gpointer user_data)
{
    UnifiedAppContext *uctx = (UnifiedAppContext *)user_data;
    if (uctx && uctx->toast_revealer) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(uctx->toast_revealer), FALSE);
        uctx->toast_timeout_id = 0;
    }
    return G_SOURCE_REMOVE;
}

/**
 * Unified toast notification trigger.
 */
static void unified_show_toast(const char *message, gpointer user_data)
{
    UnifiedAppContext *uctx = (UnifiedAppContext *)user_data;
    if (!uctx || !uctx->toast_revealer || !uctx->toast_label) return;

    gtk_label_set_text(GTK_LABEL(uctx->toast_label), message ? message : "");
    gtk_revealer_set_reveal_child(GTK_REVEALER(uctx->toast_revealer), TRUE);

    if (uctx->toast_timeout_id > 0) {
        g_source_remove(uctx->toast_timeout_id);
    }
    uctx->toast_timeout_id = g_timeout_add(3000, hide_toast_timer, uctx);
}

/**
 * Requirement 3: About Dialog
 */
static void show_about_dialog(GtkWindow *parent)
{
    GtkWidget *dialog = gtk_about_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), APP_TITLE);
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), APP_VERSION);
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), APP_COMMENTS);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), AUTHOR_WEBSITE);
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "jeyaulhoque.pages.dev");

    const char *authors[] = { AUTHOR_NAME, NULL };
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);

    GdkPixbuf *logo = load_app_logo(128, 128);
    if (logo) {
        gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(dialog), logo);
        g_object_unref(logo);
    }

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

typedef struct {
    GtkWindow *dialog;
    AppSettings *settings;
    UnifiedAppContext *uctx;
} AutostartSwitchData;

/**
 * Handles toggling of the "Start on Boot" GtkSwitch.
 * Immediately writes or deletes the autostart .desktop file on the filesystem.
 * If an error occurs, displays a GtkMessageDialog and reverts the switch state.
 */
static gboolean on_autostart_switch_state_set(GtkSwitch *widget, gboolean state, gpointer user_data)
{
    AutostartSwitchData *data = (AutostartSwitchData *)user_data;
    GError *error = NULL;

    if (state) {
        if (!autostart_enable(&error)) {
            GtkWidget *err_dialog = gtk_message_dialog_new(
                data->dialog,
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "Failed to enable autostart:\n\n%s",
                error ? error->message : "Permission denied or filesystem error."
            );
            gtk_window_set_title(GTK_WINDOW(err_dialog), "Autostart Error");
            gtk_dialog_run(GTK_DIALOG(err_dialog));
            gtk_widget_destroy(err_dialog);
            if (error) g_error_free(error);

            /* Revert switch state back to OFF */
            gtk_switch_set_state(widget, FALSE);
            return TRUE;
        }
        data->settings->start_on_boot = TRUE;
        settings_save(data->settings);
    } else {
        if (!autostart_disable(&error)) {
            GtkWidget *err_dialog = gtk_message_dialog_new(
                data->dialog,
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "Failed to disable autostart:\n\n%s",
                error ? error->message : "Permission denied or filesystem error."
            );
            gtk_window_set_title(GTK_WINDOW(err_dialog), "Autostart Error");
            gtk_dialog_run(GTK_DIALOG(err_dialog));
            gtk_widget_destroy(err_dialog);
            if (error) g_error_free(error);

            /* Revert switch state back to ON */
            gtk_switch_set_state(widget, TRUE);
            return TRUE;
        }
        data->settings->start_on_boot = FALSE;
        settings_save(data->settings);
    }

    return FALSE;
}

/**
 * Requirement 4: Settings Dialog
 * Lets user configure system autostart and clipboard retention,
 * persisting to ~/.config/appclip-manager/settings.conf and ~/.config/autostart/.
 */
static void show_settings_dialog(GtkWindow *parent, UnifiedAppContext *uctx)
{
    AppSettings current_settings;
    settings_load(&current_settings);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Settings — AppClip Manager",
        parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL
    );
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 440, 340);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 16);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(content_area), vbox);

    /* --- SECTION 1: System Integration (Start on Boot) --- */
    GtkWidget *lbl_startup_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_startup_header), "<b>System Integration</b>");
    gtk_label_set_xalign(GTK_LABEL(lbl_startup_header), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_startup_header, FALSE, FALSE, 0);

    GtkWidget *autostart_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *lbl_autostart = gtk_label_new("Start AppClip Manager on system startup");
    gtk_label_set_xalign(GTK_LABEL(lbl_autostart), 0.0);
    gtk_box_pack_start(GTK_BOX(autostart_row), lbl_autostart, TRUE, TRUE, 0);

    GtkWidget *switch_autostart = gtk_switch_new();
    gtk_widget_set_valign(switch_autostart, GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(autostart_row), switch_autostart, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), autostart_row, FALSE, FALSE, 0);

    /* Read the CURRENT autostart state from filesystem (source of truth) */
    gboolean is_autostart = autostart_is_enabled();
    gtk_switch_set_active(GTK_SWITCH(switch_autostart), is_autostart);
    gtk_switch_set_state(GTK_SWITCH(switch_autostart), is_autostart);

    AutostartSwitchData *switch_data = g_new0(AutostartSwitchData, 1);
    switch_data->dialog = GTK_WINDOW(dialog);
    switch_data->settings = &current_settings;
    switch_data->uctx = uctx;
    g_signal_connect_data(switch_autostart, "state-set",
                          G_CALLBACK(on_autostart_switch_state_set),
                          switch_data, (GClosureNotify)g_free, 0);

    /* System tray / minimized explanation note */
    GtkWidget *lbl_autostart_note = gtk_label_new(
        "Note: without a system tray, minimized mode runs invisibly. "
        "Launch AppClip Manager again from the app menu to open the window."
    );
    gtk_label_set_line_wrap(GTK_LABEL(lbl_autostart_note), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl_autostart_note), 0.0);
    GtkStyleContext *note_style = gtk_widget_get_style_context(lbl_autostart_note);
    gtk_style_context_add_class(note_style, "size-label");
    gtk_box_pack_start(GTK_BOX(vbox), lbl_autostart_note, FALSE, FALSE, 0);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 4);

    /* --- SECTION 2: Clipboard History Retention --- */
    GtkWidget *lbl_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_header), "<b>Clipboard History Retention</b>");
    gtk_label_set_xalign(GTK_LABEL(lbl_header), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_header, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);

    /* Max History Items */
    GtkWidget *lbl_items = gtk_label_new("Max history items:");
    gtk_label_set_xalign(GTK_LABEL(lbl_items), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lbl_items, 0, 0, 1, 1);

    GtkWidget *spin_items = gtk_spin_button_new_with_range(10.0, 10000.0, 50.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_items), current_settings.max_history_items);
    gtk_widget_set_hexpand(spin_items, TRUE);
    gtk_grid_attach(GTK_GRID(grid), spin_items, 1, 0, 1, 1);

    /* Max History Days */
    GtkWidget *lbl_days = gtk_label_new("Max history retention (days):");
    gtk_label_set_xalign(GTK_LABEL(lbl_days), 0.0);
    gtk_grid_attach(GTK_GRID(grid), lbl_days, 0, 1, 1, 1);

    GtkWidget *spin_days = gtk_spin_button_new_with_range(1.0, 365.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_days), current_settings.max_history_days);
    gtk_widget_set_hexpand(spin_days, TRUE);
    gtk_grid_attach(GTK_GRID(grid), spin_days, 1, 1, 1, 1);

    /* Note */
    GtkWidget *lbl_note = gtk_label_new("Pinned clips are permanently protected from auto-pruning.");
    gtk_label_set_line_wrap(GTK_LABEL(lbl_note), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl_note), 0.0);
    GtkStyleContext *note2_style = gtk_widget_get_style_context(lbl_note);
    gtk_style_context_add_class(note2_style, "size-label");
    gtk_box_pack_start(GTK_BOX(vbox), lbl_note, FALSE, FALSE, 0);

    gtk_widget_show_all(content_area);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        current_settings.max_history_items = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_items));
        current_settings.max_history_days = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_days));
        current_settings.start_on_boot = gtk_switch_get_active(GTK_SWITCH(switch_autostart));
        settings_save(&current_settings);

        /* Prune according to updated settings */
        int pruned = db_prune_old_clips(current_settings.max_history_items, current_settings.max_history_days);
        if (uctx && uctx->clip_ctx) {
            uctx->clip_ctx->settings = current_settings;
            ui_cliplist_reload(uctx->clip_ctx);
        }
        if (pruned > 0) {
            char msg[64];
            g_snprintf(msg, sizeof(msg), "Settings saved. Pruned %d expired items.", pruned);
            unified_show_toast(msg, uctx);
        } else {
            unified_show_toast("Settings saved successfully", uctx);
        }
    }

    gtk_widget_destroy(dialog);
}

static void on_settings_menu_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    UnifiedAppContext *uctx = (UnifiedAppContext *)user_data;
    if (uctx && uctx->app_ctx && uctx->app_ctx->window) {
        show_settings_dialog(GTK_WINDOW(uctx->app_ctx->window), uctx);
    }
}

static void on_about_menu_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    UnifiedAppContext *uctx = (UnifiedAppContext *)user_data;
    if (uctx && uctx->app_ctx && uctx->app_ctx->window) {
        show_about_dialog(GTK_WINDOW(uctx->app_ctx->window));
    }
}

/**
 * Header bar hamburger menu using GtkPopoverMenu.
 */
static GtkWidget *create_hamburger_menu(UnifiedAppContext *uctx, GtkWindow *window)
{
    GtkWidget *menu_button = gtk_menu_button_new();
    GtkWidget *menu_icon = gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(menu_button), menu_icon);
    gtk_widget_set_tooltip_text(menu_button, "App menu");

    GtkWidget *popover = gtk_popover_menu_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);

    /* Settings item */
    GtkWidget *btn_settings = gtk_model_button_new();
    g_object_set(btn_settings, "text", "Settings", NULL);
    g_signal_connect(btn_settings, "clicked", G_CALLBACK(on_settings_menu_clicked), uctx);
    gtk_box_pack_start(GTK_BOX(box), btn_settings, FALSE, FALSE, 0);

    /* About item */
    GtkWidget *btn_about = gtk_model_button_new();
    g_object_set(btn_about, "text", "About AppClip Manager", NULL);
    g_signal_connect(btn_about, "clicked", G_CALLBACK(on_about_menu_clicked), uctx);
    gtk_box_pack_start(GTK_BOX(box), btn_about, FALSE, FALSE, 0);

    /* Separator */
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 4);

    /* Quit item */
    GtkWidget *btn_quit = gtk_model_button_new();
    g_object_set(btn_quit, "text", "Quit", NULL);
    g_signal_connect_swapped(btn_quit, "clicked", G_CALLBACK(gtk_widget_destroy), window);
    gtk_box_pack_start(GTK_BOX(box), btn_quit, FALSE, FALSE, 0);

    gtk_widget_show_all(box);
    gtk_container_add(GTK_CONTAINER(popover), box);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu_button), popover);

    return menu_button;
}

/**
 * Scan completion callback invoked on GTK main thread.
 */
static void on_scan_finished(GPtrArray *apps, const char *status_msg, gpointer user_data)
{
    UiAppContext *ctx = (UiAppContext *)user_data;
    (void)status_msg;

    ui_applist_set_busy_state(ctx, FALSE, NULL);
    ui_applist_populate(ctx, apps);
}

static void trigger_apps_refresh(UiAppContext *ctx)
{
    ui_applist_set_busy_state(ctx, TRUE, "Scanning installed applications (APT, Snap, Flatpak, Desktop)...");
    app_scanner_scan_all_async(on_scan_finished, ctx);
}

static void on_updates_badge_changed(guint count, gpointer user_data)
{
    UnifiedAppContext *uctx = (UnifiedAppContext *)user_data;
    if (!uctx || !uctx->stack || !uctx->updates_panel) return;

    char title_buf[64];
    if (count > 0) {
        g_snprintf(title_buf, sizeof(title_buf), "Updates (%u)", count);
    } else {
        g_strlcpy(title_buf, "Updates", sizeof(title_buf));
    }

    gtk_container_child_set(GTK_CONTAINER(uctx->stack), uctx->updates_panel,
                            "title", title_buf,
                            "icon-name", "software-update-available",
                            NULL);

    /* Send desktop notification via GApplication if updates were detected */
    if (count > 0 && uctx->gtk_app) {
        GNotification *notif = g_notification_new("AppClip Manager - Updates Available");
        char *body = g_strdup_printf("%u application update%s available for your system.",
                                     count, count == 1 ? " is" : "s are");
        g_notification_set_body(notif, body);
        g_free(body);
        GIcon *icon = g_themed_icon_new("software-update-available");
        g_notification_set_icon(notif, icon);
        g_object_unref(icon);
        g_application_send_notification(G_APPLICATION(uctx->gtk_app), "appclip-updates-available", notif);
        g_object_unref(notif);
    }
}

static void on_refresh_button_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    UnifiedAppContext *uctx = (UnifiedAppContext *)user_data;
    const char *visible = gtk_stack_get_visible_child_name(GTK_STACK(uctx->stack));

    if (g_strcmp0(visible, "apps") == 0) {
        trigger_apps_refresh(uctx->app_ctx);
    } else if (g_strcmp0(visible, "updates") == 0 && uctx->updates_ctx) {
        ui_updates_trigger_check(uctx->updates_ctx, TRUE);
    } else if (g_strcmp0(visible, "cleaner") == 0 && uctx->cleaner_ctx) {
        ui_cleaner_trigger_scan(uctx->cleaner_ctx);
    } else if (uctx->clip_ctx) {
        ui_cliplist_reload(uctx->clip_ctx);
        unified_show_toast("Clipboard history refreshed", uctx);
    }
}

/**
 * Switch page handler: updates refresh tooltip dynamically based on visible stack page.
 */
static void on_stack_visible_child_notify(GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    UnifiedAppContext *uctx = (UnifiedAppContext *)user_data;
    const char *visible = gtk_stack_get_visible_child_name(GTK_STACK(gobject));

    if (g_strcmp0(visible, "apps") == 0) {
        gtk_widget_set_tooltip_text(uctx->btn_refresh_apps, "Rescan installed packages (APT, Snap, Flatpak)");
    } else if (g_strcmp0(visible, "updates") == 0) {
        gtk_widget_set_tooltip_text(uctx->btn_refresh_apps, "Check for available application updates");
    } else if (g_strcmp0(visible, "cleaner") == 0) {
        gtk_widget_set_tooltip_text(uctx->btn_refresh_apps, "Rescan leftover files across uninstalled applications");
    } else {
        gtk_widget_set_tooltip_text(uctx->btn_refresh_apps, "Reload saved clipboard entries from SQLite");
    }
}

static void on_pkg_scan_complete_show_cleaner(const char *pkg, const char *name, GList *items, guint64 bytes, gpointer user_data)
{
    UiCleanerContext *cleaner_ctx = (UiCleanerContext *)user_data;
    if (cleaner_ctx) {
        ui_cleaner_show_package_leftovers(cleaner_ctx, pkg, name, items, bytes);
    }
}

/**
 * Public helper called after package uninstallation to switch to the Cleaner tab
 * and present leftover files for user inspection and deletion.
 */
void appclip_show_cleaner_for_package(const char *package_name, const char *display_name)
{
    if (!global_uctx || !global_uctx->cleaner_ctx || !global_uctx->stack) return;

    /* Switch to the Cleaner tab */
    gtk_stack_set_visible_child_name(GTK_STACK(global_uctx->stack), "cleaner");

    /* Scan specifically for this package's leftover files */
    orphan_scanner_scan_for_package_async(
        package_name,
        display_name,
        0,
        "Just Uninstalled",
        on_pkg_scan_complete_show_cleaner,
        global_uctx->cleaner_ctx
    );
}

/**
 * GTK Application activate handler.
 */
static void app_activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    /* ------------------------------------------------------------------------
     * Single-Instance Handling:
     * If an instance is ALREADY running in the background (e.g. autostarted
     * with --minimized), the second launch fires the "activate" signal on this
     * primary instance. Present and raise the existing window.
     * ------------------------------------------------------------------------ */
    if (global_uctx && global_uctx->app_ctx && global_uctx->app_ctx->window) {
        if (global_uctx->is_holding_for_minimized) {
            g_application_release(G_APPLICATION(app));
            global_uctx->is_holding_for_minimized = FALSE;
        }
        gtk_widget_show_all(GTK_WIDGET(global_uctx->app_ctx->window));
        gtk_window_present(GTK_WINDOW(global_uctx->app_ctx->window));
        if (opt_clipboard && global_uctx->stack) {
            gtk_stack_set_visible_child_name(GTK_STACK(global_uctx->stack), "clipboard");
        }
        return;
    }

    apply_application_css();

    /* 1. Initialize SQLite database */
    db_init();

    /* 2. Load settings and perform startup maintenance pruning */
    AppSettings settings;
    settings_load(&settings);
    int pruned = db_prune_old_clips(settings.max_history_items, settings.max_history_days);
    if (pruned > 0) {
        g_message("Startup maintenance: pruned %d expired clipboard entries.", pruned);
    }

    UnifiedAppContext *uctx = g_new0(UnifiedAppContext, 1);
    uctx->gtk_app = app;
    global_uctx = uctx;

    UiAppContext *app_ctx = g_new0(UiAppContext, 1);
    uctx->app_ctx = app_ctx;

    /* 1. Main Application Window: 920x620, resizable */
    app_ctx->window = GTK_APPLICATION_WINDOW(gtk_application_window_new(app));
    gtk_window_set_title(GTK_WINDOW(app_ctx->window), APP_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(app_ctx->window), 920, 620);
    gtk_window_set_resizable(GTK_WINDOW(app_ctx->window), TRUE);
    gtk_window_set_position(GTK_WINDOW(app_ctx->window), GTK_WIN_POS_CENTER);

    /* 2. Header Bar */
    GtkWidget *header_bar = gtk_header_bar_new();
    uctx->header_bar = header_bar;
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(app_ctx->window), header_bar);

    /* Top-Left: App Logo (32x32) + Title + Refresh Button & Spinner */
    GtkWidget *top_left_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GdkPixbuf *logo_pix = load_app_logo(32, 32);
    GtkWidget *logo_img = logo_pix ? gtk_image_new_from_pixbuf(logo_pix) :
                                     gtk_image_new_from_icon_name("system-software-install-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    if (logo_pix) g_object_unref(logo_pix);
    gtk_box_pack_start(GTK_BOX(top_left_box), logo_img, FALSE, FALSE, 0);

    GtkWidget *lbl_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_title), "<b>AppClip Manager</b>");
    gtk_box_pack_start(GTK_BOX(top_left_box), lbl_title, FALSE, FALSE, 0);

    app_ctx->btn_refresh = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    uctx->btn_refresh_apps = app_ctx->btn_refresh;
    gtk_widget_set_tooltip_text(app_ctx->btn_refresh, "Rescan installed packages (APT, Snap, Flatpak)");
    g_signal_connect(app_ctx->btn_refresh, "clicked", G_CALLBACK(on_refresh_button_clicked), uctx);
    gtk_box_pack_start(GTK_BOX(top_left_box), app_ctx->btn_refresh, FALSE, FALSE, 4);

    app_ctx->spinner = GTK_SPINNER(gtk_spinner_new());
    gtk_box_pack_start(GTK_BOX(top_left_box), GTK_WIDGET(app_ctx->spinner), FALSE, FALSE, 0);

    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), top_left_box);

    /* 3. Center GtkStackSwitcher and GtkStack (Apps / Clipboard pages) */
    GtkWidget *stack = gtk_stack_new();
    uctx->stack = stack;
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 200);

    GtkWidget *stack_switcher = gtk_stack_switcher_new();
    uctx->stack_switcher = stack_switcher;
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(stack_switcher), GTK_STACK(stack));
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(header_bar), stack_switcher);

    /* Top-Right: Hamburger Menu Button with GtkPopoverMenu */
    GtkWidget *menu_button = create_hamburger_menu(uctx, GTK_WINDOW(app_ctx->window));
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), menu_button);

    /* 4. Main Container Layout with GtkOverlay for floating toast */
    GtkWidget *overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(app_ctx->window), overlay);

    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(overlay), main_vbox);

    gtk_box_pack_start(GTK_BOX(main_vbox), stack, TRUE, TRUE, 0);

    /* ------------------------------------------------------------------------
     * PAGE 1: "Apps" -> Installed Applications Manager (Part 1)
     * ------------------------------------------------------------------------ */
    GtkWidget *apps_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Search Bar */
    GtkWidget *search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(search_box), 12);

    app_ctx->search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_entry_set_placeholder_text(GTK_ENTRY(app_ctx->search_entry), "Search installed applications by name, package ID, or source...");
    gtk_box_pack_start(GTK_BOX(search_box), GTK_WIDGET(app_ctx->search_entry), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(apps_panel), search_box, FALSE, FALSE, 0);

    /* Progress Bar */
    app_ctx->progress_bar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_widget_set_no_show_all(GTK_WIDGET(app_ctx->progress_bar), TRUE);
    gtk_box_pack_start(GTK_BOX(apps_panel), GTK_WIDGET(app_ctx->progress_bar), FALSE, FALSE, 0);

    /* Scrolled Window + GtkListBox */
    GtkWidget *apps_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(apps_scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(apps_panel), apps_scrolled, TRUE, TRUE, 0);

    app_ctx->list_box = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(app_ctx->list_box, GTK_SELECTION_NONE);
    gtk_list_box_set_filter_func(app_ctx->list_box, ui_applist_filter_func, app_ctx, NULL);
    g_signal_connect_swapped(app_ctx->search_entry, "search-changed",
                             G_CALLBACK(gtk_list_box_invalidate_filter), app_ctx->list_box);

    gtk_container_add(GTK_CONTAINER(apps_scrolled), GTK_WIDGET(app_ctx->list_box));

    /* Add "Apps" titled page to GtkStack */
    gtk_stack_add_titled(GTK_STACK(stack), apps_panel, "apps", "Apps");

    /* ------------------------------------------------------------------------
     * PAGE 2: "Clipboard" -> Clipboard History Manager (Part 2)
     * ------------------------------------------------------------------------ */
    UiClipContext *clip_ctx = NULL;
    GtkWidget *clip_panel = ui_cliplist_create_panel(&clip_ctx, GTK_WINDOW(app_ctx->window));
    uctx->clip_ctx = clip_ctx;
    ui_cliplist_set_toast_callback(clip_ctx, unified_show_toast, uctx);

    /* Add "Clipboard" titled page to GtkStack */
    gtk_stack_add_titled(GTK_STACK(stack), clip_panel, "clipboard", "Clipboard");

    /* ------------------------------------------------------------------------
     * PAGE 3: "Updates" -> App Update Checker (Part 3)
     * ------------------------------------------------------------------------ */
    UiUpdatesContext *updates_ctx = NULL;
    GtkWidget *updates_panel = ui_updates_create_panel(&updates_ctx, GTK_WINDOW(app_ctx->window));
    uctx->updates_ctx = updates_ctx;
    uctx->updates_panel = updates_panel;
    ui_updates_set_toast_callback(updates_ctx, unified_show_toast, uctx);
    ui_updates_set_badge_callback(updates_ctx, on_updates_badge_changed, uctx);

    /* Add "Updates" titled page to GtkStack */
    gtk_stack_add_titled(GTK_STACK(stack), updates_panel, "updates", "Updates");
    gtk_container_child_set(GTK_CONTAINER(stack), updates_panel,
                            "icon-name", "software-update-available",
                            NULL);

    /* ------------------------------------------------------------------------
     * PAGE 4: "Cleaner" -> System Cleaner (Leftover Config & Cache Removal)
     * ------------------------------------------------------------------------ */
    UiCleanerContext *cleaner_ctx = NULL;
    GtkWidget *cleaner_panel = ui_cleaner_create_panel(&cleaner_ctx, GTK_WINDOW(app_ctx->window));
    uctx->cleaner_ctx = cleaner_ctx;
    uctx->cleaner_panel = cleaner_panel;
    ui_cleaner_set_toast_callback(cleaner_ctx, unified_show_toast, uctx);

    /* Add "Cleaner" titled page to GtkStack */
    gtk_stack_add_titled(GTK_STACK(stack), cleaner_panel, "cleaner", "Cleaner");
    gtk_container_child_set(GTK_CONTAINER(stack), cleaner_panel,
                            "icon-name", "edit-clear-all-symbolic",
                            NULL);

    /* Connect stack child notifier */
    g_signal_connect(stack, "notify::visible-child-name", G_CALLBACK(on_stack_visible_child_notify), uctx);

    /* 5. Bottom Status Bar */
    GtkWidget *statusbar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(statusbar_box), 8);

    app_ctx->statusbar_label = gtk_label_new("Ready");
    gtk_label_set_xalign(GTK_LABEL(app_ctx->statusbar_label), 0.0);
    GtkStyleContext *st_ctx = gtk_widget_get_style_context(app_ctx->statusbar_label);
    gtk_style_context_add_class(st_ctx, "size-label");
    gtk_box_pack_start(GTK_BOX(statusbar_box), app_ctx->statusbar_label, TRUE, TRUE, 4);

    GtkWidget *author_label = gtk_label_new("AppClip Manager • by Jeyaul Hoque");
    gtk_label_set_xalign(GTK_LABEL(author_label), 1.0);
    GtkStyleContext *ath_ctx = gtk_widget_get_style_context(author_label);
    gtk_style_context_add_class(ath_ctx, "size-label");
    gtk_box_pack_end(GTK_BOX(statusbar_box), author_label, FALSE, FALSE, 4);

    GtkWidget *sep_bottom = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_vbox), sep_bottom, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), statusbar_box, FALSE, FALSE, 0);

    /* 6. Floating Toast Notification (GtkRevealer inside GtkOverlay) */
    app_ctx->toast_revealer = gtk_revealer_new();
    uctx->toast_revealer = app_ctx->toast_revealer;
    gtk_revealer_set_transition_type(GTK_REVEALER(app_ctx->toast_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_widget_set_halign(app_ctx->toast_revealer, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(app_ctx->toast_revealer, GTK_ALIGN_END);

    GtkWidget *toast_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkStyleContext *tctx = gtk_widget_get_style_context(toast_box);
    gtk_style_context_add_class(tctx, "toast-container");

    GtkWidget *check_icon = gtk_image_new_from_icon_name("emblem-ok-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(toast_box), check_icon, FALSE, FALSE, 0);

    app_ctx->toast_label = gtk_label_new("Operation completed");
    uctx->toast_label = app_ctx->toast_label;
    gtk_box_pack_start(GTK_BOX(toast_box), app_ctx->toast_label, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(app_ctx->toast_revealer), toast_box);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), app_ctx->toast_revealer);

    /* Window visibility:
     * If --minimized, -m, or --tray was passed, run silently in background without showing main window.
     * SQLite DB, clipboard monitoring, and update checks are fully active.
     * If a system tray is compiled in, setup_system_tray() shows the tray icon.
     * If NO system tray is available, the app runs invisibly in background.
     * g_application_hold() keeps the process alive while window remains unmapped.
     */
    if (opt_minimized) {
        g_application_hold(G_APPLICATION(app));
        uctx->is_holding_for_minimized = TRUE;
        g_message("AppClip Manager started in background/minimized mode (--minimized).");
    } else {
        gtk_widget_show_all(GTK_WIDGET(app_ctx->window));
        if (opt_clipboard) {
            gtk_stack_set_visible_child_name(GTK_STACK(stack), "clipboard");
        }
    }

#ifdef WITH_TRAY_ICON
    setup_system_tray(GTK_WINDOW(app_ctx->window));
#endif

    /* Kick off background scanning of installed packages */
    trigger_apps_refresh(app_ctx);

    /* Automatic background update checking if enabled and interval has elapsed */
    if (settings.auto_check_updates_interval_hours > 0) {
        time_t now = time(NULL);
        gint64 elapsed_hours = (settings.last_update_check > 0) ? (now - settings.last_update_check) / 3600 : 9999;
        if (elapsed_hours >= settings.auto_check_updates_interval_hours) {
            settings.last_update_check = (gint64)now;
            settings_save(&settings);
            /* Run silent check in background (Snap and Flatpak, no root password prompt) */
            ui_updates_trigger_check(updates_ctx, FALSE);
        }
    }
}

/**
 * Requirement 5: Shutdown Handler
 * Closes DB connection, disconnects signal handlers, and frees resources.
 */
static void app_shutdown(GtkApplication *app, gpointer user_data)
{
    (void)app;
    (void)user_data;

    if (global_uctx) {
        if (global_uctx->is_holding_for_minimized) {
            g_application_release(G_APPLICATION(global_uctx->gtk_app));
            global_uctx->is_holding_for_minimized = FALSE;
        }
        if (global_uctx->toast_timeout_id > 0) {
            g_source_remove(global_uctx->toast_timeout_id);
            global_uctx->toast_timeout_id = 0;
        }
        if (global_uctx->clip_ctx) {
            ui_cliplist_free(global_uctx->clip_ctx);
            global_uctx->clip_ctx = NULL;
        }
        if (global_uctx->updates_ctx) {
            ui_updates_free(global_uctx->updates_ctx);
            global_uctx->updates_ctx = NULL;
        }
        if (global_uctx->app_ctx) {
            g_free(global_uctx->app_ctx);
            global_uctx->app_ctx = NULL;
        }
        g_free(global_uctx);
        global_uctx = NULL;
    }

    db_close();
    g_message("AppClip Manager shutdown cleanly.");
}

int main(int argc, char *argv[])
{
    /* Parse command-line flags early */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 ||
            strcmp(argv[i], "--minimized") == 0 ||
            strcmp(argv[i], "--tray") == 0) {
            opt_minimized = TRUE;
        } else if (strcmp(argv[i], "-c") == 0 ||
                   strcmp(argv[i], "--clipboard") == 0 ||
                   strcmp(argv[i], "--show-clipboard") == 0) {
            opt_clipboard = TRUE;
        }
    }

    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_application_add_main_option_entries(G_APPLICATION(app), CMD_ENTRIES);

    g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(app_shutdown), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
