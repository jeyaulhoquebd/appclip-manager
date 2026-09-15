/**
 * AppClip Manager - Settings Implementation
 * Handles loading and saving configuration to ~/.config/appclip-manager/settings.conf.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#define GROUP_CLIPBOARD "Clipboard"
#define KEY_MAX_ITEMS "max_history_items"
#define KEY_MAX_DAYS "max_history_days"
#define KEY_MONITOR_TEXT "monitor_text"
#define KEY_MONITOR_IMAGES "monitor_images"

void settings_get_defaults(AppSettings *settings)
{
    if (!settings) return;
    settings->max_history_items = 500;
    settings->max_history_days = 30;
    settings->monitor_text = TRUE;
    settings->monitor_images = TRUE;
}

char *settings_get_file_path(void)
{
    const char *config_dir = g_get_user_config_dir();
    char *app_dir = g_build_filename(config_dir, "appclip-manager", NULL);
    g_mkdir_with_parents(app_dir, 0755);

    char *path = g_build_filename(app_dir, "settings.conf", NULL);
    g_free(app_dir);
    return path;
}

gboolean settings_load(AppSettings *settings)
{
    if (!settings) return FALSE;
    settings_get_defaults(settings);

    char *file_path = settings_get_file_path();
    if (!g_file_test(file_path, G_FILE_TEST_EXISTS)) {
        /* Write initial default settings file */
        gboolean saved = settings_save(settings);
        g_free(file_path);
        return saved;
    }

    GKeyFile *kf = g_key_file_new();
    GError *err = NULL;

    if (!g_key_file_load_from_file(kf, file_path, G_KEY_FILE_NONE, &err)) {
        g_warning("Could not load settings from %s: %s", file_path, err ? err->message : "unknown error");
        if (err) g_error_free(err);
        g_key_file_free(kf);
        g_free(file_path);
        return FALSE;
    }

    if (g_key_file_has_key(kf, GROUP_CLIPBOARD, KEY_MAX_ITEMS, NULL)) {
        int val = g_key_file_get_integer(kf, GROUP_CLIPBOARD, KEY_MAX_ITEMS, NULL);
        if (val > 0) settings->max_history_items = val;
    }

    if (g_key_file_has_key(kf, GROUP_CLIPBOARD, KEY_MAX_DAYS, NULL)) {
        int val = g_key_file_get_integer(kf, GROUP_CLIPBOARD, KEY_MAX_DAYS, NULL);
        if (val > 0) settings->max_history_days = val;
    }

    if (g_key_file_has_key(kf, GROUP_CLIPBOARD, KEY_MONITOR_TEXT, NULL)) {
        settings->monitor_text = g_key_file_get_boolean(kf, GROUP_CLIPBOARD, KEY_MONITOR_TEXT, NULL);
    }

    if (g_key_file_has_key(kf, GROUP_CLIPBOARD, KEY_MONITOR_IMAGES, NULL)) {
        settings->monitor_images = g_key_file_get_boolean(kf, GROUP_CLIPBOARD, KEY_MONITOR_IMAGES, NULL);
    }

    g_key_file_free(kf);
    g_free(file_path);
    return TRUE;
}

gboolean settings_save(const AppSettings *settings)
{
    if (!settings) return FALSE;

    char *file_path = settings_get_file_path();
    GKeyFile *kf = g_key_file_new();

    g_key_file_set_comment(kf, NULL, NULL,
        " AppClip Manager Configuration File\n"
        " Created by: Jeyaul Hoque (https://jeyaulhoque.pages.dev/)\n", NULL);

    g_key_file_set_integer(kf, GROUP_CLIPBOARD, KEY_MAX_ITEMS, settings->max_history_items);
    g_key_file_set_integer(kf, GROUP_CLIPBOARD, KEY_MAX_DAYS, settings->max_history_days);
    g_key_file_set_boolean(kf, GROUP_CLIPBOARD, KEY_MONITOR_TEXT, settings->monitor_text);
    g_key_file_set_boolean(kf, GROUP_CLIPBOARD, KEY_MONITOR_IMAGES, settings->monitor_images);

    GError *err = NULL;
    gboolean success = g_key_file_save_to_file(kf, file_path, &err);
    if (!success) {
        g_warning("Failed to save settings to %s: %s", file_path, err ? err->message : "unknown error");
        if (err) g_error_free(err);
    }

    g_key_file_free(kf);
    g_free(file_path);
    return success;
}
