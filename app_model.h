#ifndef APP_MODEL_H
#define APP_MODEL_H

#include <glib.h>

/**
 * PackageType:
 * Represents the packaging ecosystem for the installed application.
 */
typedef enum {
    PACKAGE_TYPE_APT,
    PACKAGE_TYPE_SNAP,
    PACKAGE_TYPE_FLATPAK,
    PACKAGE_TYPE_DESKTOP_ENTRY
} PackageType;

/**
 * AppInfo:
 * Represents an installed application with metadata for display and uninstallation.
 */
typedef struct {
    char *name;         /* Friendly display name (e.g. "Firefox", "VLC Media Player") */
    char *pkg_id;       /* Package/App identifier used for removal (e.g. "firefox", "org.videolan.VLC") */
    char *icon_name;    /* Icon theme name or absolute file path */
    char *size;         /* Formatted size string (e.g. "128.5 MB", "2.1 GB") or NULL */
    char *comment;      /* Short description/summary */
    char *exec_cmd;     /* Binary / command name (used for desktop file matching) */
    char *version;      /* Version string if available */
    PackageType type;   /* APT, Snap, Flatpak, or Desktop Entry */
} AppInfo;

/**
 * Helper to convert PackageType to a human-readable badge label.
 */
const char *package_type_to_string(PackageType type);

/**
 * Helper to get CSS class name for package type badge styling.
 */
const char *package_type_to_badge_class(PackageType type);

/**
 * Allocate and initialize a new AppInfo instance.
 */
AppInfo *app_info_new(const char *name,
                      const char *pkg_id,
                      const char *icon_name,
                      const char *size,
                      PackageType type,
                      const char *comment,
                      const char *exec_cmd,
                      const char *version);

/**
 * Free memory occupied by an AppInfo struct.
 */
void app_info_free(AppInfo *app);

/**
 * Create a new GPtrArray configured to automatically free AppInfo items.
 */
GPtrArray *app_store_new(void);

/**
 * Free the app store and all stored AppInfo items.
 */
void app_store_free(GPtrArray *store);

/**
 * Find an AppInfo in the store by its package identifier.
 */
AppInfo *app_store_find_by_pkg_id(GPtrArray *store, const char *pkg_id);

/**
 * Find an AppInfo in the store by its display name (case-insensitive).
 */
AppInfo *app_store_find_by_name(GPtrArray *store, const char *name);

/**
 * Find an AppInfo in the store by its exec binary name.
 */
AppInfo *app_store_find_by_exec(GPtrArray *store, const char *exec_bin);

#endif /* APP_MODEL_H */
