/**
 * AppClip Manager - App Update Checker Implementation
 * Logic to detect and execute updates for APT, Snap, and Flatpak packaging systems.
 *
 * Created by: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "update_checker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib.h>

/* Structure for desktop entry cache item */
typedef struct {
    char *display_name;
    char *icon_name;
} DesktopAppMeta;

static void desktop_meta_free(gpointer data)
{
    DesktopAppMeta *meta = (DesktopAppMeta *)data;
    if (!meta) return;
    g_free(meta->display_name);
    g_free(meta->icon_name);
    g_free(meta);
}

/**
 * Allocate and initialize a new UpdateInfo.
 */
UpdateInfo *update_info_new(const char *package_name,
                            const char *display_name,
                            const char *current_version,
                            const char *new_version,
                            const char *icon_name,
                            const char *download_size,
                            PackageType type)
{
    UpdateInfo *info = g_new0(UpdateInfo, 1);
    info->package_name = g_strdup(package_name);
    info->display_name = g_strdup(display_name && *display_name ? display_name : package_name);
    info->current_version = g_strdup(current_version && *current_version ? current_version : "Installed");
    info->new_version = g_strdup(new_version && *new_version ? new_version : "Latest");
    info->icon_name = g_strdup(icon_name && *icon_name ? icon_name : "system-software-update");
    info->download_size = download_size && *download_size ? g_strdup(download_size) : NULL;
    info->type = type;
    return info;
}

/**
 * Free memory occupied by UpdateInfo.
 */
void update_info_free(UpdateInfo *info)
{
    if (!info) return;
    g_free(info->package_name);
    g_free(info->display_name);
    g_free(info->current_version);
    g_free(info->new_version);
    g_free(info->icon_name);
    g_free(info->download_size);
    g_free(info);
}

/**
 * Create a new GPtrArray pre-configured to free UpdateInfo items.
 */
GPtrArray *update_store_new(void)
{
    return g_ptr_array_new_with_free_func((GDestroyNotify)update_info_free);
}

/**
 * Execute command via fork() and execvp(), capturing stdout and stderr via pipes.
 * Never invokes system() or a shell.
 */
char *update_checker_exec_capture(const char *cmd,
                                  char *const argv[],
                                  int *exit_code,
                                  char **out_stderr)
{
    if (out_stderr) *out_stderr = NULL;
    if (exit_code) *exit_code = -1;

    /* Verify binary exists in PATH */
    char *bin_path = g_find_program_in_path(cmd);
    if (!bin_path) {
        g_warning("Command '%s' not found on system; skipping execution.", cmd);
        if (out_stderr) {
            *out_stderr = g_strdup_printf("Binary '%s' is not installed or not found in system PATH.", cmd);
        }
        return NULL;
    }
    g_free(bin_path);

    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) < 0) {
        g_warning("Failed to create stdout pipe for '%s'", cmd);
        return NULL;
    }

    if (pipe(stderr_pipe) < 0) {
        g_warning("Failed to create stderr pipe for '%s'", cmd);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        g_warning("Failed to fork process for '%s'", cmd);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return NULL;
    }

    if (pid == 0) {
        /* Child process */
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        execvp(cmd, argv);
        _exit(127);
    }

    /* Parent process: close write ends */
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    GString *stdout_str = g_string_sized_new(4096);
    GString *stderr_str = g_string_sized_new(2048);

    char buf[2048];
    ssize_t n;

    /* Read stdout */
    while ((n = read(stdout_pipe[0], buf, sizeof(buf))) > 0) {
        g_string_append_len(stdout_str, buf, n);
    }
    close(stdout_pipe[0]);

    /* Read stderr */
    while ((n = read(stderr_pipe[0], buf, sizeof(buf))) > 0) {
        g_string_append_len(stderr_str, buf, n);
    }
    close(stderr_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (exit_code) *exit_code = code;

    if (out_stderr) {
        *out_stderr = g_string_free(stderr_str, FALSE);
    } else {
        g_string_free(stderr_str, TRUE);
    }

    return g_string_free(stdout_str, FALSE);
}

