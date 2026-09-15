/**
 * AppClip Manager - SQLite3 Storage Implementation
 * Robust database persistence with prepared statements and file cleanup.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>

static sqlite3 *db_handle = NULL;

static char *get_database_path(void)
{
    const char *data_dir = g_get_user_data_dir();
    char *app_data = g_build_filename(data_dir, "appclip-manager", NULL);
    g_mkdir_with_parents(app_data, 0755);

    char *path = g_build_filename(app_data, "history.db", NULL);
    g_free(app_data);
    return path;
}

char *db_get_clips_dir(void)
{
    const char *data_dir = g_get_user_data_dir();
    char *clips_dir = g_build_filename(data_dir, "appclip-manager", "clips", NULL);
    g_mkdir_with_parents(clips_dir, 0755);
    return clips_dir;
}

char *db_save_pixbuf_to_file(GdkPixbuf *pixbuf)
{
    if (!pixbuf) return NULL;

    char *clips_dir = db_get_clips_dir();
    gint64 timestamp = g_get_real_time() / 1000; /* milliseconds */
    static guint counter = 0;
    counter++;

    char *file_name = g_strdup_printf("clip_%lld_%u.png", (long long)timestamp, counter);
    char *full_path = g_build_filename(clips_dir, file_name, NULL);
    g_free(file_name);
    g_free(clips_dir);

    GError *err = NULL;
    gboolean saved = gdk_pixbuf_save(pixbuf, full_path, "png", &err, NULL);
    if (!saved) {
        g_warning("Failed to save pixbuf to %s: %s", full_path, err ? err->message : "unknown");
        if (err) g_error_free(err);
        g_free(full_path);
        return NULL;
    }

    return full_path;
}

