/**
 * AppClip Manager - Clip Detail Viewer Implementation
 * Modal dialog for inspecting, editing, zooming, and exporting full text and image clips.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "ui_clip_detail.h"
#include "db.h"
#include "clip_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gdk/gdkkeysyms.h>

/* -----------------------------------------------------------------------------
 * Helper Utilities: Code Heuristics, Statistics, and Timestamps
 * -------------------------------------------------------------------------- */

/**
 * Heuristic to detect whether text content represents source code or structured syntax.
 * Checks for standard language tokens, operators, tags, or consistent line indentation.
 */
static gboolean is_likely_code(const char *text)
{
    if (!text || text[0] == '\0') return FALSE;

    /* Check for common code keywords and syntax patterns */
    const char *code_tokens[] = {
        "{", "}", ";", "#include", "#define", "function", "def ",
        "class ", "const ", "let ", "var ", "import ", "export ",
        "return ", "public ", "private ", "protected ", "void ",
        "int ", "char ", "float ", "double ", "SELECT ", "FROM ",
        "WHERE ", "UPDATE ", "INSERT ", "DELETE ", "CREATE ",
        "#!/bin/", "<html>", "</div>", "<?php", "npm ", "git ",
        "//", "/*", "*/", "->", "=>", "==", "!=", "print(",
        "console.log", "printf(", "std::", "fn ", "pub fn",
        NULL
    };

    for (int i = 0; code_tokens[i] != NULL; i++) {
        if (strstr(text, code_tokens[i]) != NULL) {
            return TRUE;
        }
    }

    /* Check for consistent indentation (lines starting with 2+ spaces or tabs) */
    int indented_lines = 0;
    int total_lines = 0;
    const char *line = text;
    while (*line) {
        if (*line == '\t' || (*line == ' ' && *(line + 1) == ' ')) {
            indented_lines++;
        }
        total_lines++;
        const char *next = strchr(line, '\n');
        if (!next) break;
        line = next + 1;
    }

    if (total_lines >= 3 && indented_lines >= 2) {
        return TRUE;
    }

    return FALSE;
}

/**
 * Computes character count, word count, and line count for a UTF-8 string.
 */
static void count_text_stats(const char *text, gsize *out_chars, gsize *out_words, gsize *out_lines)
{
    if (!text) {
        if (out_chars) *out_chars = 0;
        if (out_words) *out_words = 0;
        if (out_lines) *out_lines = 0;
        return;
    }

    if (out_chars) {
        *out_chars = (gsize)g_utf8_strlen(text, -1);
    }

    gsize words = 0;
    gsize lines = (text[0] != '\0') ? 1 : 0;
    gboolean in_word = FALSE;

    for (const char *p = text; *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (c == '\n') {
            lines++;
        }
        if (g_unichar_isspace(c)) {
            in_word = FALSE;
        } else {
            if (!in_word) {
                words++;
                in_word = TRUE;
            }
        }
    }

    if (out_words) *out_words = words;
    if (out_lines) *out_lines = lines;
}

/**
 * Formats epoch timestamp into a full date and time representation.
 */
static char *format_full_datetime(time_t epoch)
{
    char buf[128];
    struct tm *tm_info = localtime(&epoch);
    if (tm_info) {
        strftime(buf, sizeof(buf), "%B %d, %Y at %H:%M:%S", tm_info);
    } else {
        g_strlcpy(buf, "Unknown time", sizeof(buf));
    }
    return g_strdup(buf);
}

/* -----------------------------------------------------------------------------
 * A) TEXT CLIP DETAIL DIALOG
 * -------------------------------------------------------------------------- */

typedef struct {
    GtkDialog *dialog;
    GtkWindow *parent;
    int clip_id;
    time_t created_at;

    GtkTextView *text_view;
    GtkTextBuffer *text_buffer;
    GtkWidget *lbl_header_stats;
    GtkWidget *lbl_status_msg;

    GtkWidget *btn_edit_toggle;
    GtkWidget *btn_save_changes;

    char *original_content;
    gboolean is_editable;
    gboolean is_modified;
    gboolean db_was_updated;
    guint status_clear_timeout_id;
} TextDetailContext;

static void text_ctx_set_status(TextDetailContext *ctx, const char *msg)
{
    if (!ctx || !ctx->lbl_status_msg) return;
    gtk_label_set_text(GTK_LABEL(ctx->lbl_status_msg), msg ? msg : "");
}