/**
 * Lightweight helper to load desktop entry metadata for friendly name and icon matching.
 * Scans /usr/share/applications, ~/.local/share/applications, /var/lib/snapd/desktop/applications.
 */
static GHashTable *build_desktop_metadata_cache(void)
{
    GHashTable *cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, desktop_meta_free);

    const char *dirs[] = {
        "/usr/share/applications",
        "/var/lib/snapd/desktop/applications",
        "/var/lib/flatpak/exports/share/applications",
        NULL
    };

    for (int d = 0; dirs[d] != NULL; d++) {
        GDir *dir = g_dir_open(dirs[d], 0, NULL);
        if (!dir) continue;

        const char *filename;
        while ((filename = g_dir_read_name(dir)) != NULL) {
            if (!g_str_has_suffix(filename, ".desktop")) continue;

            char *full_path = g_build_filename(dirs[d], filename, NULL);
            GKeyFile *kf = g_key_file_new();
            if (g_key_file_load_from_file(kf, full_path, G_KEY_FILE_NONE, NULL)) {
                char *name = g_key_file_get_locale_string(kf, "Desktop Entry", "Name", NULL, NULL);
                char *icon = g_key_file_get_string(kf, "Desktop Entry", "Icon", NULL);
                char *exec = g_key_file_get_string(kf, "Desktop Entry", "Exec", NULL);

                if (name || icon) {
                    DesktopAppMeta *meta = g_new0(DesktopAppMeta, 1);
                    meta->display_name = name;
                    meta->icon_name = icon;

                    /* 1. Key by desktop filename without .desktop */
                    char *base_key = g_strndup(filename, strlen(filename) - 8);
                    g_hash_table_insert(cache, g_strdup(base_key), meta);

                    /* 2. Also key by exec command if available */
                    if (exec && *exec) {
                        char **tokens = g_strsplit(exec, " ", 2);
                        if (tokens && tokens[0]) {
                            char *bin_name = g_path_get_basename(tokens[0]);
                            if (!g_hash_table_lookup(cache, bin_name)) {
                                DesktopAppMeta *dup_meta = g_new0(DesktopAppMeta, 1);
                                dup_meta->display_name = g_strdup(meta->display_name);
                                dup_meta->icon_name = g_strdup(meta->icon_name);
                                g_hash_table_insert(cache, bin_name, dup_meta);
                            } else {
                                g_free(bin_name);
                            }
                        }
                        g_strfreev(tokens);
                    }
                    g_free(base_key);
                }
                g_free(exec);
            }
            g_key_file_free(kf);
            g_free(full_path);
        }
        g_dir_close(dir);
    }

    return cache;
}

/**
 * Match friendly name and icon for a package ID from the desktop metadata cache.
 */
static void match_app_metadata(GHashTable *cache,
                               const char *pkg_id,
                               char **out_display_name,
                               char **out_icon_name)
{
    *out_display_name = NULL;
    *out_icon_name = NULL;

    if (!cache || !pkg_id) return;

    DesktopAppMeta *meta = g_hash_table_lookup(cache, pkg_id);
    if (!meta) {
        /* Try case-insensitive or stripped common prefixes */
        char *lower = g_ascii_strdown(pkg_id, -1);
        meta = g_hash_table_lookup(cache, lower);
        g_free(lower);
    }

    if (meta) {
        if (meta->display_name) *out_display_name = g_strdup(meta->display_name);
        if (meta->icon_name) *out_icon_name = g_strdup(meta->icon_name);
    }
}

/**
 * Parses APT output from 'apt list --upgradable 2>/dev/null'.
 * Line format:
 *   firefox/jammy-updates 125.0+build2-0ubuntu0.22.04.1 amd64 [upgradable from: 124.0.2+build1-0ubuntu0.22.04.1]
 */
