/**
 * Test suite for AppClip Manager - Clipboard History and SQLite Module
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "db.h"
#include "clip_model.h"
#include "settings.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    g_print("== Running AppClip Manager Clipboard Test Suite ==\n");

    /* 1. Test Settings */
    AppSettings settings;
    settings_get_defaults(&settings);
    assert(settings.max_history_items == 500);
    assert(settings.max_history_days == 30);
    assert(settings.monitor_text == TRUE);
    assert(settings.monitor_images == TRUE);
    g_print("[PASS] Settings defaults verified\n");

    /* 2. Test DB Initialization */
    gboolean db_ok = db_init();
    assert(db_ok == TRUE);
    g_print("[PASS] SQLite database initialized\n");

    /* 3. Test Preview generation */
    char *preview1 = clip_generate_text_preview("Line 1\nLine 2\twith tab", 100);
    assert(g_strcmp0(preview1, "Line 1 Line 2 with tab") == 0);
    g_free(preview1);

    char *long_str = g_strnfill(150, 'A');
    char *preview2 = clip_generate_text_preview(long_str, 100);
    assert(g_str_has_suffix(preview2, "..."));
    g_free(long_str);
    g_free(preview2);
    g_print("[PASS] Text preview and whitespace normalization verified\n");

    /* 4. Test Text Hashing */
    char *hash1 = clip_compute_text_hash("Hello Jeyaul Hoque");
    char *hash2 = clip_compute_text_hash("Hello Jeyaul Hoque");
    char *hash3 = clip_compute_text_hash("Different Text");
    assert(g_strcmp0(hash1, hash2) == 0);
    assert(g_strcmp0(hash1, hash3) != 0);
    g_free(hash1);
    g_free(hash2);
    g_free(hash3);
    g_print("[PASS] SHA-256 Text Hashing verified\n");

    /* 5. Test Insertion of Text Clips */
    char *h_sample1 = clip_compute_text_hash("https://jeyaulhoque.pages.dev/");
    int id1 = db_insert_text_clip("https://jeyaulhoque.pages.dev/", "https://jeyaulhoque.pages.dev/", h_sample1);
    assert(id1 > 0);
    g_free(h_sample1);

    char *h_sample2 = clip_compute_text_hash("sudo apt install wl-clipboard");
    int id2 = db_insert_text_clip("sudo apt install wl-clipboard", "sudo apt install wl-clipboard", h_sample2);
    assert(id2 > 0);
    g_free(h_sample2);
    g_print("[PASS] Text clip insertion with prepared statements verified (IDs %d, %d)\n", id1, id2);

    /* 6. Test Relative Time formatting */
    char *rel_now = clip_format_relative_time(time(NULL));
    assert(g_strcmp0(rel_now, "Just now") == 0);
    g_free(rel_now);

    char *rel_past = clip_format_relative_time(time(NULL) - 300);
    assert(g_strcmp0(rel_past, "5 minutes ago") == 0);
    g_free(rel_past);
    g_print("[PASS] Relative time formatting verified\n");

    /* 7. Test Pinning */
    gboolean pinned_state = FALSE;
    gboolean pin_toggled = db_toggle_pin(id1, &pinned_state);
    assert(pin_toggled == TRUE);
    assert(pinned_state == TRUE);

    GList *clips = db_get_recent_clips(10, NULL);
    assert(clips != NULL);
    ClipEntry *first = (ClipEntry *)clips->data;
    /* The pinned item must come first */
    assert(first->id == id1);
    assert(first->pinned == TRUE);
    clip_entry_list_free(clips);
    g_print("[PASS] Pinned clip sorting verified (pinned clip floats to top)\n");

    /* 8. Test Search Filter */
    GList *search_res = db_get_recent_clips(10, "jeyaulhoque");
    assert(search_res != NULL);
    assert(g_list_length(search_res) >= 1);
    clip_entry_list_free(search_res);

    GList *search_none = db_get_recent_clips(10, "non_existent_random_phrase_123");
    assert(search_none == NULL);
    g_print("[PASS] Substring search filter verified\n");

    /* 9. Test Clear History keeping pinned */
    db_clear_history(TRUE); /* Keep pinned */
    int pinned_count = 0;
    int total_after_clear = db_get_clip_counts(&pinned_count);
    assert(total_after_clear == 1);
    assert(pinned_count == 1);
    g_print("[PASS] Clear history preserving pinned clips verified (retained %d pinned clip)\n", pinned_count);

    /* 10. Test Single Clip Deletion */
    gboolean del_ok = db_delete_clip(id1);
    assert(del_ok == TRUE);
    int total_after_del = db_get_clip_counts(NULL);
    assert(total_after_del == 0);
    g_print("[PASS] Single clip deletion verified\n");

    db_close();
    g_print("\nALL 10 TESTS PASSED SUCCESSFULLY! ✓\n");
    return 0;
}
