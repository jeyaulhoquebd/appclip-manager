/**
 * AppClip Manager - App Update Checker Header
 * Logic to detect available updates for APT, Snap, and Flatpak packaging ecosystems.
 *
 * Created by: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <glib.h>
#include "app_model.h"

/**
 * UpdateInfo:
 * Data structure representing an available application or package update.
 */
typedef struct {
    char *package_name;     /* Package/App identifier (e.g. "code", "firefox", "org.videolan.VLC") */
    char *display_name;     /* Friendly application name from .desktop matching (e.g. "Firefox Web Browser") */
    char *current_version;  /* Currently installed version (e.g. "124.0.1") */
    char *new_version;      /* Upgrade target version (e.g. "125.0.2") */
    char *icon_name;        /* Icon name or absolute path from .desktop */
    char *download_size;    /* Download size if available (e.g. "12.4 MB") or NULL */
    PackageType type;       /* PACKAGE_TYPE_APT, PACKAGE_TYPE_SNAP, PACKAGE_TYPE_FLATPAK */
} UpdateInfo;

/**
 * UpdateCheckCallback:
 * Called on the GTK main loop when an update check completes.
 * @updates: GPtrArray of UpdateInfo items (caller takes ownership).
 * @status_msg: Human-readable summary message.
 * @user_data: User data pointer passed to update_checker_check_async.
 */
typedef void (*UpdateCheckCallback)(GPtrArray *updates, const char *status_msg, gpointer user_data);

/**
 * SingleUpdateCallback:
 * Called on the GTK main loop when a package installation/refresh finishes.
 * @success: TRUE if upgrade succeeded with exit code 0.
 * @err_msg: Captured stderr or error description on failure (NULL on success).
 * @user_data: User pointer passed to update_checker_upgrade_async.
 */
typedef void (*SingleUpdateCallback)(gboolean success, const char *err_msg, gpointer user_data);

/**
 * Allocate and initialize a new UpdateInfo instance.
 */
UpdateInfo *update_info_new(const char *package_name,
                            const char *display_name,
                            const char *current_version,
                            const char *new_version,
                            const char *icon_name,
                            const char *download_size,
                            PackageType type);

/**
 * Free memory held by an UpdateInfo structure.
 */
void update_info_free(UpdateInfo *info);

/**
 * Create a new GPtrArray pre-configured to free UpdateInfo items automatically.
 */
GPtrArray *update_store_new(void);

/**
 * Executes a program safely via fork() and execvp(), capturing stdout and stderr via pipes.
 * Never invokes system() or a shell.
 *
 * @cmd: Binary to execute (checked in PATH).
 * @argv: Argument vector (NULL-terminated).
 * @exit_code: Pointer to store the process exit code, or NULL.
 * @out_stderr: Optional pointer to receive newly-allocated captured stderr string, or NULL.
 *
 * Returns: Newly allocated string with captured stdout (free with g_free), or NULL.
 */
char *update_checker_exec_capture(const char *cmd,
                                  char *const argv[],
                                  int *exit_code,
                                  char **out_stderr);

/**
 * Scans for available updates across APT, Snap, and Flatpak.
 * Runs asynchronously on a background thread created via g_thread_new().
 *
 * @refresh_apt_cache: If TRUE (user clicked "Check for Updates"), runs
 *                     'pkexec apt-get update' first to refresh indices.
 *                     If FALSE (background check), checks without root escalation.
 * @callback: Invoked on the GTK main thread with results.
 * @user_data: Context pointer.
 */
void update_checker_check_async(gboolean refresh_apt_cache,
                                UpdateCheckCallback callback,
                                gpointer user_data);

/**
 * Upgrades a single package asynchronously using fork() + execvp().
 * Runs on a background thread and invokes @callback on completion in the main thread.
 *
 * Commands executed:
 *   APT:     pkexec apt-get install --only-upgrade -y <package>
 *   Snap:    pkexec snap refresh <package>
 *   Flatpak: flatpak update -y <app-id>
 */
void update_checker_upgrade_async(const UpdateInfo *info,
                                  SingleUpdateCallback callback,
                                  gpointer user_data);

#endif /* UPDATE_CHECKER_H */