static void parse_apt_upgradable(const char *raw_output, GHashTable *cache, GPtrArray *store)
{
    if (!raw_output || !*raw_output) return;

    char **lines = g_strsplit(raw_output, "\n", 0);
    for (int i = 0; lines[i] != NULL; i++) {
        char *line = g_strstrip(lines[i]);
        if (!*line || g_str_has_prefix(line, "Listing...")) continue;

        /* Look for package name before '/' */
        char *slash = strchr(line, '/');
        if (!slash) continue;

        char *pkg_name = g_strndup(line, slash - line);

        /* Skip past repo and whitespace to find target version */
        char *space1 = strchr(slash, ' ');
        if (!space1) {
            g_free(pkg_name);
            continue;
        }
        while (*space1 == ' ') space1++;

        char *space2 = strchr(space1, ' ');
        if (!space2) {
            g_free(pkg_name);
            continue;
        }

        char *new_version = g_strndup(space1, space2 - space1);

        /* Look for "[upgradable from: <old_version>]" */
        char *current_version = NULL;
        char *marker = strstr(space2, "[upgradable from: ");
        if (marker) {
            char *old_ver_start = marker + 18;
            char *closing_bracket = strchr(old_ver_start, ']');
            if (closing_bracket) {
                current_version = g_strndup(old_ver_start, closing_bracket - old_ver_start);
            }
        }

        char *disp_name = NULL;
        char *icon_name = NULL;
        match_app_metadata(cache, pkg_name, &disp_name, &icon_name);

        UpdateInfo *info = update_info_new(
            pkg_name,
            disp_name ? disp_name : pkg_name,
            current_version ? current_version : "Installed",
            new_version,
            icon_name ? icon_name : "package-x-generic",
            NULL, /* APT size is calculated dynamically on install */
            PACKAGE_TYPE_APT
        );

        g_ptr_array_add(store, info);

        g_free(pkg_name);
        g_free(new_version);
        g_free(current_version);
        g_free(disp_name);
        g_free(icon_name);
    }
    g_strfreev(lines);
}

/**
 * Parses Snap output from 'snap refresh --list'.
 * Table columns:
 *   Name    Version   Rev   Size   Publisher   Notes
 *   vlc     3.0.19    3721  142MB  videolan✓   -
 */
static void parse_snap_refresh(const char *raw_output, GHashTable *cache, GPtrArray *store)
{
    if (!raw_output || !*raw_output) return;

    char **lines = g_strsplit(raw_output, "\n", 0);
    gboolean header_seen = FALSE;

    for (int i = 0; lines[i] != NULL; i++) {
        char *line = g_strstrip(lines[i]);
        if (!*line) continue;

        if (!header_seen) {
            if (g_str_has_prefix(line, "Name") || g_str_has_prefix(line, "name")) {
                header_seen = TRUE;
                continue;
            }
        }

        /* Tokenize by whitespace */
        char **cols = g_strsplit_set(line, " \t", -1);
        GPtrArray *tokens = g_ptr_array_new();
        for (int c = 0; cols[c] != NULL; c++) {
            if (*cols[c] != '\0') {
                g_ptr_array_add(tokens, cols[c]);
            }
        }

        if (tokens->len >= 3) {
            const char *name = g_ptr_array_index(tokens, 0);
            const char *new_ver = g_ptr_array_index(tokens, 1);
            const char *size_str = (tokens->len >= 4) ? g_ptr_array_index(tokens, 3) : NULL;

            char *disp_name = NULL;
            char *icon_name = NULL;
            match_app_metadata(cache, name, &disp_name, &icon_name);

            UpdateInfo *info = update_info_new(
                name,
                disp_name ? disp_name : name,
                "Installed",
                new_ver,
                icon_name ? icon_name : "snapcraft",
                size_str,
                PACKAGE_TYPE_SNAP
            );
            g_ptr_array_add(store, info);

            g_free(disp_name);
            g_free(icon_name);
        }

        g_ptr_array_free(tokens, TRUE);
        g_strfreev(cols);
    }
    g_strfreev(lines);
}

/**
 * Parses Flatpak output from 'flatpak update --list' or 'flatpak remote-ls --updates'.
 */