gboolean db_init(void)
{
    if (db_handle) return TRUE;

    char *db_path = get_database_path();
    int rc = sqlite3_open(db_path, &db_handle);
    if (rc != SQLITE_OK) {
        g_critical("Cannot open SQLite database at %s: %s", db_path, sqlite3_errmsg(db_handle));
        g_free(db_path);
        if (db_handle) {
            sqlite3_close(db_handle);
            db_handle = NULL;
        }
        return FALSE;
    }
    g_free(db_path);

    /* Enable WAL mode for better concurrency and performance */
    sqlite3_exec(db_handle, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
    sqlite3_exec(db_handle, "PRAGMA synchronous = NORMAL;", NULL, NULL, NULL);

    /* Create clips table and indexes strictly according to requirements */
    const char *schema_sql =
        "CREATE TABLE IF NOT EXISTS clips ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  type TEXT CHECK(type IN ('text','image')) NOT NULL,"
        "  content TEXT,"
        "  file_path TEXT,"
        "  preview TEXT,"
        "  content_hash TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  pinned INTEGER DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_created_at ON clips(created_at DESC);"
        "CREATE INDEX IF NOT EXISTS idx_pinned ON clips(pinned DESC);"
        ""
        "/* System Cleaner: Uninstalled applications tracking */"
        "CREATE TABLE IF NOT EXISTS uninstalled_apps ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  package_name TEXT NOT NULL,"
        "  display_name TEXT,"
        "  uninstalled_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  leftovers_cleaned INTEGER DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_uninstalled_pkg ON uninstalled_apps(package_name);"
        "CREATE INDEX IF NOT EXISTS idx_uninstalled_cleaned ON uninstalled_apps(leftovers_cleaned);";

    char *err_msg = NULL;
    rc = sqlite3_exec(db_handle, schema_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_critical("Failed to create clips schema: %s", err_msg ? err_msg : "unknown");
        sqlite3_free(err_msg);
        return FALSE;
    }

    /* Ensure clips image directory exists */
    char *clips_dir = db_get_clips_dir();
    g_free(clips_dir);

    return TRUE;
}

void db_close(void)
{
    if (db_handle) {
        sqlite3_close(db_handle);
        db_handle = NULL;
    }
}

int db_insert_text_clip(const char *content, const char *preview, const char *content_hash)
{
    if (!db_handle && !db_init()) return -1;
    if (!content) return -1;

    const char *sql =
        "INSERT INTO clips (type, content, file_path, preview, content_hash, created_at, pinned) "
        "VALUES ('text', ?1, NULL, ?2, ?3, CURRENT_TIMESTAMP, 0);";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("Failed to prepare insert text clip: %s", sqlite3_errmsg(db_handle));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, content, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, preview ? preview : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, content_hash ? content_hash : "", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    int inserted_id = -1;
    if (rc == SQLITE_DONE) {
        inserted_id = (int)sqlite3_last_insert_rowid(db_handle);
    } else {
        g_warning("Failed to step insert text clip: %s", sqlite3_errmsg(db_handle));
    }

    sqlite3_finalize(stmt);
    return inserted_id;
}

int db_insert_image_clip(const char *file_path, const char *content_hash)
{
    if (!db_handle && !db_init()) return -1;
    if (!file_path) return -1;

    const char *sql =
        "INSERT INTO clips (type, content, file_path, preview, content_hash, created_at, pinned) "
        "VALUES ('image', NULL, ?1, NULL, ?2, CURRENT_TIMESTAMP, 0);";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("Failed to prepare insert image clip: %s", sqlite3_errmsg(db_handle));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, file_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, content_hash ? content_hash : "", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    int inserted_id = -1;
    if (rc == SQLITE_DONE) {
        inserted_id = (int)sqlite3_last_insert_rowid(db_handle);
    } else {
        g_warning("Failed to step insert image clip: %s", sqlite3_errmsg(db_handle));
    }

    sqlite3_finalize(stmt);
    return inserted_id;
}

static ClipEntry *clip_entry_from_stmt(sqlite3_stmt *stmt)
{
    ClipEntry *entry = clip_entry_new();
    entry->id = sqlite3_column_int(stmt, 0);

    const char *type_str = (const char *)sqlite3_column_text(stmt, 1);
    if (type_str && strcmp(type_str, "image") == 0) {
        entry->type = CLIP_TYPE_IMAGE;
    } else {
        entry->type = CLIP_TYPE_TEXT;
    }

    const char *content = (const char *)sqlite3_column_text(stmt, 2);
    if (content) entry->content = g_strdup(content);

    const char *file_path = (const char *)sqlite3_column_text(stmt, 3);
    if (file_path) entry->file_path = g_strdup(file_path);

    const char *preview = (const char *)sqlite3_column_text(stmt, 4);
    if (preview) entry->preview = g_strdup(preview);

    const char *hash = (const char *)sqlite3_column_text(stmt, 5);
    if (hash) entry->content_hash = g_strdup(hash);

    /* unix timestamp in seconds from strftime */
    entry->created_at = (time_t)sqlite3_column_int64(stmt, 6);
    entry->pinned = (sqlite3_column_int(stmt, 7) != 0);

    /* If image, load dimension metadata if file exists */
    if (entry->type == CLIP_TYPE_IMAGE && entry->file_path && g_file_test(entry->file_path, G_FILE_TEST_EXISTS)) {
        int w = 0, h = 0;
        if (gdk_pixbuf_get_file_info(entry->file_path, &w, &h)) {
            entry->image_width = w;
            entry->image_height = h;
        }
        GFile *gf = g_file_new_for_path(entry->file_path);
        GFileInfo *fi = g_file_query_info(gf, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
        if (fi) {
            entry->file_size_bytes = g_file_info_get_size(fi);
            g_object_unref(fi);
        }
        g_object_unref(gf);
    }

    return entry;
}

GList *db_get_recent_clips(int limit, const char *search_query)
{
    if (!db_handle && !db_init()) return NULL;
    if (limit <= 0) limit = 100;

    sqlite3_stmt *stmt = NULL;
    int rc;

    if (search_query && search_query[0] != '\0') {
        const char *sql =
            "SELECT id, type, content, file_path, preview, content_hash, "
            "       strftime('%s', created_at) AS epoch_time, pinned "
            "FROM clips "
            "WHERE (type = 'text' AND (content LIKE ?1 OR preview LIKE ?1)) "
            "ORDER BY pinned DESC, created_at DESC "
            "LIMIT ?2;";

        rc = sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            g_warning("Prepare search clips failed: %s", sqlite3_errmsg(db_handle));
            return NULL;
        }

        char *like_pattern = g_strdup_printf("%%%s%%", search_query);
        sqlite3_bind_text(stmt, 1, like_pattern, -1, g_free);
        sqlite3_bind_int(stmt, 2, limit);
    } else {
        const char *sql =
            "SELECT id, type, content, file_path, preview, content_hash, "
            "       strftime('%s', created_at) AS epoch_time, pinned "
            "FROM clips "
            "ORDER BY pinned DESC, created_at DESC "
            "LIMIT ?1;";

        rc = sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            g_warning("Prepare recent clips failed: %s", sqlite3_errmsg(db_handle));
            return NULL;
        }

        sqlite3_bind_int(stmt, 1, limit);
    }

    GList *list = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ClipEntry *entry = clip_entry_from_stmt(stmt);
        list = g_list_append(list, entry);
    }

    sqlite3_finalize(stmt);
    return list;
}

