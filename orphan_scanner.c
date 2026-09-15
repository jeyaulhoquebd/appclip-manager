/**
 * AppClip Manager - System Cleaner (Orphan Scanner)
 * Implementation of leftover detection, recursive sizing, safety checks,
 * and privilege-separated deletion.
 *
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#define _XOPEN_SOURCE 700
#include "orphan_scanner.h"
#include "app_scanner.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

/* =========================================================================
 * SAFETY MECHANISM 6.b: HARD BLOCKLIST
 *
 * Critical system directory and file names that must NEVER be suggested
 * or deleted, even if an application package name coincidentally contains
 * or matches them. Deleting any of these could render the Linux desktop
 * environment, networking, audio, or PAM authentication completely broken.
 * ========================================================================= */
static const char *HARD_BLOCKLIST[] = {
    /* Desktop environment & session essentials */
    "systemd", "dbus", "NetworkManager", "gnome", "polkit", "pulse",
    "pipewire", "Xauthority", "gtk-3.0", "gtk-4.0", "fontconfig", "mime",
    "autostart", "ibus", "dconf", "gconf", "mesa", "wayland", "libinput",
    "xorg", "X11", "system-connections", "pulse-cookie", "environment",
    
    /* Core system directory roots */
    "system", "etc", "var", "usr", "bin", "sbin", "lib", "lib64",
    "boot", "dev", "proc", "sys", "root", "home", "default", "run", "tmp",

    /* Security, authentication & low-level hardware */
    "cups", "ssh", "ssl", "security", "pam.d", "fstab", "passwd", "shadow",
    "sudoers", "sudoers.d", "modules-load.d", "sysctl.d", "udev", "dpkg",
    "apt", "cron", "cron.d", "cron.daily", "systemd-network", "journal",

    /* AppClip Manager's own persistence layer */
    "appclip-manager",

    NULL
};

/* =========================================================================
 * Generic words that are too non-specific to permit substring matching.
 * e.g., if a package is named "app-utils", we do not want to delete every
 * folder named "app" or "utils" in ~/.config!
 * ========================================================================= */
static const char *GENERIC_WORDS[] = {
    "app", "apps", "data", "conf", "config", "cache", "share", "local",
    "common", "base", "core", "test", "bin", "user", "file", "files",
    "plugin", "plugins", "tool", "tools", "util", "utils", "default",
    "system", "client", "server", "linux", "desktop", "lib", "main",
    "doc", "docs", "media", "log", "logs", "temp", "state", "org", "io",
    "net", "com", "software", "manager", "helper", "service",
    NULL
};

/* =========================================================================
 * Memory Management
 * ========================================================================= */

OrphanFileItem *orphan_file_item_new(const char *path,
                                    OrphanItemType type,
                                    guint64 size_bytes,
                                    time_t last_modified,
                                    gboolean is_system_path,
                                    const char *matched_pkg,
                                    const char *display_name,
                                    gint64 uninstalled_app_id,
                                    const char *uninstalled_date)
{
    OrphanFileItem *item = g_new0(OrphanFileItem, 1);
    item->path = g_strdup(path);
    item->type = type;
    item->size_bytes = size_bytes;
    item->last_modified = last_modified;
    item->is_system_path = is_system_path;
    item->matched_pkg = g_strdup(matched_pkg ? matched_pkg : "");
    item->display_name = g_strdup(display_name && *display_name ? display_name : item->matched_pkg);
    item->uninstalled_app_id = uninstalled_app_id;
    item->uninstalled_date = g_strdup(uninstalled_date ? uninstalled_date : "");
    return item;
}

void orphan_file_item_free(OrphanFileItem *item)
{
    if (!item) return;
    g_free(item->path);
    g_free(item->matched_pkg);
    g_free(item->display_name);
    g_free(item->uninstalled_date);
    g_free(item);
}

