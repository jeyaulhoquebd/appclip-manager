/**
 * AppClip Manager - Clipboard History Module
 * Model definitions, structures, and helper functions for clipboard entries.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#ifndef CLIP_MODEL_H
#define CLIP_MODEL_H

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <time.h>

/**
 * Type of clipboard payload.
 */
typedef enum {
    CLIP_TYPE_TEXT = 0,
    CLIP_TYPE_IMAGE = 1
} ClipType;

/**
 * Core Clipboard Entry representation matching SQLite schema.
 */
typedef struct {
    int id;                 /* SQLite primary key ID */
    ClipType type;          /* CLIP_TYPE_TEXT or CLIP_TYPE_IMAGE */
    char *content;          /* Full text content if type == CLIP_TYPE_TEXT, NULL for image */
    char *file_path;        /* Path to saved PNG if type == CLIP_TYPE_IMAGE, NULL for text */
    char *preview;          /* First ~100 characters for text preview, NULL for image */
    char *content_hash;     /* SHA-like or checksum hash for deduplication */
    time_t created_at;      /* Unix epoch timestamp */
    gboolean pinned;        /* 1 if pinned to top, 0 otherwise */

    /* Image dimensions metadata (derived from pixbuf or file) */
    int image_width;
    int image_height;
    gint64 file_size_bytes;
} ClipEntry;

/**
 * Allocates and initializes a new empty ClipEntry.
 */
ClipEntry *clip_entry_new(void);

/**
 * Frees a ClipEntry and all internal heap-allocated strings.
 */
void clip_entry_free(ClipEntry *entry);

/**
 * Frees a GList of ClipEntry pointers.
 */
void clip_entry_list_free(GList *list);

/**
 * Computes a simple SHA-256 or MD5/checksum hex hash for a UTF-8 text string.
 * Returns a newly allocated string that must be freed with g_free().
 */
char *clip_compute_text_hash(const char *text);

/**
 * Computes a hash for image pixel data to prevent duplicate image saves.
 * Returns a newly allocated string that must be freed with g_free().
 */
char *clip_compute_pixbuf_hash(GdkPixbuf *pixbuf);

/**
 * Truncates text up to max_chars and appends "..." if longer.
 * Returns a newly allocated string that must be freed with g_free().
 */
char *clip_generate_text_preview(const char *text, int max_chars);

/**
 * Formats a relative timestamp (e.g., "Just now", "2 minutes ago", "Yesterday",
 * or "Aug 14, 2026" if > 7 days) using difftime().
 * Returns a newly allocated string that must be freed with g_free().
 */
char *clip_format_relative_time(time_t created_at);

#endif /* CLIP_MODEL_H */