static void parse_flatpak_updates(const char *raw_output, GHashTable *cache, GPtrArray *store)
{
    if (!raw_output || !*raw_output) return;

    char **lines = g_strsplit(raw_output, "\n", 0);
    for (int i = 0; lines[i] != NULL; i++) {
        char *line = g_strstrip(lines[i]);
        if (!*line) continue;
        if (g_str_has_prefix(line, "ID") || g_str_has_prefix(line, "Looking for")) continue;

        char **cols = g_strsplit_set(line, "\t", -1);
        int col_count = g_strv_length(cols);
        if (col_count < 2) {
            g_strfreev(cols);
            cols = g_strsplit_set(line, " ", -1);
        }

        GPtrArray *tokens = g_ptr_array_new();
        for (int c = 0; cols[c] != NULL; c++) {
            if (*cols[c] != '\0') {
                g_ptr_array_add(tokens, cols[c]);
            }
        }

        if (tokens->len >= 1) {
            const char *app_id = g_ptr_array_index(tokens, 0);
            const char *version = (tokens->len >= 2) ? g_ptr_array_index(tokens, 1) : "Update available";
            const char *size_val = NULL;

            /* Check if any token contains "MB" or "GB" */
            for (guint t = 2; t < tokens->len; t++) {
                const char *tok = g_ptr_array_index(tokens, t);
                if (strstr(tok, "MB") || strstr(tok, "GB") || strstr(tok, "kB")) {
                    size_val = tok;
                    break;
                }
            }

            char *disp_name = NULL;
            char *icon_name = NULL;
            match_app_metadata(cache, app_id, &disp_name, &icon_name);

            UpdateInfo *info = update_info_new(
                app_id,
                disp_name ? disp_name : app_id,
                "Installed",
                version,
                icon_name ? icon_name : app_id,
                size_val,
                PACKAGE_TYPE_FLATPAK
            );
            g_ptr_array_add(store, info);

            g_free(disp_name);
            g_free(icon_name);
        }

        g_ptr_array_free(tokens, TRUE);
        g_strfreev(cols);
    }
    g_strfreev(lines);
}

/**
 * Data payload for asynchronous update check.
 */
typedef struct {
    gboolean refresh_apt_cache;
    UpdateCheckCallback callback;
    gpointer user_data;
    GPtrArray *results;
    char *status_msg;
} CheckJobData;

/**
 * Main-thread idle completion callback.
 */
static gboolean check_idle_completion(gpointer data)
{
    CheckJobData *job = (CheckJobData *)data;

    if (job->callback) {
        job->callback(job->results, job->status_msg, job->user_data);
    } else {
        if (job->results) g_ptr_array_unref(job->results);
    }

    g_free(job->status_msg);
    g_free(job);
    return G_SOURCE_REMOVE;
}

/**
 * Worker thread for checking updates across APT, Snap, and Flatpak.
 */
