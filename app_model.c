#include "app_model.h"
#include <string.h>

/**
 * package_type_to_string:
 * Returns the short display name for the badge.
 */
const char *package_type_to_string(PackageType type)
{
    switch (type) {
        case PACKAGE_TYPE_APT:
            return "APT";
        case PACKAGE_TYPE_SNAP:
            return "Snap";
        case PACKAGE_TYPE_FLATPAK:
            return "Flatpak";
        case PACKAGE_TYPE_DESKTOP_ENTRY:
            return "Desktop Entry";
        default:
            return "Unknown";
    }
}

/**
 * package_type_to_badge_class:
 * Returns the CSS class for custom badge coloring in GTK3.
 */
const char *package_type_to_badge_class(PackageType type)
{
    switch (type) {
        case PACKAGE_TYPE_APT:
            return "badge-apt";
        case PACKAGE_TYPE_SNAP:
            return "badge-snap";
        case PACKAGE_TYPE_FLATPAK:
            return "badge-flatpak";
        case PACKAGE_TYPE_DESKTOP_ENTRY:
            return "badge-desktop";
        default:
            return "badge-default";
    }
}

/**
 * app_info_new:
 * Allocates and initializes an AppInfo record with cloned strings.
 */
AppInfo *app_info_new(const char *name,
                      const char *pkg_id,
                      const char *icon_name,
                      const char *size,
                      PackageType type,
                      const char *comment,
                      const char *exec_cmd,
                      const char *version)
{
    AppInfo *app = g_new0(AppInfo, 1);
    app->name = g_strdup(name ? name : "Unnamed Application");
    app->pkg_id = g_strdup(pkg_id ? pkg_id : "");
    app->icon_name = g_strdup(icon_name ? icon_name : "application-x-executable");
    app->size = g_strdup(size);
    app->type = type;
    app->comment = g_strdup(comment);
    app->exec_cmd = g_strdup(exec_cmd);
    app->version = g_strdup(version);
    return app;
}

/**
 * app_info_free:
 * Safely releases all heap-allocated fields of an AppInfo record.
 */
void app_info_free(AppInfo *app)
{
    if (!app) return;
    g_free(app->name);
    g_free(app->pkg_id);
    g_free(app->icon_name);
    g_free(app->size);
    g_free(app->comment);
    g_free(app->exec_cmd);
    g_free(app->version);
    g_free(app);
}

/**
 * app_store_new:
 * Returns a new GPtrArray that automatically frees AppInfo objects when destroyed.
 */
GPtrArray *app_store_new(void)
{
    return g_ptr_array_new_with_free_func((GDestroyNotify)app_info_free);
}

/**
 * app_store_free:
 * Frees the entire pointer array and its items.
 */
void app_store_free(GPtrArray *store)
{
    if (store) {
        g_ptr_array_unref(store);
    }
}

/**
 * app_store_find_by_pkg_id:
 * Performs linear search in the array matching pkg_id exactly.
 */
AppInfo *app_store_find_by_pkg_id(GPtrArray *store, const char *pkg_id)
{
    if (!store || !pkg_id) return NULL;
    for (guint i = 0; i < store->len; i++) {
        AppInfo *item = (AppInfo *)g_ptr_array_index(store, i);
        if (item->pkg_id && g_strcmp0(item->pkg_id, pkg_id) == 0) {
            return item;
        }
    }
    return NULL;
}

/**
 * app_store_find_by_name:
 * Case-insensitive match on application name for de-duplication.
 */
AppInfo *app_store_find_by_name(GPtrArray *store, const char *name)
{
    if (!store || !name) return NULL;
    for (guint i = 0; i < store->len; i++) {
        AppInfo *item = (AppInfo *)g_ptr_array_index(store, i);
        if (item->name && g_ascii_strcasecmp(item->name, name) == 0) {
            return item;
        }
    }
    return NULL;
}

/**
 * app_store_find_by_exec:
 * Looks up an app where the exec_cmd or pkg_id matches the binary name.
 */
AppInfo *app_store_find_by_exec(GPtrArray *store, const char *exec_bin)
{
    if (!store || !exec_bin) return NULL;
    for (guint i = 0; i < store->len; i++) {
        AppInfo *item = (AppInfo *)g_ptr_array_index(store, i);
        if (item->exec_cmd && g_ascii_strcasecmp(item->exec_cmd, exec_bin) == 0) {
            return item;
        }
        if (item->pkg_id && g_ascii_strcasecmp(item->pkg_id, exec_bin) == 0) {
            return item;
        }
    }
    return NULL;
}
