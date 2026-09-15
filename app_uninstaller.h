#ifndef APP_UNINSTALLER_H
#define APP_UNINSTALLER_H

#include "app_model.h"
#include <glib.h>

/**
 * AppUninstallCallback:
 * Invoked on the GTK main thread once the asynchronous uninstall command finishes.
 * @success: TRUE if command returned exit code 0; FALSE on error or cancellation.
 * @err_message: Captured error output or user-friendly cancellation notice if failed.
 * @user_data: Custom pointer passed to app_uninstaller_run_async.
 */
typedef void (*AppUninstallCallback)(gboolean success, const char *err_message, gpointer user_data);

/**
 * app_uninstaller_validate_pkg_id:
 * Validates package identifier string against allowed character sets
 * (alphanumerics, dots, dashes, underscores, plus signs) and disallows leading
 * dashes to prevent command-line option injection.
 */
gboolean app_uninstaller_validate_pkg_id(const char *pkg_id);

/**
 * app_uninstaller_run_async:
 * Executes the privilege-escalated or user-level uninstall command asynchronously
 * using fork() + execvp() and captures stderr for feedback.
 *
 * Commands executed:
 *  - APT:     pkexec apt-get remove -y <pkg_id>
 *  - Snap:    pkexec snap remove <pkg_id>
 *  - Flatpak: flatpak uninstall -y <pkg_id>
 *
 * @app: AppInfo containing the target package information.
 * @callback: Completion callback executed on the main UI thread.
 * @user_data: Pointer forwarded to @callback.
 */
void app_uninstaller_run_async(const AppInfo *app, AppUninstallCallback callback, gpointer user_data);

#endif /* APP_UNINSTALLER_H */