static gpointer check_worker_thread(gpointer data)
{
    CheckJobData *job = (CheckJobData *)data;
    GPtrArray *store = update_store_new();
    GHashTable *meta_cache = build_desktop_metadata_cache();

    int apt_count = 0;
    int snap_count = 0;
    int flatpak_count = 0;

    /* -------------------------------------------------------------------------
     * 1. APT Update Check
     * ------------------------------------------------------------------------- */
    char *apt_bin = g_find_program_in_path("apt");
    if (apt_bin) {
        g_free(apt_bin);

        /* Only refresh repository indices if user explicitly consented */
        if (job->refresh_apt_cache) {
            char *pkexec = g_find_program_in_path("pkexec");
            if (pkexec) {
                g_free(pkexec);
                char *const apt_upd_argv[] = { "pkexec", "apt-get", "update", NULL };
                int exit_code = 0;
                char *err = NULL;
                char *out = update_checker_exec_capture("pkexec", apt_upd_argv, &exit_code, &err);
                g_free(out);
                g_free(err);
            }
        }

        /* Check list of upgradable packages (reads local cache, no root needed) */
        char *const apt_list_argv[] = { "apt", "list", "--upgradable", NULL };
        int exit_code = 0;
        char *apt_output = update_checker_exec_capture("apt", apt_list_argv, &exit_code, NULL);
        if (apt_output && exit_code == 0) {
            guint before = store->len;
            parse_apt_upgradable(apt_output, meta_cache, store);
            apt_count = (int)(store->len - before);
        }
        g_free(apt_output);
    } else {
        g_warning("APT package manager not found on this system.");
    }

    /* -------------------------------------------------------------------------
     * 2. Snap Update Check
     * ------------------------------------------------------------------------- */
    char *snap_bin = g_find_program_in_path("snap");
    if (snap_bin) {
        g_free(snap_bin);
        char *const snap_argv[] = { "snap", "refresh", "--list", NULL };
        int exit_code = 0;
        char *snap_output = update_checker_exec_capture("snap", snap_argv, &exit_code, NULL);
        if (snap_output && exit_code == 0) {
            guint before = store->len;
            parse_snap_refresh(snap_output, meta_cache, store);
            snap_count = (int)(store->len - before);
        }
        g_free(snap_output);
    } else {
        g_warning("Snap daemon/utility not found on this system.");
    }

    /* -------------------------------------------------------------------------
     * 3. Flatpak Update Check
     * ------------------------------------------------------------------------- */
    char *flatpak_bin = g_find_program_in_path("flatpak");
    if (flatpak_bin) {
        g_free(flatpak_bin);

        /* Detect Flatpak version */
        char *const ver_argv[] = { "flatpak", "--version", NULL };
        int ver_exit = 0;
        char *ver_out = update_checker_exec_capture("flatpak", ver_argv, &ver_exit, NULL);
        g_free(ver_out);

        /* Try flatpak update --list */
        char *const fp_argv1[] = { "flatpak", "update", "--list", NULL };
        int fp_exit = 0;
        char *fp_output = update_checker_exec_capture("flatpak", fp_argv1, &fp_exit, NULL);

        /* Fallback if --list is unsupported */
        if (!fp_output || fp_exit != 0) {
            g_free(fp_output);
            char *const fp_argv2[] = { "flatpak", "remote-ls", "--updates", NULL };
            fp_output = update_checker_exec_capture("flatpak", fp_argv2, &fp_exit, NULL);
        }

        if (fp_output && fp_exit == 0) {
            guint before = store->len;
            parse_flatpak_updates(fp_output, meta_cache, store);
            flatpak_count = (int)(store->len - before);
        }
        g_free(fp_output);
    } else {
        g_warning("Flatpak utility not found on this system.");
    }

    g_hash_table_destroy(meta_cache);

    job->results = store;
    job->status_msg = g_strdup_printf(
        "Update check complete: %u updates found (APT: %d, Snap: %d, Flatpak: %d)",
        store->len, apt_count, snap_count, flatpak_count
    );

    g_idle_add(check_idle_completion, job);
    return NULL;
}

/**
 * Initiates an asynchronous update scan.
 */
void update_checker_check_async(gboolean refresh_apt_cache,
                                UpdateCheckCallback callback,
                                gpointer user_data)
{
    CheckJobData *job = g_new0(CheckJobData, 1);
    job->refresh_apt_cache = refresh_apt_cache;
    job->callback = callback;
    job->user_data = user_data;

    g_thread_new("update-check-worker", check_worker_thread, job);
}

/**
 * Payload for upgrading a single application.
 */
typedef struct {
    char *package_name;
    PackageType type;
    SingleUpdateCallback callback;
    gpointer user_data;
    gboolean success;
    char *err_msg;
} UpgradeJobData;

static gboolean upgrade_idle_completion(gpointer data)
{
    UpgradeJobData *job = (UpgradeJobData *)data;

    if (job->callback) {
        job->callback(job->success, job->err_msg, job->user_data);
    }

    g_free(job->package_name);
    g_free(job->err_msg);
    g_free(job);
    return G_SOURCE_REMOVE;
}