static void update_text_header_stats(TextDetailContext *ctx)
{
    if (!ctx || !ctx->lbl_header_stats || !ctx->text_buffer) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(ctx->text_buffer, &start, &end);
    char *current_text = gtk_text_buffer_get_text(ctx->text_buffer, &start, &end, FALSE);

    gsize chars = 0, words = 0, lines = 0;
    count_text_stats(current_text, &chars, &words, &lines);
    g_free(current_text);

    char *time_str = format_full_datetime(ctx->created_at);
    char *stats_str = g_strdup_printf(
        "Recorded: %s   •   %lu characters   •   %lu words   •   %lu lines",
        time_str, (unsigned long)chars, (unsigned long)words, (unsigned long)lines
    );
    gtk_label_set_text(GTK_LABEL(ctx->lbl_header_stats), stats_str);
    g_free(stats_str);
    g_free(time_str);
}

static void on_text_buffer_changed(GtkTextBuffer *buffer, gpointer user_data)
{
    TextDetailContext *ctx = (TextDetailContext *)user_data;
    if (!ctx) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *current_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    ctx->is_modified = (g_strcmp0(current_text, ctx->original_content) != 0);
    g_free(current_text);

    update_text_header_stats(ctx);

    if (ctx->is_modified && ctx->is_editable) {
        text_ctx_set_status(ctx, "Unsaved changes");
        gtk_widget_set_sensitive(ctx->btn_save_changes, TRUE);
    } else {
        gtk_widget_set_sensitive(ctx->btn_save_changes, FALSE);
    }
}

static void copy_text_to_clipboard(TextDetailContext *ctx)
{
    if (!ctx || !ctx->text_buffer) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(ctx->text_buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(ctx->text_buffer, &start, &end, FALSE);

    GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(cb, text ? text : "", -1);
    gtk_clipboard_store(cb);
    g_free(text);

    text_ctx_set_status(ctx, "Copied full text to clipboard");
}

static void on_btn_copy_text_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    copy_text_to_clipboard((TextDetailContext *)user_data);
}

static void save_text_changes(TextDetailContext *ctx)
{
    if (!ctx || !ctx->text_buffer) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(ctx->text_buffer, &start, &end);
    char *new_text = gtk_text_buffer_get_text(ctx->text_buffer, &start, &end, FALSE);

    if (db_update_clip_content(ctx->clip_id, new_text ? new_text : "")) {
        g_free(ctx->original_content);
        ctx->original_content = g_strdup(new_text ? new_text : "");
        ctx->is_modified = FALSE;
        ctx->db_was_updated = TRUE;
        gtk_widget_set_sensitive(ctx->btn_save_changes, FALSE);
        text_ctx_set_status(ctx, "Changes saved to database");
    } else {
        text_ctx_set_status(ctx, "Error: Failed to save changes to database");
    }

    g_free(new_text);
}

static void on_btn_save_changes_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    save_text_changes((TextDetailContext *)user_data);
}

static void on_btn_edit_toggled(GtkToggleButton *toggle, gpointer user_data)
{
    TextDetailContext *ctx = (TextDetailContext *)user_data;
    if (!ctx) return;

    gboolean active = gtk_toggle_button_get_active(toggle);
    ctx->is_editable = active;
    gtk_text_view_set_editable(ctx->text_view, active);
    gtk_text_view_set_cursor_visible(ctx->text_view, active);

    if (active) {
        gtk_widget_show(ctx->btn_save_changes);
        gtk_widget_set_sensitive(ctx->btn_save_changes, ctx->is_modified);
        text_ctx_set_status(ctx, "Editing enabled");
        gtk_widget_grab_focus(GTK_WIDGET(ctx->text_view));
    } else {
        gtk_widget_hide(ctx->btn_save_changes);
        text_ctx_set_status(ctx, "Read-only mode");
    }
}

static void save_text_as_file(TextDetailContext *ctx)
{
    if (!ctx || !ctx->text_buffer) return;

    GtkWidget *chooser = gtk_file_chooser_dialog_new(
        "Save Text Clip As",
        GTK_WINDOW(ctx->dialog),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), "clip_export.txt");

    /* Add text filter */
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Text files (*.txt)");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

    GtkFileFilter *all_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(all_filter, "All files");
    gtk_file_filter_add_pattern(all_filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), all_filter);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        if (filename) {
            GtkTextIter start, end;
            gtk_text_buffer_get_bounds(ctx->text_buffer, &start, &end);
            char *text = gtk_text_buffer_get_text(ctx->text_buffer, &start, &end, FALSE);

            GError *err = NULL;
            if (g_file_set_contents(filename, text ? text : "", -1, &err)) {
                text_ctx_set_status(ctx, "Exported file successfully");
            } else {
                GtkWidget *err_dialog = gtk_message_dialog_new(
                    GTK_WINDOW(ctx->dialog),
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_CLOSE,
                    "Failed to save file:\n%s",
                    err ? err->message : "Unknown error"
                );
                gtk_dialog_run(GTK_DIALOG(err_dialog));
                gtk_widget_destroy(err_dialog);
                if (err) g_error_free(err);
            }

            g_free(text);
            g_free(filename);
        }
    }

    gtk_widget_destroy(chooser);
}