ClipEntry *db_get_clip_by_id(int id)
{
    if (!db_handle && !db_init()) return NULL;

    const char *sql =
        "SELECT id, type, content, file_path, preview, content_hash, "
        "       strftime('%s', created_at) AS epoch_time, pinned "
        "FROM clips WHERE id = ?1 LIMIT 1;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, id);

    ClipEntry *entry = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        entry = clip_entry_from_stmt(stmt);
    }

    sqlite3_finalize(stmt);
    return entry;
}

char *db_get_clip_full_content(int clip_id)
{
    if (!db_handle && !db_init()) return NULL;

    const char *sql = "SELECT content FROM clips WHERE id = ?1 LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        g_warning("db_get_clip_full_content prepare failed: %s", sqlite3_errmsg(db_handle));
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, clip_id);

    char *content = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *txt = (const char *)sqlite3_column_text(stmt, 0);
        if (txt) {
            content = g_strdup(txt);
        }
    }

    sqlite3_finalize(stmt);
    return content;
}

char *db_get_clip_file_path(int clip_id)
{
    if (!db_handle && !db_init()) return NULL;

    const char *sql = "SELECT file_path FROM clips WHERE id = ?1 LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        g_warning("db_get_clip_file_path prepare failed: %s", sqlite3_errmsg(db_handle));
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, clip_id);

    char *path = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *txt = (const char *)sqlite3_column_text(stmt, 0);
        if (txt) {
            path = g_strdup(txt);
        }
    }

    sqlite3_finalize(stmt);
    return path;
}

gboolean db_update_clip_content(int clip_id, const char *new_content)
{
    if (!db_handle && !db_init()) return FALSE;
    if (!new_content) new_content = "";

    char *preview = clip_generate_text_preview(new_content, 100);
    char *new_hash = clip_compute_text_hash(new_content);

    const char *sql = "UPDATE clips SET content = ?1, preview = ?2, content_hash = ?3 WHERE id = ?4;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        g_warning("db_update_clip_content prepare failed: %s", sqlite3_errmsg(db_handle));
        g_free(preview);
        g_free(new_hash);
        return FALSE;
    }

    sqlite3_bind_text(stmt, 1, new_content, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, preview ? preview : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, new_hash ? new_hash : "", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, clip_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    g_free(preview);
    g_free(new_hash);

    return (rc == SQLITE_DONE);
}

gboolean db_delete_clip(int id)
{
    if (!db_handle && !db_init()) return FALSE;

    /* Check if this clip has an image file to unlink */
    ClipEntry *entry = db_get_clip_by_id(id);
    if (entry) {
        if (entry->type == CLIP_TYPE_IMAGE && entry->file_path) {
            unlink(entry->file_path);
        }
        clip_entry_free(entry);
    }

    const char *sql = "DELETE FROM clips WHERE id = ?1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, id);
    gboolean ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

gboolean db_toggle_pin(int id, gboolean *out_new_pinned)
{
    if (!db_handle && !db_init()) return FALSE;

    const char *sql_query = "SELECT pinned FROM clips WHERE id = ?1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql_query, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }
    sqlite3_bind_int(stmt, 1, id);

    int current_pinned = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        current_pinned = sqlite3_column_int(stmt, 0);
    } else {
        sqlite3_finalize(stmt);
        return FALSE;
    }
    sqlite3_finalize(stmt);

    int new_pinned = current_pinned ? 0 : 1;

    const char *sql_update = "UPDATE clips SET pinned = ?1 WHERE id = ?2;";
    if (sqlite3_prepare_v2(db_handle, sql_update, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }
    sqlite3_bind_int(stmt, 1, new_pinned);
    sqlite3_bind_int(stmt, 2, id);

    gboolean ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    if (ok && out_new_pinned) {
        *out_new_pinned = (new_pinned != 0);
    }
    return ok;
}

gboolean db_clear_history(gboolean keep_pinned)
{
    if (!db_handle && !db_init()) return FALSE;

    /* 1. Unlink disk images for rows about to be deleted */
    const char *sql_find = keep_pinned ?
        "SELECT file_path FROM clips WHERE pinned = 0 AND type = 'image' AND file_path IS NOT NULL;" :
        "SELECT file_path FROM clips WHERE type = 'image' AND file_path IS NOT NULL;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql_find, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *fp = (const char *)sqlite3_column_text(stmt, 0);
            if (fp && fp[0] != '\0') {
                unlink(fp);
            }
        }
        sqlite3_finalize(stmt);
    }

    /* 2. Delete rows */
    const char *sql_del = keep_pinned ?
        "DELETE FROM clips WHERE pinned = 0;" :
        "DELETE FROM clips;";

    int rc = sqlite3_exec(db_handle, sql_del, NULL, NULL, NULL);
    return (rc == SQLITE_OK);
}

