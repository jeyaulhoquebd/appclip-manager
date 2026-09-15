/**
 * AppClip Manager - Autostart Management Module
 * Standard Freedesktop / XDG autostart support across GNOME, KDE, XFCE, etc.
 *
 * Created by: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef AUTOSTART_H
#define AUTOSTART_H

#include <glib.h>

#define AUTOSTART_DESKTOP_FILENAME "appclip-manager.desktop"

/**
 * Returns the absolute path to ~/.config/autostart/appclip-manager.desktop.
 * The caller is responsible for freeing the returned string using g_free().
 */
char *autostart_get_desktop_file_path(void);

/**
 * Resolves the absolute path to the current running executable using /proc/self/exe.
 * Falls back to PATH search or "appclip-manager" if /proc is unavailable.
 * The caller is responsible for freeing the returned string using g_free().
 */
char *autostart_get_executable_path(void);

/**
 * Checks whether autostart is currently enabled by verifying that
 * ~/.config/autostart/appclip-manager.desktop exists and is active.
 *
 * @return TRUE if autostart is active on the filesystem, FALSE otherwise.
 */
gboolean autostart_is_enabled(void);

/**
 * Enables autostart on system startup by creating:
 * ~/.config/autostart/appclip-manager.desktop
 * with the command line: <exec_path> --minimized
 *
 * @param error Location to store error details on failure, or NULL.
 * @return TRUE on success, FALSE if an error occurred.
 */
gboolean autostart_enable(GError **error);

/**
 * Disables autostart on system startup by unlinking:
 * ~/.config/autostart/appclip-manager.desktop
 * ENOENT (file already missing) is treated as a graceful success.
 *
 * @param error Location to store error details on failure, or NULL.
 * @return TRUE on success, FALSE if removal failed due to an OS error.
 */
gboolean autostart_disable(GError **error);

#endif /* AUTOSTART_H */