static void on_btn_save_as_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    save_text_as_file((TextDetailContext *)user_data);
}

/**
 * Prompts the user before discarding unsaved edits.
 * Returns TRUE if safe to close, FALSE if the user chose to keep editing.
 */
static gboolean confirm_discard_if_modified(TextDetailContext *ctx)
{
    if (!ctx || !ctx->is_modified) return TRUE;

    GtkWidget *confirm = gtk_message_dialog_new(
        GTK_WINDOW(ctx->dialog),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE,
        "You have unsaved changes. Discard them?"
    );
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(confirm),
        "If you close without saving, your modifications to this clip will be lost."
    );

    gtk_dialog_add_button(GTK_DIALOG(confirm), "_Keep Editing", GTK_RESPONSE_CANCEL);
    GtkWidget *btn_discard = gtk_dialog_add_button(GTK_DIALOG(confirm), "_Discard Changes", GTK_RESPONSE_ACCEPT);
    GtkStyleContext *st = gtk_widget_get_style_context(btn_discard);
    gtk_style_context_add_class(st, "destructive-action");

    int resp = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);

    return (resp == GTK_RESPONSE_ACCEPT);
}

static gboolean on_text_dialog_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    (void)widget;
    (void)event;
    TextDetailContext *ctx = (TextDetailContext *)user_data;
    /* If discard confirmed, let dialog close (return FALSE), else stop (return TRUE) */
    return !confirm_discard_if_modified(ctx);
}

