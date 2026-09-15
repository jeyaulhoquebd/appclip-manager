/**
 * AppClip Manager - System Cleaner (Orphan Scanner)
 * Header for detecting and safely removing leftover application files.
 *
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef ORPHAN_SCANNER_H
#define ORPHAN_SCANNER_H

#include <glib.h>
#include <time.h>

/* =========================================================================
 * Data Structures
 * ========================================================================= */

typedef enum {
    ORPHAN_TYPE_FILE,
    ORPHAN_TYPE_DIRECTORY
} OrphanItemType;

typedef struct {
    char *path;                 /* Absolute filesystem path */
    OrphanItemType type;        /* File or Directory */
    guint64 size_bytes;         /* Recursive size in bytes */
    time_t last_modified;       /* Last modification timestamp */
    gboolean is_system_path;    /* TRUE for /etc or /var paths requiring root */
    char *matched_pkg;          /* Package identifier or app name */
    char *display_name;         /* Friendly application name */
    gint64 uninstalled_app_id;  /* SQLite row ID in uninstalled_apps (or 0) */
    char *uninstalled_date;     /* Uninstallation timestamp string */
} OrphanFileItem;

typedef struct {
    char *package_name;         /* Package identifier */
    char *display_name;         /* Human-readable application title */
    gint64 uninstalled_app_id;  /* SQLite row ID in uninstalled_apps */
    char *uninstalled_date;     /* When uninstalled (or "Scan Discovery") */
    GList *items;               /* List of OrphanFileItem* */
    guint64 total_size_bytes;   /* Aggregated size of all items in this group */
} OrphanAppGroup;

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
                                    const char *uninstalled_date);

void orphan_file_item_free(OrphanFileItem *item);

OrphanAppGroup *orphan_app_group_new(const char *package_name,
                                     const char *display_name,
                                     gint64 uninstalled_app_id,
                                     const char *uninstalled_date);

void orphan_app_group_free(OrphanAppGroup *group);
void orphan_app_group_list_free(GList *groups);

/* =========================================================================
 * Safety Validation Checks (Section 6)
 * ========================================================================= */

/**
 * Returns TRUE if the directory/file name is in the critical system blocklist.
 * Never suggests or deletes items in this blocklist.
 */
gboolean orphan_scanner_is_blocked_name(const char *name);

/**
 * Returns TRUE if word is too generic (e.g. "app", "data", "bin") to be used
 * for fuzzy or substring matching.
 */
gboolean orphan_scanner_is_generic_word(const char *word);

/**
 * Validates path depth and ensures the path is strictly inside an allowed
 * search location, and never a protected parent or root itself.
 * If out_reason is non-NULL, sets an explanation if unsafe.
 */
gboolean orphan_scanner_is_path_safe_for_deletion(const char *path, char **out_reason);

/* =========================================================================
 * Scanning Operations
 * ========================================================================= */

/**
 * Scans standard locations for leftovers of a single package.
 * Returns a GList of OrphanFileItem* (caller owns list and items).
 */
GList *orphan_scanner_scan_for_package_sync(const char *package_name,
                                            const char *display_name,
                                            gint64 uninstalled_app_id,
                                            const char *uninstalled_date);

typedef void (*OrphanScanPackageCallback)(const char *package_name,
                                          const char *display_name,
                                          GList *items,
                                          guint64 total_bytes,
                                          gpointer user_data);

/**
 * Runs single-package scan on a background thread.
 * Invokes callback on GTK main thread.
 */
void orphan_scanner_scan_for_package_async(const char *package_name,
                                           const char *display_name,
                                           gint64 uninstalled_app_id,
                                           const char *uninstalled_date,
                                           OrphanScanPackageCallback callback,
                                           gpointer user_data);

typedef void (*OrphanFullScanCallback)(GList *groups,
                                       guint64 total_bytes,
                                       const char *status_msg,
                                       gpointer user_data);

/**
 * Runs full orphan scan: checks all uncleaned apps in SQLite history,
 * plus deep scan of ~/.config and ~/.cache for directories not corresponding
 * to any currently installed package in installed_apps.
 * Invokes callback on GTK main thread.
 */
void orphan_scanner_scan_all_async(GList *uncleaned_uninstalled_apps,
                                   GPtrArray *installed_apps,
                                   OrphanFullScanCallback callback,
                                   gpointer user_data);

/* =========================================================================
 * Safe Deletion & Audit Logging
 * ========================================================================= */

/**
 * Calculates total size and latest mtime recursively for a path.
 */
guint64 orphan_scanner_calculate_path_size(const char *path, time_t *out_mtime, OrphanItemType *out_type);

/**
 * Safely removes a file or directory.
 * If is_system is TRUE, uses pkexec to escalate privileges.
 * Validates path safety prior to executing removal.
 * Returns TRUE on success, FALSE otherwise with out_error populated.
 */
gboolean orphan_scanner_delete_path(const char *path, gboolean is_system, char **out_error);

/**
 * Logs deletion event to ~/.local/share/appclip-manager/cleaner-log.txt.
 */
void orphan_scanner_log_deletion(const char *path,
                                 guint64 size_bytes,
                                 gboolean is_system,
                                 gboolean success,
                                 const char *error_msg);

/**
 * Utility to format bytes into human-readable string (e.g. "47.2 MB").
 * Caller must free returned string with g_free().
 */
char *orphan_scanner_format_size(guint64 bytes);

#endif /* ORPHAN_SCANNER_H */
