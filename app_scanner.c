#include "app_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib.h>

/**
 * Format size from Kilobytes into a readable string (MB/GB).
 */
static char *format_kb_size(long long kb)
{
    if (kb <= 0) {
        return NULL;
    }
    if (kb >= 1048576) {
        return g_strdup_printf("%.2f GB", (double)kb / 1048576.0);
    } else if (kb >= 1024) {
        return g_strdup_printf("%.1f MB", (double)kb / 1024.0);
    } else {
        return g_strdup_printf("%lld KB", kb);
    }
}

/**
 * app_scanner_run_cmd_pipe:
 * Safely runs @cmd with argument vector @argv via fork() + execvp() and captures stdout.
 * Standard error is redirected to /dev/null to suppress noise.
 * Returns newly allocated string with stdout, or NULL on error or missing binary.
 */
char *app_scanner_run_cmd_pipe(const char *cmd, char *const argv[], int *exit_status)
{
    /* Silently check if the binary exists in PATH to prevent crashing */
    char *bin_path = g_find_program_in_path(cmd);
    if (!bin_path) {
        g_warning("Command '%s' not found on system; skipping this source.", cmd);
        if (exit_status) *exit_status = -1;
        return NULL;
    }
    g_free(bin_path);

    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
        g_warning("Failed to create pipe for '%s'", cmd);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        g_warning("Failed to fork process for '%s'", cmd);
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return NULL;
    }

    if (pid == 0) {
        /* Child process */
        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) {
            _exit(126);
        }
        close(pipe_fds[1]);

        /* Suppress stderr */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        execvp(cmd, argv);
        _exit(127);
    }

    /* Parent process */
    close(pipe_fds[1]);

    GString *output = g_string_sized_new(4096);
    char buffer[4096];
    ssize_t bytes_read;

    while ((bytes_read = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {
        g_string_append_len(output, buffer, bytes_read);
    }
    close(pipe_fds[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (exit_status) {
        *exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    return g_string_free(output, FALSE);
}

/**
 * app_scanner_scan_apt:
 * Runs dpkg-query and adds installed packages to the store.
 */
void app_scanner_scan_apt(GPtrArray *store)
{
    char *const argv[] = {
        "dpkg-query",
        "-W",
        "-f=${Package}\t${Installed-Size}\t${Status}\n",
        NULL
    };

    int status = 0;
    char *output = app_scanner_run_cmd_pipe("dpkg-query", argv, &status);
    if (!output || status != 0) {
        g_free(output);
        return;
    }

    char **lines = g_strsplit(output, "\n", -1);
    g_free(output);

    for (int i = 0; lines[i] != NULL; i++) {
        char *line = lines[i];
        if (!*line) continue;

        char **cols = g_strsplit(line, "\t", 3);
        if (!cols[0] || !cols[1] || !cols[2]) {
            g_strfreev(cols);
            continue;
        }

        const char *pkg_name = cols[0];
        long long size_kb = g_ascii_strtoll(cols[1], NULL, 10);
        const char *pkg_status = cols[2];

        /* Only include fully installed packages */
        if (strstr(pkg_status, "install ok installed") != NULL) {
            char *formatted_size = format_kb_size(size_kb);

            /* Capitalize or use friendly name if available */
            AppInfo *app = app_info_new(pkg_name,
                                        pkg_name,
                                        "application-x-executable",
                                        formatted_size,
                                        PACKAGE_TYPE_APT,
                                        NULL,
                                        pkg_name,
                                        NULL);
            g_free(formatted_size);
            g_ptr_array_add(store, app);
        }

        g_strfreev(cols);
    }

    g_strfreev(lines);
}

/**
 * app_scanner_scan_snap:
 * Runs snap list and appends found packages.
 */
void app_scanner_scan_snap(GPtrArray *store)
{
    char *const argv[] = {
        "snap",
        "list",
        NULL
    };

    int status = 0;
    char *output = app_scanner_run_cmd_pipe("snap", argv, &status);
    if (!output || status != 0) {
        g_free(output);
        return;
    }

    char **lines = g_strsplit(output, "\n", -1);
    g_free(output);

    /* Skip header line (i = 0) */
    for (int i = 1; lines[i] != NULL; i++) {
        char *line = g_strstrip(lines[i]);
        if (!*line) continue;

        /* Split whitespace tokens */
        char **tokens = g_strsplit_set(line, " \t", -1);
        int tok_idx = 0;
        char *name = NULL;
        char *version = NULL;

        for (int t = 0; tokens[t] != NULL; t++) {
            if (*tokens[t] != '\0') {
                if (tok_idx == 0) name = tokens[t];
                else if (tok_idx == 1) version = tokens[t];
                tok_idx++;
            }
        }

        if (name) {
            AppInfo *existing = app_store_find_by_pkg_id(store, name);
            if (!existing) {
                AppInfo *app = app_info_new(name,
                                            name,
                                            "application-x-executable",
                                            NULL,
                                            PACKAGE_TYPE_SNAP,
                                            NULL,
                                            name,
                                            version);
                g_ptr_array_add(store, app);
            }
        }

        g_strfreev(tokens);
    }

    g_strfreev(lines);
}

/**
 * app_scanner_scan_flatpak:
 * Runs flatpak list --app --columns=application,name,size and appends found packages.
 */
void app_scanner_scan_flatpak(GPtrArray *store)
{
    char *const argv[] = {
        "flatpak",
        "list",
        "--app",
        "--columns=application,name,size",
        NULL
    };

    int status = 0;
    char *output = app_scanner_run_cmd_pipe("flatpak", argv, &status);
    if (!output || status != 0) {
        g_free(output);
        return;
    }

    char **lines = g_strsplit(output, "\n", -1);
    g_free(output);

    for (int i = 0; lines[i] != NULL; i++) {
        char *line = lines[i];
        if (!*line) continue;

        char **cols = g_strsplit(line, "\t", 3);
        if (!cols[0]) {
            g_strfreev(cols);
            continue;
        }

        const char *app_id = cols[0];
        const char *friendly_name = (cols[1] && *cols[1]) ? cols[1] : app_id;
        const char *size = (cols[1] && cols[2] && *cols[2]) ? cols[2] : NULL;

        AppInfo *existing = app_store_find_by_pkg_id(store, app_id);
        if (!existing) {
            AppInfo *app = app_info_new(friendly_name,
                                        app_id,
                                        "application-x-executable",
                                        size,
                                        PACKAGE_TYPE_FLATPAK,
                                        NULL,
                                        app_id,
                                        NULL);
            g_ptr_array_add(store, app);
        }

        g_strfreev(cols);
    }

    g_strfreev(lines);
}

/**
 * Clean an Exec= value by removing argument format specifiers (%u, %f, etc.)
 * and extracting the base executable command name.
 */
static char *clean_exec_command(const char *raw_exec, char **out_bin_name)
{
    if (!raw_exec) {
        if (out_bin_name) *out_bin_name = NULL;
        return NULL;
    }

    /* Strip parameter codes */
    GString *cleaned = g_string_new(NULL);
    const char *p = raw_exec;

    while (*p) {
        if (*p == '%' && *(p + 1) != '\0') {
            /* Skip %u, %U, %f, %F, etc. */
            p += 2;
            continue;
        }
        g_string_append_c(cleaned, *p);
        p++;
    }

    char *cmd_str = g_strstrip(g_string_free(cleaned, FALSE));

    /* Extract the first word / executable binary name */
    if (out_bin_name) {
        char **parts = g_strsplit(cmd_str, " ", 2);
        if (parts[0]) {
            char *base = g_path_get_basename(parts[0]);
            *out_bin_name = base;
        } else {
            *out_bin_name = NULL;
        }
        g_strfreev(parts);
    }

    return cmd_str;
}

/**
 * Parse a single .desktop file using a custom INI parser.
 */
static void parse_desktop_file(const char *filepath, GPtrArray *store)
{
    char *content = NULL;
    gsize length = 0;

    if (!g_file_get_contents(filepath, &content, &length, NULL) || !content) {
        return;
    }

    char **lines = g_strsplit(content, "\n", -1);
    g_free(content);

    gboolean in_desktop_entry = FALSE;
    char *name = NULL;
    char *icon = NULL;
    char *exec = NULL;
    char *comment = NULL;
    gboolean no_display = FALSE;
    gboolean hidden = FALSE;

    for (int i = 0; lines[i] != NULL; i++) {
        char *line = g_strstrip(lines[i]);
        if (!*line || *line == '#') continue;

        if (*line == '[') {
            if (g_strcmp0(line, "[Desktop Entry]") == 0) {
                in_desktop_entry = TRUE;
            } else {
                /* New section started; stop parsing main entry */
                if (in_desktop_entry) break;
            }
            continue;
        }

        if (!in_desktop_entry) continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = g_strstrip(line);
        char *val = g_strstrip(eq + 1);

        if (g_strcmp0(key, "Name") == 0 && !name) {
            name = g_strdup(val);
        } else if (g_strcmp0(key, "Icon") == 0 && !icon) {
            icon = g_strdup(val);
        } else if (g_strcmp0(key, "Exec") == 0 && !exec) {
            exec = g_strdup(val);
        } else if (g_strcmp0(key, "Comment") == 0 && !comment) {
            comment = g_strdup(val);
        } else if (g_strcmp0(key, "NoDisplay") == 0) {
            if (g_ascii_strcasecmp(val, "true") == 0) no_display = TRUE;
        } else if (g_strcmp0(key, "Hidden") == 0) {
            if (g_ascii_strcasecmp(val, "true") == 0) hidden = TRUE;
        }
    }

    g_strfreev(lines);

    if (no_display || hidden || !name) {
        g_free(name);
        g_free(icon);
        g_free(exec);
        g_free(comment);
        return;
    }

    char *bin_name = NULL;
    char *clean_exec = clean_exec_command(exec, &bin_name);
    g_free(exec);

    /* Get desktop file base ID (e.g. "firefox.desktop" -> "firefox") */
    char *file_basename = g_path_get_basename(filepath);
    char *desktop_id = g_strdup(file_basename);
    if (g_str_has_suffix(desktop_id, ".desktop")) {
        desktop_id[strlen(desktop_id) - 8] = '\0';
    }
    g_free(file_basename);

    /* Try to enrich an existing package match first */
    AppInfo *matched = NULL;
    if (bin_name) {
        matched = app_store_find_by_exec(store, bin_name);
    }
    if (!matched && desktop_id) {
        matched = app_store_find_by_pkg_id(store, desktop_id);
    }
    if (!matched && name) {
        matched = app_store_find_by_name(store, name);
    }

    if (matched) {
        /* Enrich the existing item */
        if (name && (!matched->name || g_strcmp0(matched->name, matched->pkg_id) == 0)) {
            g_free(matched->name);
            matched->name = g_strdup(name);
        }
        if (icon && (!matched->icon_name || g_strcmp0(matched->icon_name, "application-x-executable") == 0)) {
            g_free(matched->icon_name);
            matched->icon_name = g_strdup(icon);
        }
        if (comment && !matched->comment) {
            matched->comment = g_strdup(comment);
        }
        if (bin_name && !matched->exec_cmd) {
            matched->exec_cmd = g_strdup(bin_name);
        }
    } else {
        /* Check if we already have this app by friendly name */
        AppInfo *duplicate = app_store_find_by_name(store, name);
        if (!duplicate) {
            AppInfo *app = app_info_new(name,
                                        desktop_id ? desktop_id : (bin_name ? bin_name : name),
                                        icon ? icon : "application-x-executable",
                                        NULL,
                                        PACKAGE_TYPE_DESKTOP_ENTRY,
                                        comment,
                                        bin_name ? bin_name : clean_exec,
                                        NULL);
            g_ptr_array_add(store, app);
        }
    }

    g_free(name);
    g_free(icon);
    g_free(clean_exec);
    g_free(bin_name);
    g_free(desktop_id);
    g_free(comment);
}

/**
 * Scan a single directory for .desktop files.
 */
static void scan_directory_desktop_files(const char *dir_path, GPtrArray *store)
{
    if (!g_file_test(dir_path, G_FILE_TEST_IS_DIR)) {
        return;
    }

    GDir *dir = g_dir_open(dir_path, 0, NULL);
    if (!dir) return;

    const char *entry = NULL;
    while ((entry = g_dir_read_name(dir)) != NULL) {
        if (g_str_has_suffix(entry, ".desktop")) {
            char *full_path = g_build_filename(dir_path, entry, NULL);
            parse_desktop_file(full_path, store);
            g_free(full_path);
        }
    }

    g_dir_close(dir);
}

/**
 * app_scanner_scan_desktop_entries:
 * Scans standard system and user desktop file directories.
 */
void app_scanner_scan_desktop_entries(GPtrArray *store)
{
    /* 1. /usr/share/applications */
    scan_directory_desktop_files("/usr/share/applications", store);

    /* 2. ~/.local/share/applications */
    char *user_apps = g_build_filename(g_get_user_data_dir(), "applications", NULL);
    scan_directory_desktop_files(user_apps, store);
    g_free(user_apps);

    /* 3. /usr/local/share/applications */
    scan_directory_desktop_files("/usr/local/share/applications", store);

    /* 4. Snap applications if present */
    scan_directory_desktop_files("/var/lib/snapd/desktop/applications", store);

    /* 5. Flatpak exports if present */
    scan_directory_desktop_files("/var/lib/flatpak/exports/share/applications", store);
}

/**
 * Sorter: sorts AppInfo alphabetically by display name (case-insensitive).
 */
static gint compare_app_info(gconstpointer a, gconstpointer b)
{
    const AppInfo *app_a = *(const AppInfo **)a;
    const AppInfo *app_b = *(const AppInfo **)b;

    if (!app_a || !app_a->name) return 1;
    if (!app_b || !app_b->name) return -1;

    return g_ascii_strcasecmp(app_a->name, app_b->name);
}

/**
 * app_scanner_scan_all_sync:
 * Collects apps from APT, Snap, Flatpak, and .desktop files, and sorts by name.
 */
GPtrArray *app_scanner_scan_all_sync(void)
{
    GPtrArray *store = app_store_new();

    /* Step 1: Scan APT */
    app_scanner_scan_apt(store);

    /* Step 2: Scan Snap */
    app_scanner_scan_snap(store);

    /* Step 3: Scan Flatpak */
    app_scanner_scan_flatpak(store);

    /* Step 4: Scan and enrich with Desktop Entries */
    app_scanner_scan_desktop_entries(store);

    /* Sort apps alphabetically by name */
    g_ptr_array_sort(store, compare_app_info);

    return store;
}

/**
 * Async thread helper data structure.
 */
typedef struct {
    AppScanCallback callback;
    gpointer user_data;
    GPtrArray *result_store;
    char *status_msg;
} AsyncScanData;

/**
 * Invoked on main thread via g_idle_add once background scan completes.
 */
static gboolean async_scan_idle_notify(gpointer data)
{
    AsyncScanData *async_data = (AsyncScanData *)data;

    if (async_data->callback) {
        async_data->callback(async_data->result_store,
                             async_data->status_msg,
                             async_data->user_data);
    }

    g_free(async_data->status_msg);
    g_free(async_data);
    return G_SOURCE_REMOVE;
}

/**
 * Background worker thread executing scanning without blocking UI.
 */
static gpointer async_scan_thread_worker(gpointer data)
{
    AsyncScanData *async_data = (AsyncScanData *)data;

    GPtrArray *store = app_scanner_scan_all_sync();
    async_data->result_store = store;
    async_data->status_msg = g_strdup_printf("Found %u installed applications", store->len);

    /* Post back to GTK main context */
    g_idle_add(async_scan_idle_notify, async_data);
    return NULL;
}

/**
 * app_scanner_scan_all_async:
 * Launches background worker thread using GLib's g_thread_new().
 */
void app_scanner_scan_all_async(AppScanCallback callback, gpointer user_data)
{
    AsyncScanData *async_data = g_new0(AsyncScanData, 1);
    async_data->callback = callback;
    async_data->user_data = user_data;

    g_thread_new("app-scanner-thread", async_scan_thread_worker, async_data);
}
