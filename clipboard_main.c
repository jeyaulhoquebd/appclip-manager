/**
 * AppClip Manager - Clipboard History Module (Standalone Executable)
 * 
 * Created by: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 * 
 * ============================================================================
 * NOTEBOOK INTEGRATION INSTRUCTIONS:
 * 
 * To integrate this clipboard module as a tab inside a unified GtkNotebook:
 * 
 *   1. Include headers in your main window controller:
 *          #include "ui_cliplist.h"
 *          #include "db.h"
 *          #include "settings.h"
 * 
 *   2. Initialize the SQLite database and settings on application startup:
 *          db_init();
 *          AppSettings settings;
 *          settings_load(&settings);
 *          db_prune_old_clips(settings.max_history_items, settings.max_history_days);
 * 
 *   3. Create the GtkNotebook in your window:
 *          GtkWidget *notebook = gtk_notebook_new();
 * 
 *   4. Create the Clipboard panel and add it as Tab 2:
 *          UiClipContext *clip_ctx = NULL;
 *          GtkWidget *clip_panel = ui_cliplist_create_panel(&clip_ctx, GTK_WINDOW(main_window));
 * 
 *          GtkWidget *tab_label = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
 *          GtkWidget *tab_icon = gtk_image_new_from_icon_name("edit-paste-symbolic", GTK_ICON_SIZE_MENU);
 *          GtkWidget *tab_title = gtk_label_new("Clipboard History");
 *          gtk_box_pack_start(GTK_BOX(tab_label), tab_icon, FALSE, FALSE, 0);
 *          gtk_box_pack_start(GTK_BOX(tab_label), tab_title, FALSE, FALSE, 0);
 *          gtk_widget_show_all(tab_label);
 * 
 *          gtk_notebook_append_page(GTK_NOTEBOOK(notebook), clip_panel, tab_label);
 * 
 *   5. When closing the application, clean up resources:
 *          ui_cliplist_free(clip_ctx);
 *          db_close();
 * ============================================================================
 */

#include "ui_cliplist.h"
#include "db.h"
#include "settings.h"
#include <gtk/gtk.h>

#define APP_ID "dev.jeyaulhoque.appclipmanager.clipboard"
#define APP_TITLE "AppClip Manager — Clipboard History"
#define APP_VERSION "1.0.0"
#define AUTHOR_NAME "Jeyaul Hoque"
#define AUTHOR_WEBSITE "https://jeyaulhoque.pages.dev/"

static void show_about_dialog(GtkWindow *parent)
{
    const char *logo_locations[] = {
        "/usr/share/appclip-manager/logo.png",
        "./assets/logo.png",
        "assets/logo.png",
        NULL
    };

    GdkPixbuf *logo_pixbuf = NULL;
    for (int i = 0; logo_locations[i]; i++) {
        if (g_file_test(logo_locations[i], G_FILE_TEST_EXISTS)) {
            GError *err = NULL;
            logo_pixbuf = gdk_pixbuf_new_from_file_at_scale(logo_locations[i], 128, 128, TRUE, &err);
            if (logo_pixbuf) break;
            if (err) g_error_free(err);
        }
    }

    const char *authors[] = { AUTHOR_NAME, NULL };

    gtk_show_about_dialog(
        parent,
        "program-name", "AppClip Manager (Clipboard Module)",
        "version", APP_VERSION,
        "comments", "Clipboard History module with X11/Wayland monitoring,\n"
                    "SQLite3 persistence, image previews, deduplication, and export.",
        "website", AUTHOR_WEBSITE,
        "website-label", "Jeyaul Hoque — Official Website",
        "authors", authors,
        "logo", logo_pixbuf,
        "copyright", "© 2026 Jeyaul Hoque. All rights reserved.",
        "license-type", GTK_LICENSE_GPL_3_0,
        NULL
    );

    if (logo_pixbuf) g_object_unref(logo_pixbuf);
}

static void on_about_menu_item_clicked(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    GtkWindow *window = GTK_WINDOW(user_data);
    show_about_dialog(window);
}

#ifdef WITH_TRAY_ICON
#include <libappindicator/app-indicator.h>

/**
 * System Tray implementation using AppIndicator:
 * Displays dynamic menu with "Show Clipboard History", separator,
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
        "appclip-clipboard-tray",
        "edit-paste-symbolic",
        APP_INDICATOR_CATEGORY_APPLICATION_STATUS
    );
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);

    GtkWidget *menu = gtk_menu_new();

    GtkWidget *item_show = gtk_menu_item_new_with_label("Show Clipboard History");
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
#else
/**
 * TODO: Optional libappindicator3-dev Tray Icon Enhancement
 * To enable the system tray icon showing the 5 most recent clips:
 * 1. Install libappindicator3-dev: sudo apt install libappindicator3-dev
 * 2. Recompile with tray flag: make WITH_TRAY=1
 */
#endif

static void activate_standalone(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    /* Initialize database layer */
    if (!db_init()) {
        g_printerr("Failed to initialize SQLite database.\n");
        return;
    }

    /* Load settings and perform startup pruning */
    AppSettings settings;
    settings_load(&settings);
    int pruned = db_prune_old_clips(settings.max_history_items, settings.max_history_days);
    if (pruned > 0) {
        g_message("Startup maintenance: pruned %d old clips.", pruned);
    }

    /* 1. Main Window: 850x600 */
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), APP_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(window), 850, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    /* 2. HeaderBar */
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "AppClip Manager");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Clipboard History");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    /* Menu button */
    GtkWidget *menu_button = gtk_menu_button_new();
    GtkWidget *menu_icon = gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(menu_button), menu_icon);

    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item_about = gtk_menu_item_new_with_label("About AppClip Manager");
    g_signal_connect(item_about, "activate", G_CALLBACK(on_about_menu_item_clicked), window);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_about);

    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep);

    GtkWidget *item_quit = gtk_menu_item_new_with_label("Quit");
    g_signal_connect_swapped(item_quit, "activate", G_CALLBACK(gtk_widget_destroy), window);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_quit);

    gtk_widget_show_all(menu);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(menu_button), menu);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), menu_button);

    /* 3. Create Clipboard History UI Panel */
    UiClipContext *clip_ctx = NULL;
    GtkWidget *panel = ui_cliplist_create_panel(&clip_ctx, GTK_WINDOW(window));
    gtk_container_add(GTK_CONTAINER(window), panel);

    gtk_widget_show_all(window);

#ifdef WITH_TRAY_ICON
    setup_system_tray(GTK_WINDOW(window));
#endif
}

int main(int argc, char *argv[])
{
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate_standalone), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    db_close();
    return status;
}
