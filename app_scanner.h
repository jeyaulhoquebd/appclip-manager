#ifndef APP_SCANNER_H
#define APP_SCANNER_H

#include "app_model.h"
#include <glib.h>

/**
 * AppScanCallback:
 * Called on the GTK main thread when asynchronous scanning completes.
 * @apps: Pointer array of AppInfo objects (caller takes ownership).
 * @status_msg: Summary string (e.g. "Scan complete: 142 applications found").
 * @user_data: User pointer supplied to app_scanner_scan_all_async.
 */
typedef void (*AppScanCallback)(GPtrArray *apps, const char *status_msg, gpointer user_data);

/**
 * app_scanner_run_cmd_pipe:
 * Safely executes @cmd with argument vector @argv using fork() and execvp(),
 * capturing standard output into a dynamically allocated string.
 * Validates executable existence in PATH before executing; returns NULL if missing.
 * @exit_status: Pointer to store the process exit code, or NULL.
 */
char *app_scanner_run_cmd_pipe(const char *cmd, char *const argv[], int *exit_status);

/**
 * app_scanner_scan_apt:
 * Scans installed Debian/Ubuntu APT packages using dpkg-query.
 */
void app_scanner_scan_apt(GPtrArray *store);

/**
 * app_scanner_scan_snap:
 * Scans installed Snap packages using snap list.
 */
void app_scanner_scan_snap(GPtrArray *store);

/**
 * app_scanner_scan_flatpak:
 * Scans installed Flatpak applications using flatpak list.
 */
void app_scanner_scan_flatpak(GPtrArray *store);

/**
 * app_scanner_scan_desktop_entries:
 * Scans desktop entry files (/usr/share/applications and ~/.local/share/applications)
 * using an internal lightweight INI parser. Enriches existing package records with
 * icons, descriptions, and friendly names, and adds standalone desktop applications.
 */
void app_scanner_scan_desktop_entries(GPtrArray *store);

/**
 * app_scanner_scan_all_sync:
 * Runs a complete scan across all sources synchronously.
 * Returns a new GPtrArray of AppInfo items.
 */
GPtrArray *app_scanner_scan_all_sync(void);

/**
 * app_scanner_scan_all_async:
 * Spawns a background thread to collect data asynchronously without freezing
 * the GTK UI. Invokes @callback on the main thread via g_idle_add() when finished.
 */
void app_scanner_scan_all_async(AppScanCallback callback, gpointer user_data);

#endif /* APP_SCANNER_H */
