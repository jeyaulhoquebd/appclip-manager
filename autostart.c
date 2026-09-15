/**
 * AppClip Manager - Autostart Implementation
 * Implements XDG Autostart specification by generating/deleting
 * ~/.config/autostart/appclip-manager.desktop with full binary path and --minimized flag.
 *
 * Created by: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "autostart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <glib.h>

/**
 * Constructs the standard XDG autostart path:
 * ~/.config/autostart/appclip-manager.desktop
 */
char *autostart_get_desktop_file_path(void)
{
    const char *config_dir = g_get_user_config_dir();
    return g_build_filename(config_dir, "autostart", AUTOSTART_DESKTOP_FILENAME, NULL);
}

/**
 * Resolves the dynamic absolute path of the current running binary.
 * Reads /proc/self/exe to handle arbitrary build directories (e.g. ./appclip-manager),
 * package installations in /usr/local/bin/appclip-manager, or custom prefix paths.
 */
char *autostart_get_executable_path(void)
{
    char buf[PATH_MAX];
    memset(buf, 0, sizeof(buf));

    /* 1. Primary: Linux /proc/self/exe symlink resolution */
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return g_strdup(buf);
    }

    /* 2. Fallback: Lookup in system PATH */
    char *path_bin = g_find_program_in_path("appclip-manager");
    if (path_bin) {
        return path_bin;
    }

    /* 3. Final default: standard binary name */
    return g_strdup("appclip-manager");
}

/**
 * Checks if autostart is enabled.
 * Filesystem is the single source of truth to properly account for manual edits
 * or external removals done by GNOME / KDE / XFCE "Startup Applications" settings.
 */
gboolean autostart_is_enabled(void)
{
    char *desktop_path = autostart_get_desktop_file_path();
    if (!desktop_path) return FALSE;

    if (!g_file_test(desktop_path, G_FILE_TEST_EXISTS)) {
        g_free(desktop_path);
        return FALSE;
    }

    /* Verify that the .desktop file is not explicitly disabled */
    GKeyFile *kf = g_key_file_new();
    gboolean is_active = TRUE;

    if (g_key_file_load_from_file(kf, desktop_path, G_KEY_FILE_NONE, NULL)) {
        /* Check XDG 'Hidden' key */
        if (g_key_file_has_key(kf, "Desktop Entry", "Hidden", NULL)) {
            if (g_key_file_get_boolean(kf, "Desktop Entry", "Hidden", NULL)) {
                is_active = FALSE;
            }
        }
        /* Check GNOME autostart enabled key */
        if (g_key_file_has_key(kf, "Desktop Entry", "X-GNOME-Autostart-enabled", NULL)) {
            if (!g_key_file_get_boolean(kf, "Desktop Entry", "X-GNOME-Autostart-enabled", NULL)) {
                is_active = FALSE;
            }
        }
    }

    g_key_file_free(kf);
    g_free(desktop_path);
    return is_active;
}

/**
 * Creates ~/.config/autostart/appclip-manager.desktop with the --minimized flag.
 */
gboolean autostart_enable(GError **error)
{
    const char *config_dir = g_get_user_config_dir();
    char *autostart_dir = g_build_filename(config_dir, "autostart", NULL);

    /* 1. Ensure directory ~/.config/autostart exists */
    if (g_mkdir_with_parents(autostart_dir, 0755) != 0) {
        int errsv = errno;
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errsv),
                    "Failed to create autostart directory '%s': %s",
                    autostart_dir, g_strerror(errsv));
        g_free(autostart_dir);
        return FALSE;
    }
    g_free(autostart_dir);

    /* 2. Resolve executable path and autostart target path */
    char *exe_path = autostart_get_executable_path();
    char *desktop_path = autostart_get_desktop_file_path();

    /* 3. Generate freedesktop standard autostart file content */
    char *content = g_strdup_printf(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=AppClip Manager\n"
        "Comment=Clipboard history monitor and app manager\n"
        "Exec=%s --minimized\n"
        "Icon=appclip-manager\n"
        "Terminal=false\n"
        "X-GNOME-Autostart-enabled=true\n"
        "Hidden=false\n",
        exe_path
    );

    /* 4. Write content to disk atomically */
    GError *write_err = NULL;
    gboolean ok = g_file_set_contents(desktop_path, content, -1, &write_err);

    g_free(content);
    g_free(exe_path);

    if (!ok) {
        g_propagate_error(error, write_err);
        g_free(desktop_path);
        return FALSE;
    }

    /* 5. Ensure file is readable and executable */
    chmod(desktop_path, 0755);
    g_free(desktop_path);
    return TRUE;
}

/**
 * Disables autostart by deleting ~/.config/autostart/appclip-manager.desktop.
 * Gracefully ignores ENOENT (file not found).
 */
gboolean autostart_disable(GError **error)
{
    char *desktop_path = autostart_get_desktop_file_path();
    if (!desktop_path) return TRUE;

    if (unlink(desktop_path) != 0) {
        int errsv = errno;
        if (errsv != ENOENT) {
            /* File exists but could not be removed (e.g. permissions or read-only mount) */
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errsv),
                        "Failed to remove autostart file '%s': %s",
                        desktop_path, g_strerror(errsv));
            g_free(desktop_path);
            return FALSE;
        }
    }

    g_free(desktop_path);
    return TRUE;
}