static gpointer upgrade_worker_thread(gpointer data)
{
    UpgradeJobData *job = (UpgradeJobData *)data;
    char *argv_storage[8];
    int argc = 0;
    const char *binary = NULL;

    if (job->type == PACKAGE_TYPE_APT) {
        /* APT: pkexec apt-get install --only-upgrade -y <package> */
        char *pkexec = g_find_program_in_path("pkexec");
        if (!pkexec) {
            job->success = FALSE;
            job->err_msg = g_strdup("pkexec not installed. Root authorization is required for APT upgrades.");
            g_idle_add(upgrade_idle_completion, job);
            return NULL;
        }
        g_free(pkexec);

        binary = "pkexec";
        argv_storage[argc++] = "pkexec";
        argv_storage[argc++] = "apt-get";
        argv_storage[argc++] = "install";
        argv_storage[argc++] = "--only-upgrade";
        argv_storage[argc++] = "-y";
        argv_storage[argc++] = job->package_name;
        argv_storage[argc++] = NULL;
    } else if (job->type == PACKAGE_TYPE_SNAP) {
        /* Snap: pkexec snap refresh <package> */
        char *pkexec = g_find_program_in_path("pkexec");
        if (!pkexec) {
            job->success = FALSE;
            job->err_msg = g_strdup("pkexec not installed. Root authorization is required for Snap refresh.");
            g_idle_add(upgrade_idle_completion, job);
            return NULL;
        }
        g_free(pkexec);

        binary = "pkexec";
        argv_storage[argc++] = "pkexec";
        argv_storage[argc++] = "snap";
        argv_storage[argc++] = "refresh";
        argv_storage[argc++] = job->package_name;
        argv_storage[argc++] = NULL;
    } else if (job->type == PACKAGE_TYPE_FLATPAK) {
        /* Flatpak: flatpak update -y <app-id> (no pkexec needed) */
        char *flatpak = g_find_program_in_path("flatpak");
        if (!flatpak) {
            job->success = FALSE;
            job->err_msg = g_strdup("Flatpak utility is not installed.");
            g_idle_add(upgrade_idle_completion, job);
            return NULL;
        }
        g_free(flatpak);

        binary = "flatpak";
        argv_storage[argc++] = "flatpak";
        argv_storage[argc++] = "update";
        argv_storage[argc++] = "-y";
        argv_storage[argc++] = job->package_name;
        argv_storage[argc++] = NULL;
    } else {
        job->success = FALSE;
        job->err_msg = g_strdup("Unsupported package type for upgrade.");
        g_idle_add(upgrade_idle_completion, job);
        return NULL;
    }

    int exit_code = 0;
    char *err_captured = NULL;
    char *out_captured = update_checker_exec_capture(binary, argv_storage, &exit_code, &err_captured);

    if (exit_code == 0) {
        job->success = TRUE;
        job->err_msg = NULL;
    } else {
        job->success = FALSE;
        if (exit_code == 126 || exit_code == 127) {
            job->err_msg = g_strdup("Authentication dismissed or permission denied.");
        } else {
            job->err_msg = g_strdup_printf(
                "Upgrade process failed with exit code %d.\n\nError output:\n%s\n%s",
                exit_code,
                err_captured ? err_captured : "",
                out_captured ? out_captured : ""
            );
        }
    }

    g_free(err_captured);
    g_free(out_captured);

    g_idle_add(upgrade_idle_completion, job);
    return NULL;
}

/**
 * Runs upgrade for a single package asynchronously.
 */
void update_checker_upgrade_async(const UpdateInfo *info,
                                  SingleUpdateCallback callback,
                                  gpointer user_data)
{
    if (!info || !info->package_name) {
        if (callback) callback(FALSE, "Invalid package info provided.", user_data);
        return;
    }

    UpgradeJobData *job = g_new0(UpgradeJobData, 1);
    job->package_name = g_strdup(info->package_name);
    job->type = info->type;
    job->callback = callback;
    job->user_data = user_data;

    g_thread_new("upgrade-worker", upgrade_worker_thread, job);
}