int db_prune_old_clips(int max_items, int max_days)
{
    if (!db_handle && !db_init()) return 0;
    int pruned_count = 0;

    /* 1. Prune by age (older than max_days, except pinned) */
    if (max_days > 0) {
        const char *sql_find_old =
            "SELECT file_path FROM clips "
            "WHERE pinned = 0 AND type = 'image' AND file_path IS NOT NULL "
            "  AND created_at < datetime('now', '-' || ?1 || ' days');";

        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db_handle, sql_find_old, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, max_days);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *fp = (const char *)sqlite3_column_text(stmt, 0);
                if (fp && fp[0] != '\0') unlink(fp);
            }
            sqlite3_finalize(stmt);
        }

        const char *sql_del_old =
            "DELETE FROM clips WHERE pinned = 0 AND created_at < datetime('now', '-' || ?1 || ' days');";
        if (sqlite3_prepare_v2(db_handle, sql_del_old, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, max_days);
            sqlite3_step(stmt);
            pruned_count += sqlite3_changes(db_handle);
            sqlite3_finalize(stmt);
        }
    }

    /* 2. Prune by count: keep only max_items non-pinned items */
    if (max_items > 0) {
        const char *sql_find_excess =
            "SELECT file_path FROM clips "
            "WHERE pinned = 0 AND type = 'image' AND file_path IS NOT NULL "
            "  AND id NOT IN ("
            "    SELECT id FROM clips WHERE pinned = 0 ORDER BY created_at DESC LIMIT ?1"
            "  );";

        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db_handle, sql_find_excess, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, max_items);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *fp = (const char *)sqlite3_column_text(stmt, 0);
                if (fp && fp[0] != '\0') unlink(fp);
            }
            sqlite3_finalize(stmt);
        }

        const char *sql_del_excess =
            "DELETE FROM clips WHERE pinned = 0 AND id NOT IN ("
            "  SELECT id FROM clips WHERE pinned = 0 ORDER BY created_at DESC LIMIT ?1"
            ");";

        if (sqlite3_prepare_v2(db_handle, sql_del_excess, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, max_items);
            sqlite3_step(stmt);
            pruned_count += sqlite3_changes(db_handle);
            sqlite3_finalize(stmt);
        }
    }

    return pruned_count;
}

char *db_get_last_content_hash(void)
{
    if (!db_handle && !db_init()) return NULL;

    const char *sql = "SELECT content_hash FROM clips ORDER BY id DESC LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }

    char *hash = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *h = (const char *)sqlite3_column_text(stmt, 0);
        if (h) hash = g_strdup(h);
    }

    sqlite3_finalize(stmt);
    return hash;
}

int db_get_clip_counts(int *out_pinned_count)
{
    if (!db_handle && !db_init()) {
        if (out_pinned_count) *out_pinned_count = 0;
        return 0;
    }

    int total = 0;
    int pinned = 0;

    const char *sql = "SELECT COUNT(*), SUM(CASE WHEN pinned = 1 THEN 1 ELSE 0 END) FROM clips;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total = sqlite3_column_int(stmt, 0);
            pinned = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    if (out_pinned_count) *out_pinned_count = pinned;
    return total;
}

/* =========================================================================
 * System Cleaner: Uninstalled Apps Record Management
 * ========================================================================= */

UninstalledAppRecord *uninstalled_app_record_new(gint64 id,
                                                const char *package_name,
                                                const char *display_name,
                                                const char *uninstalled_at,
                                                int leftovers_cleaned)
{
    UninstalledAppRecord *rec = g_new0(UninstalledAppRecord, 1);
    rec->id = id;
    rec->package_name = g_strdup(package_name ? package_name : "");
    rec->display_name = g_strdup(display_name && *display_name ? display_name : rec->package_name);
    rec->uninstalled_at = g_strdup(uninstalled_at ? uninstalled_at : "");
    rec->leftovers_cleaned = leftovers_cleaned;
    return rec;
}

