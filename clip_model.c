/**
 * AppClip Manager - Clipboard History Module
 * Model implementation, hashing, and relative time formatting.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "clip_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ClipEntry *clip_entry_new(void)
{
    ClipEntry *entry = g_new0(ClipEntry, 1);
    entry->id = 0;
    entry->type = CLIP_TYPE_TEXT;
    entry->content = NULL;
    entry->file_path = NULL;
    entry->preview = NULL;
    entry->content_hash = NULL;
    entry->created_at = time(NULL);
    entry->pinned = FALSE;
    entry->image_width = 0;
    entry->image_height = 0;
    entry->file_size_bytes = 0;
    return entry;
}

void clip_entry_free(ClipEntry *entry)
{
    if (!entry) return;

    if (entry->content) {
        g_free(entry->content);
        entry->content = NULL;
    }
    if (entry->file_path) {
        g_free(entry->file_path);
        entry->file_path = NULL;
    }
    if (entry->preview) {
        g_free(entry->preview);
        entry->preview = NULL;
    }
    if (entry->content_hash) {
        g_free(entry->content_hash);
        entry->content_hash = NULL;
    }

    g_free(entry);
}

void clip_entry_list_free(GList *list)
{
    if (!list) return;
    g_list_free_full(list, (GDestroyNotify)clip_entry_free);
}

char *clip_compute_text_hash(const char *text)
{
    if (!text) return g_strdup("");

    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, (const guchar *)"TEXT:", 5);
    g_checksum_update(checksum, (const guchar *)text, strlen(text));
    char *hash = g_strdup(g_checksum_get_string(checksum));
    g_checksum_free(checksum);

    return hash;
}

char *clip_compute_pixbuf_hash(GdkPixbuf *pixbuf)
{
    if (!pixbuf) return g_strdup("");

    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, (const guchar *)"IMG:", 4);

    char dim_buf[64];
    snprintf(dim_buf, sizeof(dim_buf), "%dx%dx%d", width, height, n_channels);
    g_checksum_update(checksum, (const guchar *)dim_buf, strlen(dim_buf));

    /* Hash pixel buffer data directly */
    gsize data_len = (gsize)rowstride * (gsize)height;
    if (pixels && data_len > 0) {
        g_checksum_update(checksum, pixels, data_len);
    }

    char *hash = g_strdup(g_checksum_get_string(checksum));
    g_checksum_free(checksum);

    return hash;
}

char *clip_generate_text_preview(const char *text, int max_chars)
{
    if (!text) return g_strdup("");
    if (max_chars <= 0) max_chars = 100;

    /* Normalize whitespace (replace tabs/newlines with spaces for clean single-line preview) */
    GString *preview_str = g_string_new(NULL);
    int char_count = 0;
    const char *p = text;

    while (*p && char_count < max_chars) {
        gunichar c = g_utf8_get_char(p);
        if (c == '\n' || c == '\r' || c == '\t') {
            g_string_append_c(preview_str, ' ');
        } else {
            g_string_append_unichar(preview_str, c);
        }
        char_count++;
        p = g_utf8_next_char(p);
    }

    /* If original text has more characters, append ellipsis */
    if (*p) {
        g_string_append(preview_str, "...");
    }

    return g_string_free(preview_str, FALSE);
}

char *clip_format_relative_time(time_t created_at)
{
    time_t now = time(NULL);
    double diff = difftime(now, created_at);

    if (diff < 0) diff = 0;

    if (diff < 10) {
        return g_strdup("Just now");
    } else if (diff < 60) {
        return g_strdup_printf("%d seconds ago", (int)diff);
    } else if (diff < 120) {
        return g_strdup("1 minute ago");
    } else if (diff < 3600) {
        return g_strdup_printf("%d minutes ago", (int)(diff / 60));
    } else if (diff < 7200) {
        return g_strdup("1 hour ago");
    } else if (diff < 86400) {
        return g_strdup_printf("%d hours ago", (int)(diff / 3600));
    } else if (diff < 172800) {
        return g_strdup("Yesterday");
    } else if (diff < 604800) { /* 7 days */
        int days = (int)(diff / 86400);
        return g_strdup_printf("%d days ago", days);
    } else {
        /* Older than 7 days: format as full date (e.g., "Aug 14, 2026") */
        struct tm tm_info;
        localtime_r(&created_at, &tm_info);
        char date_buf[64];
        strftime(date_buf, sizeof(date_buf), "%b %d, %Y", &tm_info);
        return g_strdup(date_buf);
    }
}
