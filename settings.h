/**
 * AppClip Manager - Settings Module
 * INI-style configuration management for clipboard history preferences.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <glib.h>

typedef struct {
    int max_history_items;  /* Max non-pinned entries (default 500) */
    int max_history_days;   /* Retention days for non-pinned entries (default 30) */
    gboolean monitor_text;  /* Whether to monitor text clips (default TRUE) */
    gboolean monitor_images;/* Whether to monitor image clips (default TRUE) */
    int auto_check_updates_interval_hours; /* Auto check interval in hours (default 24, 0 = disabled) */
    gint64 last_update_check;              /* Unix timestamp of last update check */
    gboolean start_on_boot;                /* Whether to launch minimized on system startup (default FALSE) */
} AppSettings;

/**
 * Populates settings with default values.
 */
void settings_get_defaults(AppSettings *settings);

/**
 * Loads settings from ~/.config/appclip-manager/settings.conf.
 * If file does not exist, writes defaults and returns TRUE.
 */
gboolean settings_load(AppSettings *settings);

/**
 * Saves settings to ~/.config/appclip-manager/settings.conf.
 */
gboolean settings_save(const AppSettings *settings);

/**
 * Returns the absolute path to the settings file.
 * Caller must free returned string with g_free().
 */
char *settings_get_file_path(void);

#endif /* SETTINGS_H */