OrphanAppGroup *orphan_app_group_new(const char *package_name,
                                     const char *display_name,
                                     gint64 uninstalled_app_id,
                                     const char *uninstalled_date)
{
    OrphanAppGroup *group = g_new0(OrphanAppGroup, 1);
    group->package_name = g_strdup(package_name ? package_name : "");
    group->display_name = g_strdup(display_name && *display_name ? display_name : group->package_name);
    group->uninstalled_app_id = uninstalled_app_id;
    group->uninstalled_date = g_strdup(uninstalled_date ? uninstalled_date : "");
    group->items = NULL;
    group->total_size_bytes = 0;
    return group;
}

void orphan_app_group_free(OrphanAppGroup *group)
{
    if (!group) return;
    g_free(group->package_name);
    g_free(group->display_name);
    g_free(group->uninstalled_date);
    g_list_free_full(group->items, (GDestroyNotify)orphan_file_item_free);
    g_free(group);
}

void orphan_app_group_list_free(GList *groups)
{
    g_list_free_full(groups, (GDestroyNotify)orphan_app_group_free);
}

/* =========================================================================
 * Safety Validation Implementation (Section 6)
 * ========================================================================= */

gboolean orphan_scanner_is_blocked_name(const char *name)
{
    if (!name || *name == '\0') return TRUE;

    for (int i = 0; HARD_BLOCKLIST[i] != NULL; i++) {
        if (g_ascii_strcasecmp(name, HARD_BLOCKLIST[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

gboolean orphan_scanner_is_generic_word(const char *word)
{
    if (!word) return TRUE;
    if (strlen(word) < 3) return TRUE;

    for (int i = 0; GENERIC_WORDS[i] != NULL; i++) {
        if (g_ascii_strcasecmp(word, GENERIC_WORDS[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * SAFETY MECHANISM 6.c: MINIMUM PATH DEPTH CHECK
 *
 * Refuses to operate on top-level configuration, cache, home, or root
 * directories themselves.
 * Validates that:
 *  1. Path is non-empty and starts with '/'
 *  2. Path does not contain traversal sequences ("..")
 *  3. Path is strictly a sub-path within one of the designated search locations
 *  4. Path does not equal the search location itself (e.g. cannot delete ~/.config)
 *  5. Basename is not in the HARD BLOCKLIST
 *  6. Path has sufficient directory depth (e.g. at least 3 path components for user dirs)
 */
gboolean orphan_scanner_is_path_safe_for_deletion(const char *path, char **out_reason)
{
    if (!path || *path == '\0' || path[0] != '/') {
        if (out_reason) *out_reason = g_strdup("Invalid or relative path.");
        return FALSE;
    }

    if (strstr(path, "/../") || g_str_has_suffix(path, "/..") || strcmp(path, "..") == 0) {
        if (out_reason) *out_reason = g_strdup("Path contains parent traversal sequences (..).");
        return FALSE;
    }

    /* Check protected top-level directories */
    const char *home = g_get_home_dir();
    const char *user_config = g_get_user_config_dir();
    const char *user_cache = g_get_user_cache_dir();
    const char *user_data = g_get_user_data_dir();
    char *user_state = g_build_filename(home, ".local", "state", NULL);

    /* Disallow deleting home or any parent root */
    if (strcmp(path, "/") == 0 ||
        (home && strcmp(path, home) == 0) ||
        (user_config && strcmp(path, user_config) == 0) ||
        (user_cache && strcmp(path, user_cache) == 0) ||
        (user_data && strcmp(path, user_data) == 0) ||
        (user_state && strcmp(path, user_state) == 0) ||
        strcmp(path, "/etc") == 0 ||
        strcmp(path, "/var") == 0 ||
        strcmp(path, "/var/log") == 0) {
        g_free(user_state);
        if (out_reason) *out_reason = g_strdup("Refusing to delete a protected root system directory.");
        return FALSE;
    }

    /* Verify path is strictly a descendant of an allowed location */
    gboolean is_descendant = FALSE;
    const char *allowed_roots[] = {
        user_config,
        user_cache,
        user_data,
        user_state,
        "/etc",
        "/var/log",
        NULL
    };

    for (int i = 0; allowed_roots[i] != NULL; i++) {
        const char *root = allowed_roots[i];
        if (!root || *root == '\0') continue;

        size_t root_len = strlen(root);
        if (strncmp(path, root, root_len) == 0 && path[root_len] == '/') {
            /* Path is inside this root, and not equal to the root itself */
            const char *sub = path + root_len + 1;
            if (*sub != '\0' && strcmp(sub, ".") != 0) {
                is_descendant = TRUE;
                break;
            }
        }
    }
    g_free(user_state);

    if (!is_descendant) {
        if (out_reason) *out_reason = g_strdup("Path is outside designated cache/config cleaning locations.");
        return FALSE;
    }

    /* Check basename against hard blocklist */
    char *base = g_path_get_basename(path);
    if (orphan_scanner_is_blocked_name(base)) {
        g_free(base);
        if (out_reason) *out_reason = g_strdup("Path name is protected by the critical system blocklist.");
        return FALSE;
    }
    g_free(base);

    /* Verify minimum component depth (avoid deleting something like /etc or /home/user/.config) */
    int slash_count = 0;
    for (const char *p = path; *p; p++) {
        if (*p == '/') slash_count++;
    }

    if (slash_count < 2) {
        if (out_reason) *out_reason = g_strdup("Path depth is too shallow for safe deletion.");
        return FALSE;
    }

    return TRUE;
}

/* =========================================================================
 * Recursive Size Calculation
 * ========================================================================= */

guint64 orphan_scanner_calculate_path_size(const char *path, time_t *out_mtime, OrphanItemType *out_type)
{
    if (!path || *path == '\0') return 0;

    struct stat st;
    if (lstat(path, &st) != 0) {
        return 0;
    }

    if (out_mtime) {
        *out_mtime = st.st_mtime;
    }

    if (S_ISDIR(st.st_mode)) {
        if (out_type) *out_type = ORPHAN_TYPE_DIRECTORY;

        guint64 total = st.st_size;
        time_t max_mtime = st.st_mtime;

        GDir *dir = g_dir_open(path, 0, NULL);
        if (!dir) return total;

        const char *name;
        while ((name = g_dir_read_name(dir)) != NULL) {
            char *child_path = g_build_filename(path, name, NULL);
            struct stat cst;
            if (lstat(child_path, &cst) == 0) {
                if (cst.st_mtime > max_mtime) {
                    max_mtime = cst.st_mtime;
                }
                if (S_ISDIR(cst.st_mode) && !S_ISLNK(cst.st_mode)) {
                    time_t sub_mtime = 0;
                    total += orphan_scanner_calculate_path_size(child_path, &sub_mtime, NULL);
                    if (sub_mtime > max_mtime) max_mtime = sub_mtime;
                } else {
                    total += (guint64)cst.st_size;
                }
            }
            g_free(child_path);
        }
        g_dir_close(dir);

        if (out_mtime) *out_mtime = max_mtime;
        return total;
    } else {
        if (out_type) *out_type = ORPHAN_TYPE_FILE;
        return (guint64)st.st_size;
    }
}

/* =========================================================================
 * Matching Logic
 * ========================================================================= */

/**
 * Checks whether an entry name in a configuration or cache directory
 * corresponds to the target package name.
 * Requires specific matches and rejects generic words to avoid false positives.
 */
static gboolean matches_package_name(const char *entry_name, const char *pkg_name)
{
    if (!entry_name || !pkg_name) return FALSE;
    if (orphan_scanner_is_blocked_name(entry_name)) return FALSE;

    char *entry_lower = g_ascii_strdown(entry_name, -1);
    char *pkg_lower = g_ascii_strdown(pkg_name, -1);

    /* 1. Exact case-insensitive match (e.g. "vlc" == "vlc") */
    if (strcmp(entry_lower, pkg_lower) == 0) {
        g_free(entry_lower);
        g_free(pkg_lower);
        return TRUE;
    }

    /* 2. Prefix match followed by delimiter (e.g. "vlc-data", "vlc_cache", "vlc.conf") */
    size_t pkg_len = strlen(pkg_lower);
    if (strncmp(entry_lower, pkg_lower, pkg_len) == 0) {
        char next_char = entry_lower[pkg_len];
        if (next_char == '-' || next_char == '_' || next_char == '.' || next_char == '\0') {
            g_free(entry_lower);
            g_free(pkg_lower);
            return TRUE;
        }
    }

    /* 3. Reverse prefix: Flatpak / domain identifiers (e.g. "org.videolan.VLC" matching "vlc") */
    if (g_str_has_suffix(entry_lower, pkg_lower)) {
        size_t entry_len = strlen(entry_lower);
        if (entry_len > pkg_len) {
            char prev_char = entry_lower[entry_len - pkg_len - 1];
            if (prev_char == '.' || prev_char == '-' || prev_char == '_') {
                /* Only match if pkg_lower is sufficiently specific (>= 3 chars and not generic) */
                if (!orphan_scanner_is_generic_word(pkg_lower)) {
                    g_free(entry_lower);
                    g_free(pkg_lower);
                    return TRUE;
                }
            }
        }
    }

    /* 4. Token-level substring match (e.g., "google-chrome" matching "chrome") */
    if (pkg_len >= 4 && !orphan_scanner_is_generic_word(pkg_lower)) {
        char **tokens = g_strsplit_set(entry_lower, "-_.", -1);
        gboolean token_matched = FALSE;
        for (int i = 0; tokens && tokens[i] != NULL; i++) {
            if (strcmp(tokens[i], pkg_lower) == 0) {
                token_matched = TRUE;
                break;
            }
        }
        g_strfreev(tokens);
        if (token_matched) {
            g_free(entry_lower);
            g_free(pkg_lower);
            return TRUE;
        }
    }

    g_free(entry_lower);
    g_free(pkg_lower);
    return FALSE;
}

/* =========================================================================
 * Single Package Scan
 * ========================================================================= */

GList *orphan_scanner_scan_for_package_sync(const char *package_name,
                                            const char *display_name,
                                            gint64 uninstalled_app_id,
                                            const char *uninstalled_date)
{
    if (!package_name || *package_name == '\0') return NULL;
    if (orphan_scanner_is_blocked_name(package_name)) return NULL;

    GList *results = NULL;

    const char *home = g_get_home_dir();
    const char *user_config = g_get_user_config_dir();
    const char *user_cache = g_get_user_cache_dir();
    const char *user_data = g_get_user_data_dir();
    char *user_state = g_build_filename(home, ".local", "state", NULL);

    struct ScanTarget {
        const char *dir_path;
        gboolean is_system;
    } targets[] = {
        { user_config, FALSE },
        { user_cache,  FALSE },
        { user_data,   FALSE },
        { user_state,  FALSE },
        { "/etc",      TRUE  },
        { "/var/log",  TRUE  },
        { NULL,        FALSE }
    };

    for (int t = 0; targets[t].dir_path != NULL; t++) {
        const char *base_dir = targets[t].dir_path;
        gboolean is_system = targets[t].is_system;

        if (!base_dir || !g_file_test(base_dir, G_FILE_TEST_IS_DIR)) {
            continue;
        }

        GDir *dir = g_dir_open(base_dir, 0, NULL);
        if (!dir) continue;

        const char *entry_name;
        while ((entry_name = g_dir_read_name(dir)) != NULL) {
            if (matches_package_name(entry_name, package_name)) {
                char *full_path = g_build_filename(base_dir, entry_name, NULL);

                /* Validate through Section 6 safety rules */
                if (orphan_scanner_is_path_safe_for_deletion(full_path, NULL)) {
                    time_t mtime = 0;
                    OrphanItemType type = ORPHAN_TYPE_FILE;
                    guint64 size = orphan_scanner_calculate_path_size(full_path, &mtime, &type);

                    OrphanFileItem *item = orphan_file_item_new(
                        full_path,
                        type,
                        size,
                        mtime,
                        is_system,
                        package_name,
                        display_name,
                        uninstalled_app_id,
                        uninstalled_date
                    );
                    results = g_list_append(results, item);
                }
                g_free(full_path);
            }
        }
        g_dir_close(dir);
    }

    g_free(user_state);
    return results;
}

/* =========================================================================
 * Async Single-Package Scan Worker
 * ========================================================================= */

typedef struct {
    char *package_name;
    char *display_name;
    gint64 uninstalled_app_id;
    char *uninstalled_date;
    OrphanScanPackageCallback callback;
    gpointer user_data;
    GList *results;
    guint64 total_bytes;
} PkgScanJobData;

static gboolean on_pkg_scan_idle(gpointer data)
{
    PkgScanJobData *job = (PkgScanJobData *)data;
    if (job->callback) {
        job->callback(job->package_name, job->display_name, job->results, job->total_bytes, job->user_data);
    }
    g_free(job->package_name);
    g_free(job->display_name);
    g_free(job->uninstalled_date);
    g_free(job);
    return G_SOURCE_REMOVE;
}

static gpointer pkg_scan_worker_thread(gpointer data)
{
    PkgScanJobData *job = (PkgScanJobData *)data;
    job->results = orphan_scanner_scan_for_package_sync(
        job->package_name,
        job->display_name,
        job->uninstalled_app_id,
        job->uninstalled_date
    );

    guint64 total = 0;
    for (GList *l = job->results; l != NULL; l = l->next) {
        OrphanFileItem *item = (OrphanFileItem *)l->data;
        total += item->size_bytes;
    }
    job->total_bytes = total;

    g_idle_add(on_pkg_scan_idle, job);
    return NULL;
}

void orphan_scanner_scan_for_package_async(const char *package_name,
                                           const char *display_name,
                                           gint64 uninstalled_app_id,
                                           const char *uninstalled_date,
                                           OrphanScanPackageCallback callback,
                                           gpointer user_data)
{
    PkgScanJobData *job = g_new0(PkgScanJobData, 1);
    job->package_name = g_strdup(package_name);
    job->display_name = g_strdup(display_name);
    job->uninstalled_app_id = uninstalled_app_id;
    job->uninstalled_date = g_strdup(uninstalled_date);
    job->callback = callback;
    job->user_data = user_data;

    g_thread_new("orphan-pkg-scanner", pkg_scan_worker_thread, job);
}

/* =========================================================================
 * Full Scan (All Uncleaned Apps + Cache/Config Directory Cross-Reference)
 * ========================================================================= */

typedef struct {
    GList *uninstalled_apps; /* List of UninstalledAppRecord* copied */
    GPtrArray *installed_apps; /* Retained copy or scanned */
    OrphanFullScanCallback callback;
    gpointer user_data;
    GList *groups;
    guint64 total_bytes;
    char *status_msg;
} FullScanJobData;

static gboolean on_full_scan_idle(gpointer data)
{
    FullScanJobData *job = (FullScanJobData *)data;
    if (job->callback) {
        job->callback(job->groups, job->total_bytes, job->status_msg, job->user_data);
    }
    g_free(job->status_msg);
    g_free(job);
    return G_SOURCE_REMOVE;
}

static gpointer full_scan_worker_thread(gpointer data)
{
    FullScanJobData *job = (FullScanJobData *)data;
    GList *groups = NULL;
    guint64 grand_total = 0;
    guint total_items_count = 0;

    /* Part 1: Scan for leftovers of apps explicitly recorded in uninstalled_apps */
    for (GList *l = job->uninstalled_apps; l != NULL; l = l->next) {
        UninstalledAppRecord *rec = (UninstalledAppRecord *)l->data;
        if (!rec || !rec->package_name) continue;

        GList *items = orphan_scanner_scan_for_package_sync(
            rec->package_name,
            rec->display_name,
            rec->id,
            rec->uninstalled_at
        );

        if (items) {
            OrphanAppGroup *grp = orphan_app_group_new(
                rec->package_name,
                rec->display_name,
                rec->id,
                rec->uninstalled_at
            );
            grp->items = items;
            guint64 grp_size = 0;
            for (GList *it = items; it != NULL; it = it->next) {
                OrphanFileItem *fi = (OrphanFileItem *)it->data;
                grp_size += fi->size_bytes;
                total_items_count++;
            }
            grp->total_size_bytes = grp_size;
            grand_total += grp_size;
            groups = g_list_append(groups, grp);
        }
    }

    /* Part 2: Cross-reference ~/.config and ~/.cache for directories that do not
     * correspond to any installed application */
    const char *user_config = g_get_user_config_dir();
    const char *user_cache = g_get_user_cache_dir();

    const char *scan_dirs[] = { user_config, user_cache, NULL };
    OrphanAppGroup *generic_group = NULL;

    /* Build hash set of installed package names and binary names */
    GHashTable *installed_lookup = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    if (job->installed_apps) {
        for (guint i = 0; i < job->installed_apps->len; i++) {
            AppInfo *app = (AppInfo *)g_ptr_array_index(job->installed_apps, i);
            if (app->pkg_id) {
                char *pkg_lower = g_ascii_strdown(app->pkg_id, -1);
                g_hash_table_add(installed_lookup, pkg_lower);
            }
            if (app->name) {
                char *name_lower = g_ascii_strdown(app->name, -1);
                g_hash_table_add(installed_lookup, name_lower);
            }
        }
    }

    for (int d = 0; scan_dirs[d] != NULL; d++) {
        const char *dir_path = scan_dirs[d];
        if (!dir_path || !g_file_test(dir_path, G_FILE_TEST_IS_DIR)) continue;

        GDir *dir = g_dir_open(dir_path, 0, NULL);
        if (!dir) continue;

        const char *name;
        while ((name = g_dir_read_name(dir)) != NULL) {
            /* Skip blocked names and generic words */
            if (orphan_scanner_is_blocked_name(name)) continue;
            if (orphan_scanner_is_generic_word(name)) continue;

            /* Check if this matches an already identified uninstalled group */
            gboolean already_grouped = FALSE;
            for (GList *g = groups; g != NULL; g = g->next) {
                OrphanAppGroup *grp = (OrphanAppGroup *)g->data;
                for (GList *it = grp->items; it != NULL; it = it->next) {
                    OrphanFileItem *fi = (OrphanFileItem *)it->data;
                    char *b = g_path_get_basename(fi->path);
                    if (g_ascii_strcasecmp(b, name) == 0) {
                        already_grouped = TRUE;
                    }
                    g_free(b);
                    if (already_grouped) break;
                }
                if (already_grouped) break;
            }
            if (already_grouped) continue;

            /* Cross-reference against installed applications */
            char *name_lower = g_ascii_strdown(name, -1);
            gboolean is_installed = g_hash_table_contains(installed_lookup, name_lower);
            if (!is_installed) {
                /* Check tokenized match against installed apps */
                GHashTableIter iter;
                gpointer key, value;
                g_hash_table_iter_init(&iter, installed_lookup);
                while (g_hash_table_iter_next(&iter, &key, &value)) {
                    const char *inst_pkg = (const char *)key;
                    if (g_str_has_prefix(name_lower, inst_pkg) || g_str_has_prefix(inst_pkg, name_lower)) {
                        is_installed = TRUE;
                        break;
                    }
                }
            }
            g_free(name_lower);

            if (!is_installed) {
                char *full_path = g_build_filename(dir_path, name, NULL);
                if (orphan_scanner_is_path_safe_for_deletion(full_path, NULL)) {
                    time_t mtime = 0;
                    OrphanItemType type = ORPHAN_TYPE_FILE;
                    guint64 sz = orphan_scanner_calculate_path_size(full_path, &mtime, &type);

                    /* Only consider orphaned items larger than 4KB to avoid noise */
                    if (sz > 4096) {
                        if (!generic_group) {
                            generic_group = orphan_app_group_new(
                                "orphaned_cache",
                                "Orphaned Cache & Application Data",
                                0,
                                "Deep Scan"
                            );
                        }
                        OrphanFileItem *fi = orphan_file_item_new(
                            full_path,
                            type,
                            sz,
                            mtime,
                            FALSE,
                            name,
                            name,
                            0,
                            "Deep Scan"
                        );
                        generic_group->items = g_list_append(generic_group->items, fi);
                        generic_group->total_size_bytes += sz;
                        grand_total += sz;
                        total_items_count++;
                    }
                }
                g_free(full_path);
            }
        }
        g_dir_close(dir);
    }

    g_hash_table_destroy(installed_lookup);

    if (generic_group && generic_group->items) {
        groups = g_list_append(groups, generic_group);
    } else if (generic_group) {
        orphan_app_group_free(generic_group);
    }

    char *sz_formatted = orphan_scanner_format_size(grand_total);
    job->groups = groups;
    job->total_bytes = grand_total;
    job->status_msg = g_strdup_printf("Scan complete: found %u items across %u applications (%s reclaimable)",
                                      total_items_count, g_list_length(groups), sz_formatted);
    g_free(sz_formatted);

    /* Free job data copy */
    uninstalled_app_record_list_free(job->uninstalled_apps);
    if (job->installed_apps) g_ptr_array_unref(job->installed_apps);

    g_idle_add(on_full_scan_idle, job);
    return NULL;
}

void orphan_scanner_scan_all_async(GList *uncleaned_uninstalled_apps,
                                   GPtrArray *installed_apps,
                                   OrphanFullScanCallback callback,
                                   gpointer user_data)
{
    FullScanJobData *job = g_new0(FullScanJobData, 1);

    /* Deep copy uninstalled apps records */
    GList *copy = NULL;
    for (GList *l = uncleaned_uninstalled_apps; l != NULL; l = l->next) {
        UninstalledAppRecord *r = (UninstalledAppRecord *)l->data;
        copy = g_list_append(copy, uninstalled_app_record_new(
            r->id, r->package_name, r->display_name, r->uninstalled_at, r->leftovers_cleaned
        ));
    }
    job->uninstalled_apps = copy;

    if (installed_apps) {
        g_ptr_array_ref(installed_apps);
        job->installed_apps = installed_apps;
    }
    job->callback = callback;
    job->user_data = user_data;

    g_thread_new("orphan-full-scanner", full_scan_worker_thread, job);
}

/* =========================================================================
 * SAFE DELETION MECHANISMS (Section 6.e, 6.f, 6.g)
 * ========================================================================= */

/**
 * nftw callback for user-space recursive directory removal.
 * Uses FTW_DEPTH so child files and subdirectories are unlinked before
 * parent directory is rmdir'd.
 */
static int nftw_remove_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    (void)sb;
    (void)ftwbuf;

    if (typeflag == FTW_DP) {
        return rmdir(fpath);
    } else {
        return unlink(fpath);
    }
}

/**
 * Executes safe removal of a target path:
 *  - Validates through Section 6 safety rules first.
 *  - For user paths: uses nftw() with FTW_DEPTH | FTW_PHYS and unlink().
 *    If needed, falls back to fork() + execvp("rm", ["rm", "-rf", "--", path]).
 *  - For system paths (/etc, /var/log): uses fork() + execvp("pkexec",
 *    ["pkexec", "rm", "-rf", "--", path]) with "--" argument separator.
 */
gboolean orphan_scanner_delete_path(const char *path, gboolean is_system, char **out_error)
{
    char *safety_reason = NULL;
    if (!orphan_scanner_is_path_safe_for_deletion(path, &safety_reason)) {
        if (out_error) {
            *out_error = g_strdup_printf("Safety check blocked deletion: %s",
                                         safety_reason ? safety_reason : "Unsafe path");
        }
        g_free(safety_reason);
        return FALSE;
    }
    g_free(safety_reason);

    /* Double check existence */
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        /* Already gone */
        return TRUE;
    }

    if (is_system) {
        /* ---------------------------------------------------------------------
         * SAFETY MECHANISM 6.e & 6.f:
         * System paths (/etc, /var/log) require elevated PolicyKit authentication.
         * We execute "pkexec rm -rf -- <path>" via execvp (never shell string
         * concatenation) to eliminate command injection vulnerabilities.
         * --------------------------------------------------------------------- */
        char *pkexec = g_find_program_in_path("pkexec");
        if (!pkexec) {
            if (out_error) *out_error = g_strdup("pkexec is not available for elevated system deletion.");
            return FALSE;
        }
        g_free(pkexec);

        char *argv[] = {
            "pkexec",
            "rm",
            "-rf",
            "--",
            (char *)path,
            NULL
        };

        int exit_code = 0;
        char *output = app_scanner_run_cmd_pipe("pkexec", argv, &exit_code);

        if (exit_code != 0) {
            if (out_error) {
                *out_error = g_strdup_printf("Failed to delete system path (exit code %d): %s",
                                             exit_code, output ? output : "Cancelled or permission denied.");
            }
            g_free(output);
            return FALSE;
        }
        g_free(output);
        return TRUE;
    } else {
        /* ---------------------------------------------------------------------
         * User-space deletion:
         * Attempt native nftw with FTW_DEPTH | FTW_PHYS for directories,
         * or unlink for regular files/symlinks.
         * --------------------------------------------------------------------- */
        struct stat st;
        if (lstat(path, &st) != 0) {
            if (errno == ENOENT) return TRUE;
            if (out_error) *out_error = g_strdup(g_strerror(errno));
            return FALSE;
        }

        int res = 0;
        if (S_ISDIR(st.st_mode)) {
            res = nftw(path, nftw_remove_cb, 64, FTW_DEPTH | FTW_PHYS);
        } else {
            res = unlink(path);
        }

        if (res != 0) {
            /* Fallback to safe fork() + execvp("rm", ["rm", "-rf", "--", path]) */
            char *argv[] = {
                "rm",
                "-rf",
                "--",
                (char *)path,
                NULL
            };
            int exit_code = 0;
            char *output = app_scanner_run_cmd_pipe("rm", argv, &exit_code);
            if (exit_code != 0) {
                if (out_error) {
                    *out_error = g_strdup_printf("Could not remove %s: %s", path,
                                                 output ? output : g_strerror(errno));
                }
                g_free(output);
                return FALSE;
            }
            g_free(output);
        }
        return TRUE;
    }
}

/* =========================================================================
 * SAFETY MECHANISM 6.g: AUDIT LOGGING
 *
 * Logs every deletion action to ~/.local/share/appclip-manager/cleaner-log.txt
 * with timestamp, size, security domain, and outcome.
 * ========================================================================= */
void orphan_scanner_log_deletion(const char *path,
                                 guint64 size_bytes,
                                 gboolean is_system,
                                 gboolean success,
                                 const char *error_msg)
{
    if (!path) return;

    const char *data_dir = g_get_user_data_dir();
    char *app_dir = g_build_filename(data_dir, "appclip-manager", NULL);
    g_mkdir_with_parents(app_dir, 0755);

    char *log_path = g_build_filename(app_dir, "cleaner-log.txt", NULL);
    g_free(app_dir);

    FILE *fp = fopen(log_path, "a");
    g_free(log_path);
    if (!fp) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    char *sz_str = orphan_scanner_format_size(size_bytes);

    fprintf(fp, "[%s] [%s] [%s] Path: %s (Size: %s)%s%s\n",
            time_buf,
            success ? "SUCCESS" : "FAILED",
            is_system ? "ADMIN/ROOT" : "USER",
            path,
            sz_str,
            error_msg && *error_msg ? " | Reason: " : "",
            error_msg && *error_msg ? error_msg : "");

    g_free(sz_str);
    fclose(fp);
}

char *orphan_scanner_format_size(guint64 bytes)
{
    if (bytes >= (guint64)1024 * 1024 * 1024) {
        double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
        return g_strdup_printf("%.2f GB", gb);
    } else if (bytes >= 1024 * 1024) {
        double mb = (double)bytes / (1024.0 * 1024.0);
        return g_strdup_printf("%.1f MB", mb);
    } else if (bytes >= 1024) {
        double kb = (double)bytes / 1024.0;
        return g_strdup_printf("%.1f KB", kb);
    } else {
        return g_strdup_printf("%llu B", (unsigned long long)bytes);
    }
}