static gboolean on_text_dialog_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    (void)widget;
    TextDetailContext *ctx = (TextDetailContext *)user_data;
    guint state = event->state & gtk_accelerator_get_default_mod_mask();

    if (state == GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_c || event->keyval == GDK_KEY_C) {
            /* If no active text selection in GtkTextView, copy entire full clip */
            GtkTextIter sel_start, sel_end;
            if (!gtk_text_buffer_get_selection_bounds(ctx->text_buffer, &sel_start, &sel_end)) {
                copy_text_to_clipboard(ctx);
                return GDK_EVENT_STOP;
            }
            return GDK_EVENT_PROPAGATE;
        } else if (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S) {
            if (ctx->is_editable && ctx->is_modified) {
                save_text_changes(ctx);
            } else {
                save_text_as_file(ctx);
            }
            return GDK_EVENT_STOP;
        }
    } else if (state == 0) {
        if (event->keyval == GDK_KEY_Escape) {
            if (confirm_discard_if_modified(ctx)) {
                gtk_dialog_response(ctx->dialog, GTK_RESPONSE_CLOSE);
            }
            return GDK_EVENT_STOP;
        }
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean run_text_detail_dialog(GtkWindow *parent, int clip_id, ClipEntry *entry)
{
    char *full_content = db_get_clip_full_content(clip_id);
    if (!full_content && entry && entry->content) {
        full_content = g_strdup(entry->content);
    }
    if (!full_content) {
        full_content = g_strdup("");
    }

    TextDetailContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.clip_id = clip_id;
    ctx.parent = parent;
    ctx.created_at = entry ? entry->created_at : time(NULL);
    ctx.original_content = full_content;
    ctx.is_editable = FALSE;
    ctx.is_modified = FALSE;
    ctx.db_was_updated = FALSE;

    /* Build Dialog (first_button_text = NULL, followed by terminating sentinel NULL) */
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Clip Details - Text",
        parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        NULL,
        NULL
    );
    ctx.dialog = GTK_DIALOG(dialog);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 520);
    gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);

    GtkWidget *content_area = gtk_dialog_get_content_area(ctx.dialog);
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 12);
    gtk_box_set_spacing(GTK_BOX(content_area), 8);

    /* 1. Header Box: Stats and Timestamp */
    GtkWidget *header_frame = gtk_frame_new(NULL);
    GtkStyleContext *hf_style = gtk_widget_get_style_context(header_frame);
    gtk_style_context_add_class(hf_style, "header-stats-frame");

    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(header_box), 8);

    GtkWidget *icon_text = gtk_image_new_from_icon_name("text-x-generic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(header_box), icon_text, FALSE, FALSE, 0);

    ctx.lbl_header_stats = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(ctx.lbl_header_stats), 0.0);
    GtkStyleContext *st_ctx = gtk_widget_get_style_context(ctx.lbl_header_stats);
    gtk_style_context_add_class(st_ctx, "size-label");
    gtk_box_pack_start(GTK_BOX(header_box), ctx.lbl_header_stats, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(header_frame), header_box);
    gtk_box_pack_start(GTK_BOX(content_area), header_frame, FALSE, FALSE, 0);

    /* 2. Main Content: Scrolled GtkTextView */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(content_area), scrolled, TRUE, TRUE, 0);

    ctx.text_view = GTK_TEXT_VIEW(gtk_text_view_new());
    ctx.text_buffer = gtk_text_view_get_buffer(ctx.text_view);
    gtk_text_view_set_editable(ctx.text_view, FALSE);
    gtk_text_view_set_cursor_visible(ctx.text_view, FALSE);
    gtk_text_view_set_wrap_mode(ctx.text_view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(ctx.text_view, 10);
    gtk_text_view_set_right_margin(ctx.text_view, 10);
    gtk_text_view_set_top_margin(ctx.text_view, 10);
    gtk_text_view_set_bottom_margin(ctx.text_view, 10);

    /* Monospace CSS styling if code heuristic matched */
    gboolean code_detected = is_likely_code(full_content);
    GtkCssProvider *css = gtk_css_provider_new();
    if (code_detected) {
        gtk_css_provider_load_from_data(css,
            "textview { font-family: 'DejaVu Sans Mono', 'Fira Code', 'JetBrains Mono', monospace; font-size: 13px; }\n"
            ".header-stats-frame { border-radius: 6px; border: 1px solid alpha(currentColor, 0.15); }\n",
            -1, NULL);
    } else {
        gtk_css_provider_load_from_data(css,
            "textview { font-size: 14px; line-height: 1.5; }\n"
            ".header-stats-frame { border-radius: 6px; border: 1px solid alpha(currentColor, 0.15); }\n",
            -1, NULL);
    }
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(GTK_WIDGET(ctx.text_view)),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(header_frame),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(css);

    gtk_text_buffer_set_text(ctx.text_buffer, full_content, -1);
    update_text_header_stats(&ctx);

    g_signal_connect(ctx.text_buffer, "changed", G_CALLBACK(on_text_buffer_changed), &ctx);
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(ctx.text_view));

    /* 3. Status Bar Box */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    ctx.lbl_status_msg = gtk_label_new(code_detected ? "Detected code syntax (Monospace font)" : "Ready (Read-only)");
    gtk_label_set_xalign(GTK_LABEL(ctx.lbl_status_msg), 0.0);
    GtkStyleContext *st_msg = gtk_widget_get_style_context(ctx.lbl_status_msg);
    gtk_style_context_add_class(st_msg, "size-label");
    gtk_box_pack_start(GTK_BOX(status_bar), ctx.lbl_status_msg, TRUE, TRUE, 0);

    GtkWidget *lbl_shortcuts = gtk_label_new("Ctrl+C: Copy  •  Ctrl+S: Save  •  Esc: Close");
    gtk_label_set_xalign(GTK_LABEL(lbl_shortcuts), 1.0);
    GtkStyleContext *st_sc = gtk_widget_get_style_context(lbl_shortcuts);
    gtk_style_context_add_class(st_sc, "size-label");
    gtk_box_pack_end(GTK_BOX(status_bar), lbl_shortcuts, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content_area), status_bar, FALSE, FALSE, 0);

    /* 4. Bottom Action Buttons Bar */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_set_homogeneous(GTK_BOX(btn_box), FALSE);

    /* Copy to Clipboard Button */
    GtkWidget *btn_copy = gtk_button_new_with_label("Copy to Clipboard");
    gtk_button_set_image(GTK_BUTTON(btn_copy), gtk_image_new_from_icon_name("edit-copy-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(btn_copy), TRUE);
    g_signal_connect(btn_copy, "clicked", G_CALLBACK(on_btn_copy_text_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_copy, FALSE, FALSE, 0);

    /* Edit Toggle Button */
    ctx.btn_edit_toggle = gtk_toggle_button_new_with_label("Edit");
    gtk_button_set_image(GTK_BUTTON(ctx.btn_edit_toggle), gtk_image_new_from_icon_name("document-edit-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(ctx.btn_edit_toggle), TRUE);
    g_signal_connect(ctx.btn_edit_toggle, "toggled", G_CALLBACK(on_btn_edit_toggled), &ctx);
    gtk_box_pack_start(GTK_BOX(btn_box), ctx.btn_edit_toggle, FALSE, FALSE, 0);

    /* Save Changes Button (Revealed in edit mode) */
    ctx.btn_save_changes = gtk_button_new_with_label("Save Changes");
    gtk_button_set_image(GTK_BUTTON(ctx.btn_save_changes), gtk_image_new_from_icon_name("document-save-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(ctx.btn_save_changes), TRUE);
    GtkStyleContext *sc_save = gtk_widget_get_style_context(ctx.btn_save_changes);
    gtk_style_context_add_class(sc_save, "suggested-action");
    gtk_widget_set_no_show_all(ctx.btn_save_changes, TRUE);
    gtk_widget_set_sensitive(ctx.btn_save_changes, FALSE);
    g_signal_connect(ctx.btn_save_changes, "clicked", G_CALLBACK(on_btn_save_changes_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(btn_box), ctx.btn_save_changes, FALSE, FALSE, 0);

    /* Save As File... Button */
    GtkWidget *btn_save_as = gtk_button_new_with_label("Save As File...");
    gtk_button_set_image(GTK_BUTTON(btn_save_as), gtk_image_new_from_icon_name("document-save-as-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(btn_save_as), TRUE);
    g_signal_connect(btn_save_as, "clicked", G_CALLBACK(on_btn_save_as_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_save_as, FALSE, FALSE, 0);

    /* Close Button */
    GtkWidget *btn_close = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(btn_close, "clicked", G_CALLBACK(gtk_dialog_response), dialog);
    gtk_box_pack_end(GTK_BOX(btn_box), btn_close, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content_area), btn_box, FALSE, FALSE, 0);

    /* Connect dialog signals */
    g_signal_connect(dialog, "delete-event", G_CALLBACK(on_text_dialog_delete_event), &ctx);
    g_signal_connect(dialog, "key-press-event", G_CALLBACK(on_text_dialog_key_press), &ctx);

    gtk_widget_show_all(dialog);

    /* Run dialog loop with unsaved changes verification */
    while (TRUE) {
        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        (void)response;
        if (confirm_discard_if_modified(&ctx)) {
            break;
        }
    }

    gtk_widget_destroy(dialog);
    g_free(ctx.original_content);

    return ctx.db_was_updated;
}

/* -----------------------------------------------------------------------------
 * B) IMAGE CLIP DETAIL DIALOG
 * -------------------------------------------------------------------------- */

typedef struct {
    GtkDialog *dialog;
    GtkWindow *parent;
    int clip_id;
    char *file_path;
    time_t created_at;

    GdkPixbuf *original_pixbuf;
    double current_zoom; /* 1.0 = 100% */

    GtkWidget *image_widget;
    GtkWidget *scrolled_window;
    GtkWidget *lbl_zoom;
    GtkWidget *lbl_header_stats;
    GtkWidget *lbl_status_msg;
} ImageDetailContext;

static void update_image_zoom(ImageDetailContext *ctx)
{
    if (!ctx || !ctx->original_pixbuf || !ctx->image_widget) return;

    int orig_w = gdk_pixbuf_get_width(ctx->original_pixbuf);
    int orig_h = gdk_pixbuf_get_height(ctx->original_pixbuf);

    int scaled_w = (int)(orig_w * ctx->current_zoom);
    int scaled_h = (int)(orig_h * ctx->current_zoom);
    if (scaled_w < 16) scaled_w = 16;
    if (scaled_h < 16) scaled_h = 16;

    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(
        ctx->original_pixbuf,
        scaled_w,
        scaled_h,
        (ctx->current_zoom <= 1.0) ? GDK_INTERP_BILINEAR : GDK_INTERP_NEAREST
    );

    if (scaled) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(ctx->image_widget), scaled);
        g_object_unref(scaled);
    }

    if (ctx->lbl_zoom) {
        char zoom_text[32];
        snprintf(zoom_text, sizeof(zoom_text), "%d%%", (int)(ctx->current_zoom * 100.0 + 0.5));
        gtk_label_set_text(GTK_LABEL(ctx->lbl_zoom), zoom_text);
    }
}

static void on_btn_zoom_in_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ImageDetailContext *ctx = (ImageDetailContext *)user_data;
    if (!ctx) return;

    if (ctx->current_zoom < 5.0) {
        ctx->current_zoom = MIN(5.0, ctx->current_zoom + 0.25);
        update_image_zoom(ctx);
    }
}

static void on_btn_zoom_out_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ImageDetailContext *ctx = (ImageDetailContext *)user_data;
    if (!ctx) return;

    if (ctx->current_zoom > 0.15) {
        ctx->current_zoom = MAX(0.10, ctx->current_zoom - 0.25);
        update_image_zoom(ctx);
    }
}

static void on_btn_zoom_fit_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ImageDetailContext *ctx = (ImageDetailContext *)user_data;
    if (!ctx || !ctx->original_pixbuf || !ctx->scrolled_window) return;

    GtkAllocation alloc;
    gtk_widget_get_allocation(ctx->scrolled_window, &alloc);

    int avail_w = alloc.width - 32;
    int avail_h = alloc.height - 32;
    if (avail_w < 100) avail_w = 400;
    if (avail_h < 100) avail_h = 300;

    int orig_w = gdk_pixbuf_get_width(ctx->original_pixbuf);
    int orig_h = gdk_pixbuf_get_height(ctx->original_pixbuf);

    double scale_w = (double)avail_w / (double)orig_w;
    double scale_h = (double)avail_h / (double)orig_h;
    double fit_zoom = MIN(scale_w, scale_h);

    ctx->current_zoom = CLAMP(fit_zoom, 0.1, 3.0);
    update_image_zoom(ctx);
}

static void on_btn_zoom_original_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    ImageDetailContext *ctx = (ImageDetailContext *)user_data;
    if (!ctx) return;

    ctx->current_zoom = 1.0;
    update_image_zoom(ctx);
}

static void copy_image_to_clipboard(ImageDetailContext *ctx)
{
    if (!ctx || !ctx->original_pixbuf) return;

    GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_image(cb, ctx->original_pixbuf);
    gtk_clipboard_store(cb);

    if (ctx->lbl_status_msg) {
        gtk_label_set_text(GTK_LABEL(ctx->lbl_status_msg), "Copied full image to clipboard");
    }
}

static void on_btn_copy_image_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    copy_image_to_clipboard((ImageDetailContext *)user_data);
}

static void save_image_as_file(ImageDetailContext *ctx)
{
    if (!ctx || !ctx->original_pixbuf) return;

    GtkWidget *chooser = gtk_file_chooser_dialog_new(
        "Save Image Clip As",
        GTK_WINDOW(ctx->dialog),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), "clip_image.png");

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "PNG Images (*.png)");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        if (filename) {
            GError *err = NULL;
            if (gdk_pixbuf_save(ctx->original_pixbuf, filename, "png", &err, NULL)) {
                if (ctx->lbl_status_msg) {
                    gtk_label_set_text(GTK_LABEL(ctx->lbl_status_msg), "Image saved successfully");
                }
            } else {
                GtkWidget *err_dialog = gtk_message_dialog_new(
                    GTK_WINDOW(ctx->dialog),
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_CLOSE,
                    "Failed to save image:\n%s",
                    err ? err->message : "Unknown error"
                );
                gtk_dialog_run(GTK_DIALOG(err_dialog));
                gtk_widget_destroy(err_dialog);
                if (err) g_error_free(err);
            }
            g_free(filename);
        }
    }

    gtk_widget_destroy(chooser);
}