void uninstalled_app_record_free(UninstalledAppRecord *rec)
{
    if (!rec) return;
    g_free(rec->package_name);
    g_free(rec->display_name);
    g_free(rec->uninstalled_at);
    g_free(rec);
}

void uninstalled_app_record_list_free(GList *list)
{
    g_list_free_full(list, (GDestroyNotify)uninstalled_app_record_free);
}

gint64 db_insert_uninstalled_app(const char *package_name, const char *display_name)
{
    if (!package_name || *package_name == '\0') return -1;
    if (!db_handle && !db_init()) return -1;

    const char *sql =
        "INSERT INTO uninstalled_apps (package_name, display_name, uninstalled_at, leftovers_cleaned) "
        "VALUES (?1, ?2, CURRENT_TIMESTAMP, 0);";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("Failed to prepare insert uninstalled app: %s", sqlite3_errmsg(db_handle));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, package_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, display_name && *display_name ? display_name : package_name, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        g_warning("Failed to execute insert uninstalled app: %s", sqlite3_errmsg(db_handle));
        return -1;
    }

    return (gint64)sqlite3_last_insert_rowid(db_handle);
}

GList *db_get_uncleaned_uninstalled_apps(void)
{
    if (!db_handle && !db_init()) return NULL;

    const char *sql =
        "SELECT id, package_name, display_name, uninstalled_at, leftovers_cleaned "
        "FROM uninstalled_apps WHERE leftovers_cleaned = 0 "
        "ORDER BY uninstalled_at DESC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        g_warning("Failed to query uncleaned apps: %s", sqlite3_errmsg(db_handle));
        return NULL;
    }

    GList *list = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        gint64 id = sqlite3_column_int64(stmt, 0);
        const char *pkg = (const char *)sqlite3_column_text(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        const char *date = (const char *)sqlite3_column_text(stmt, 3);
        int cleaned = sqlite3_column_int(stmt, 4);

        UninstalledAppRecord *rec = uninstalled_app_record_new(id, pkg, name, date, cleaned);
        list = g_list_append(list, rec);
    }

    sqlite3_finalize(stmt);
    return list;
}

GList *db_get_all_uninstalled_apps(void)
{
    if (!db_handle && !db_init()) return NULL;

    const char *sql =
        "SELECT id, package_name, display_name, uninstalled_at, leftovers_cleaned "
        "FROM uninstalled_apps ORDER BY uninstalled_at DESC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        g_warning("Failed to query all uninstalled apps: %s", sqlite3_errmsg(db_handle));
        return NULL;
    }

    GList *list = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        gint64 id = sqlite3_column_int64(stmt, 0);
        const char *pkg = (const char *)sqlite3_column_text(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        const char *date = (const char *)sqlite3_column_text(stmt, 3);
        int cleaned = sqlite3_column_int(stmt, 4);

        UninstalledAppRecord *rec = uninstalled_app_record_new(id, pkg, name, date, cleaned);
        list = g_list_append(list, rec);
    }

    sqlite3_finalize(stmt);
    return list;
}

gboolean db_mark_uninstalled_app_cleaned(gint64 id, const char *package_name)
{
    if (!db_handle && !db_init()) return FALSE;

    char *sql = NULL;
    if (id > 0) {
        sql = sqlite3_mprintf("UPDATE uninstalled_apps SET leftovers_cleaned = 1 WHERE id = %lld;", (long long)id);
    } else if (package_name && *package_name) {
        sql = sqlite3_mprintf("UPDATE uninstalled_apps SET leftovers_cleaned = 1 WHERE package_name = %Q;", package_name);
    } else {
        return FALSE;
    }

    char *err_msg = NULL;
    int rc = sqlite3_exec(db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free(sql);

    if (rc != SQLITE_OK) {
        g_warning("Failed to mark uninstalled app cleaned: %s", err_msg ? err_msg : "unknown");
        sqlite3_free(err_msg);
        return FALSE;
    }

    return TRUE;
}

gboolean db_delete_uninstalled_app(gint64 id)
{
    if (id <= 0) return FALSE;
    if (!db_handle && !db_init()) return FALSE;

    char *sql = sqlite3_mprintf("DELETE FROM uninstalled_apps WHERE id = %lld;", (long long)id);
    char *err_msg = NULL;
    int rc = sqlite3_exec(db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free(sql);

    if (rc != SQLITE_OK) {
        g_warning("Failed to delete uninstalled app record: %s", err_msg ? err_msg : "unknown");
        sqlite3_free(err_msg);
        return FALSE;
    }

    return TRUE;
}

