/**
 * AppClip Manager - SQLite3 Storage Layer
 * Header for database operations, prepared statements, and clip persistence.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef DB_H
#define DB_H

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "clip_model.h"

/**
 * Initializes the SQLite3 database connection and creates required tables/indexes.
 * Creates ~/.local/share/appclip-manager/ and clips/ directory if needed.
 * Returns TRUE on success, FALSE otherwise.
 */
gboolean db_init(void);

/**
 * Closes the SQLite3 database connection.
 */
void db_close(void);

/**
 * Returns the absolute directory path where PNG clips are stored:
 * ~/.local/share/appclip-manager/clips/
 * Caller must free returned string with g_free().
 */
char *db_get_clips_dir(void);

/**
 * Saves a GdkPixbuf into ~/.local/share/appclip-manager/clips/clip_<timestamp>_<uniq>.png
 * Returns the newly allocated absolute file path or NULL on error.
 */
char *db_save_pixbuf_to_file(GdkPixbuf *pixbuf);

/**
 * Inserts a new text clip.
 * Returns the newly inserted row ID, or -1 on error.
 */
int db_insert_text_clip(const char *content, const char *preview, const char *content_hash);

/**
 * Inserts a new image clip with its saved PNG file path.
 * Returns the newly inserted row ID, or -1 on error.
 */
int db_insert_image_clip(const char *file_path, const char *content_hash);

/**
 * Retrieves the most recent clips, ordered with pinned items first, then created_at DESC.
 * If search_query is provided (non-NULL and non-empty), filters text clips by content substring.
 * Returns a GList of newly allocated ClipEntry* (free with clip_entry_list_free).
 */
GList *db_get_recent_clips(int limit, const char *search_query);

/**
 * Retrieves a single clip by its SQLite ID.
 * Returns ClipEntry* or NULL if not found.
 */
ClipEntry *db_get_clip_by_id(int id);

/**
 * Deletes a clip by ID. If it is an image clip, unlinks the associated PNG file on disk.
 * Returns TRUE on success, FALSE otherwise.
 */
gboolean db_delete_clip(int id);

/**
 * Toggles the pinned state of a clip (0 -> 1 or 1 -> 0).
 * If out_new_pinned is non-NULL, writes the new pinned state.
 * Returns TRUE on success, FALSE otherwise.
 */
gboolean db_toggle_pin(int id, gboolean *out_new_pinned);

/**
 * Clears history. If keep_pinned is TRUE, only non-pinned clips are deleted.
 * All associated PNG files for deleted clips are unlinked from disk.
 * Returns TRUE on success.
 */
gboolean db_clear_history(gboolean keep_pinned);

/**
 * Prunes clips based on max count and age in days.
 * Never deletes pinned clips.
 * Deletes unneeded PNG files from disk.
 * Returns the number of pruned rows.
 */
int db_prune_old_clips(int max_items, int max_days);

/**
 * Retrieves the content_hash of the most recently inserted clip.
 * Used for fast deduplication check against consecutive copies.
 * Caller must free returned string with g_free().
 */
char *db_get_last_content_hash(void);

/**
 * Returns total count of clips in database.
 * If out_pinned_count is non-NULL, stores count of pinned clips.
 */
int db_get_clip_counts(int *out_pinned_count);

/**
 * Retrieves the complete, untruncated content of a text clip by its ID.
 * Caller must free returned string with g_free().
 */
char *db_get_clip_full_content(int clip_id);

/**
 * Retrieves the stored image file path of an image clip by its ID.
 * Caller must free returned string with g_free().
 */
char *db_get_clip_file_path(int clip_id);

/**
 * Updates the content of a text clip by its ID, regenerates its preview column
 * (first 100 characters), and updates its content hash.
 * Returns TRUE on success, FALSE otherwise.
 */
gboolean db_update_clip_content(int clip_id, const char *new_content);

/* -------------------------------------------------------------------------
 * System Cleaner: Uninstalled Apps History Tracking
 * ------------------------------------------------------------------------- */

typedef struct {
    gint64 id;
    char *package_name;
    char *display_name;
    char *uninstalled_at;
    int leftovers_cleaned;
} UninstalledAppRecord;

/**
 * Allocates a new UninstalledAppRecord.
 */
UninstalledAppRecord *uninstalled_app_record_new(gint64 id,
                                                const char *package_name,
                                                const char *display_name,
                                                const char *uninstalled_at,
                                                int leftovers_cleaned);

/**
 * Frees an UninstalledAppRecord.
 */
void uninstalled_app_record_free(UninstalledAppRecord *rec);

/**
 * Frees a GList of UninstalledAppRecord pointers.
 */
void uninstalled_app_record_list_free(GList *list);

/**
 * Inserts a newly uninstalled application into the uninstalled_apps table.
 * Returns the inserted row ID, or -1 on error.
 */
gint64 db_insert_uninstalled_app(const char *package_name, const char *display_name);

/**
 * Retrieves uninstalled apps whose leftover files haven't been fully cleaned (leftovers_cleaned = 0).
 * Ordered by uninstalled_at DESC.
 * Returns a GList of UninstalledAppRecord* (free with uninstalled_app_record_list_free).
 */
GList *db_get_uncleaned_uninstalled_apps(void);

/**
 * Retrieves all uninstalled app records for history listing.
 * Returns a GList of UninstalledAppRecord* (free with uninstalled_app_record_list_free).
 */
GList *db_get_all_uninstalled_apps(void);

/**
 * Marks uninstalled application leftovers as cleaned (leftovers_cleaned = 1).
 * Can be targeted by record id (if id > 0) or by package_name (if package_name != NULL).
 * Returns TRUE on success.
 */
gboolean db_mark_uninstalled_app_cleaned(gint64 id, const char *package_name);

/**
 * Deletes an uninstalled application record from SQLite history.
 */
gboolean db_delete_uninstalled_app(gint64 id);

#endif /* DB_H */