static void on_btn_save_image_as_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    save_image_as_file((ImageDetailContext *)user_data);
}

static void open_image_with_external_viewer(ImageDetailContext *ctx)
{
    if (!ctx || !ctx->file_path) return;

    char *uri = g_filename_to_uri(ctx->file_path, NULL, NULL);
    if (uri) {
        GError *err = NULL;
        if (!g_app_info_launch_default_for_uri(uri, NULL, &err)) {
            if (ctx->lbl_status_msg) {
                gtk_label_set_text(GTK_LABEL(ctx->lbl_status_msg), "Unable to open image in system viewer");
            }
            if (err) g_error_free(err);
        } else {
            if (ctx->lbl_status_msg) {
                gtk_label_set_text(GTK_LABEL(ctx->lbl_status_msg), "Opened in default system viewer");
            }
        }
        g_free(uri);
    }
}

static void on_btn_open_with_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    open_image_with_external_viewer((ImageDetailContext *)user_data);
}

static gboolean on_image_dialog_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    (void)widget;
    ImageDetailContext *ctx = (ImageDetailContext *)user_data;
    guint state = event->state & gtk_accelerator_get_default_mod_mask();

    if (state == GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_c || event->keyval == GDK_KEY_C) {
            copy_image_to_clipboard(ctx);
            return GDK_EVENT_STOP;
        } else if (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S) {
            save_image_as_file(ctx);
            return GDK_EVENT_STOP;
        }
    } else if (state == 0) {
        if (event->keyval == GDK_KEY_Escape) {
            gtk_dialog_response(ctx->dialog, GTK_RESPONSE_CLOSE);
            return GDK_EVENT_STOP;
        }
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean run_image_detail_dialog(GtkWindow *parent, int clip_id, ClipEntry *entry)
{
    char *file_path = db_get_clip_file_path(clip_id);
    if (!file_path && entry && entry->file_path) {
        file_path = g_strdup(entry->file_path);
    }

    if (!file_path || !g_file_test(file_path, G_FILE_TEST_EXISTS)) {
        GtkWidget *msg = gtk_message_dialog_new(
            parent,
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "Cannot display image clip: Stored image file not found on disk."
        );
        gtk_dialog_run(GTK_DIALOG(msg));
        gtk_widget_destroy(msg);
        g_free(file_path);
        return FALSE;
    }

    GError *err = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(file_path, &err);
    if (!pixbuf) {
        GtkWidget *msg = gtk_message_dialog_new(
            parent,
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "Failed to load image file:\n%s",
            err ? err->message : "Unknown error"
        );
        gtk_dialog_run(GTK_DIALOG(msg));
        gtk_widget_destroy(msg);
        if (err) g_error_free(err);
        g_free(file_path);
        return FALSE;
    }

    ImageDetailContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.clip_id = clip_id;
    ctx.file_path = file_path;
    ctx.parent = parent;
    ctx.created_at = entry ? entry->created_at : time(NULL);
    ctx.original_pixbuf = pixbuf;
    ctx.current_zoom = 1.0;

    /* Build Dialog (first_button_text = NULL, followed by terminating sentinel NULL) */
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Clip Details - Image",
        parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        NULL,
        NULL
    );
    ctx.dialog = GTK_DIALOG(dialog);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 720, 560);
    gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);

    GtkWidget *content_area = gtk_dialog_get_content_area(ctx.dialog);
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 12);
    gtk_box_set_spacing(GTK_BOX(content_area), 8);

    /* 1. Header Frame: Dimensions, File Size, Timestamp */
    GtkWidget *header_frame = gtk_frame_new(NULL);
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(header_box), 8);

    GtkWidget *icon_img = gtk_image_new_from_icon_name("image-x-generic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(header_box), icon_img, FALSE, FALSE, 0);

    int orig_w = gdk_pixbuf_get_width(pixbuf);
    int orig_h = gdk_pixbuf_get_height(pixbuf);

    gint64 disk_size = 0;
    GFile *gf = g_file_new_for_path(file_path);
    GFileInfo *fi = g_file_query_info(gf, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (fi) {
        disk_size = g_file_info_get_size(fi);
        g_object_unref(fi);
    }
    g_object_unref(gf);

    double size_kb = (double)disk_size / 1024.0;
    char *time_str = format_full_datetime(ctx.created_at);
    char *stats_str = g_strdup_printf(
        "Dimensions: %d × %d px   •   Disk Size: %.1f KB   •   Saved: %s",
        orig_w, orig_h, size_kb, time_str
    );
    g_free(time_str);

    ctx.lbl_header_stats = gtk_label_new(stats_str);
    g_free(stats_str);
    gtk_label_set_xalign(GTK_LABEL(ctx.lbl_header_stats), 0.0);
    GtkStyleContext *st_hdr = gtk_widget_get_style_context(ctx.lbl_header_stats);
    gtk_style_context_add_class(st_hdr, "size-label");
    gtk_box_pack_start(GTK_BOX(header_box), ctx.lbl_header_stats, TRUE, TRUE, 0);

    /* Zoom Controls in Header Box */
    GtkWidget *zoom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *btn_zoom_out = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(btn_zoom_out), gtk_image_new_from_icon_name("zoom-out-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_widget_set_tooltip_text(btn_zoom_out, "Zoom Out (25%)");
    g_signal_connect(btn_zoom_out, "clicked", G_CALLBACK(on_btn_zoom_out_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(zoom_box), btn_zoom_out, FALSE, FALSE, 0);

    ctx.lbl_zoom = gtk_label_new("100%");
    gtk_widget_set_size_request(ctx.lbl_zoom, 46, -1);
    gtk_box_pack_start(GTK_BOX(zoom_box), ctx.lbl_zoom, FALSE, FALSE, 2);

    GtkWidget *btn_zoom_in = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(btn_zoom_in), gtk_image_new_from_icon_name("zoom-in-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_widget_set_tooltip_text(btn_zoom_in, "Zoom In (25%)");
    g_signal_connect(btn_zoom_in, "clicked", G_CALLBACK(on_btn_zoom_in_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(zoom_box), btn_zoom_in, FALSE, FALSE, 0);

    GtkWidget *btn_zoom_fit = gtk_button_new_with_label("Fit");
    gtk_widget_set_tooltip_text(btn_zoom_fit, "Fit image within viewing window");
    g_signal_connect(btn_zoom_fit, "clicked", G_CALLBACK(on_btn_zoom_fit_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(zoom_box), btn_zoom_fit, FALSE, FALSE, 0);

    GtkWidget *btn_zoom_orig = gtk_button_new_with_label("1:1");
    gtk_widget_set_tooltip_text(btn_zoom_orig, "Reset zoom to original 100%");
    g_signal_connect(btn_zoom_orig, "clicked", G_CALLBACK(on_btn_zoom_original_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(zoom_box), btn_zoom_orig, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(header_box), zoom_box, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(header_frame), header_box);
    gtk_box_pack_start(GTK_BOX(content_area), header_frame, FALSE, FALSE, 0);

    /* 2. Main Scrolled Image Viewport */
    ctx.scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ctx.scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(ctx.scrolled_window), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(content_area), ctx.scrolled_window, TRUE, TRUE, 0);

    /* Center image inside viewport using an event box / viewport */
    GtkWidget *viewport = gtk_viewport_new(NULL, NULL);
    GtkWidget *align_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(align_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(align_box, GTK_ALIGN_CENTER);

    ctx.image_widget = gtk_image_new_from_pixbuf(pixbuf);
    gtk_box_pack_start(GTK_BOX(align_box), ctx.image_widget, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(viewport), align_box);
    gtk_container_add(GTK_CONTAINER(ctx.scrolled_window), viewport);

    /* 3. Status Bar */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    ctx.lbl_status_msg = gtk_label_new("Ready");
    gtk_label_set_xalign(GTK_LABEL(ctx.lbl_status_msg), 0.0);
    GtkStyleContext *st_img_msg = gtk_widget_get_style_context(ctx.lbl_status_msg);
    gtk_style_context_add_class(st_img_msg, "size-label");
    gtk_box_pack_start(GTK_BOX(status_bar), ctx.lbl_status_msg, TRUE, TRUE, 0);

    GtkWidget *lbl_sc = gtk_label_new("Ctrl+C: Copy  •  Ctrl+S: Save As  •  Esc: Close");
    gtk_label_set_xalign(GTK_LABEL(lbl_sc), 1.0);
    GtkStyleContext *st_sc2 = gtk_widget_get_style_context(lbl_sc);
    gtk_style_context_add_class(st_sc2, "size-label");
    gtk_box_pack_end(GTK_BOX(status_bar), lbl_sc, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content_area), status_bar, FALSE, FALSE, 0);

    /* 4. Bottom Action Buttons Bar */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    /* Copy to Clipboard */
    GtkWidget *btn_copy = gtk_button_new_with_label("Copy to Clipboard");
    gtk_button_set_image(GTK_BUTTON(btn_copy), gtk_image_new_from_icon_name("edit-copy-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(btn_copy), TRUE);
    g_signal_connect(btn_copy, "clicked", G_CALLBACK(on_btn_copy_image_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_copy, FALSE, FALSE, 0);

    /* Save As... */
    GtkWidget *btn_save_as = gtk_button_new_with_label("Save As...");
    gtk_button_set_image(GTK_BUTTON(btn_save_as), gtk_image_new_from_icon_name("document-save-as-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(btn_save_as), TRUE);
    g_signal_connect(btn_save_as, "clicked", G_CALLBACK(on_btn_save_image_as_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_save_as, FALSE, FALSE, 0);

    /* Open With... */
    GtkWidget *btn_open_with = gtk_button_new_with_label("Open With...");
    gtk_button_set_image(GTK_BUTTON(btn_open_with), gtk_image_new_from_icon_name("document-open-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(btn_open_with), TRUE);
    gtk_widget_set_tooltip_text(btn_open_with, "Open image with system's default image viewer");
    g_signal_connect(btn_open_with, "clicked", G_CALLBACK(on_btn_open_with_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_open_with, FALSE, FALSE, 0);

    /* Close */
    GtkWidget *btn_close = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(btn_close, "clicked", G_CALLBACK(gtk_dialog_response), dialog);
    gtk_box_pack_end(GTK_BOX(btn_box), btn_close, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content_area), btn_box, FALSE, FALSE, 0);

    g_signal_connect(dialog, "key-press-event", G_CALLBACK(on_image_dialog_key_press), &ctx);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    g_object_unref(pixbuf);
    g_free(file_path);

    return FALSE;
}

/* -----------------------------------------------------------------------------
 * Public Entry Point
 * -------------------------------------------------------------------------- */

gboolean show_clip_detail_dialog(GtkWindow *parent, int clip_id, ClipType type)
{
    ClipEntry *entry = db_get_clip_by_id(clip_id);

    gboolean modified = FALSE;
    if (type == CLIP_TYPE_IMAGE || (entry && entry->type == CLIP_TYPE_IMAGE)) {
        modified = run_image_detail_dialog(parent, clip_id, entry);
    } else {
        modified = run_text_detail_dialog(parent, clip_id, entry);
    }

    if (entry) {
        clip_entry_free(entry);
    }

    return modified;
}
