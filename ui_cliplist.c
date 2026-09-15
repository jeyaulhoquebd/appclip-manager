/**
 * AppClip Manager - Clipboard UI Implementation
 * Visual representation of clipboard history with row actions, real-time search, and SQLite backing.
 * 
 * Author: Jeyaul Hoque
 * Website: https://jeyaulhoque.pages.dev/
 */

#include "ui_cliplist.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROW_DATA_KEY "appclip_entry_data"

/* Forward declarations */
static void refresh_counts_label(UiClipContext *ctx);
static GtkWidget *create_clip_row(UiClipContext *ctx, ClipEntry *entry);

static void trigger_toast(UiClipContext *ctx, const char *message)
{
    if (ctx->show_toast) {
        ctx->show_toast(message, ctx->toast_user_data);
    }
}

/**
 * Filter function for GtkListBox real-time search.
 */
static gboolean clip_filter_func(GtkListBoxRow *row, gpointer user_data)
{
    UiClipContext *ctx = (UiClipContext *)user_data;
    const char *query = gtk_entry_get_text(GTK_ENTRY(ctx->search_entry));
    if (!query || query[0] == '\0') return TRUE;

    ClipEntry *entry = (ClipEntry *)g_object_get_data(G_OBJECT(row), ROW_DATA_KEY);
    if (!entry) return TRUE;

    /* Search in text content or preview */
    if (entry->type == CLIP_TYPE_TEXT) {
        if (entry->content && g_strrstr_len(entry->content, -1, query)) return TRUE;
        if (entry->preview && g_strrstr_len(entry->preview, -1, query)) return TRUE;
    } else {
        /* Search by image filename or "image" keyword */
        if (g_strrstr_len("image png picture", -1, query)) return TRUE;
        if (entry->file_path && g_strrstr_len(entry->file_path, -1, query)) return TRUE;
    }

    return FALSE;
}

/**
 * Sort function: Pinned clips float to top, followed by created_at DESC.
 */
static gint clip_sort_func(GtkListBoxRow *row1, GtkListBoxRow *row2, gpointer user_data)
{
    (void)user_data;
    ClipEntry *e1 = (ClipEntry *)g_object_get_data(G_OBJECT(row1), ROW_DATA_KEY);
    ClipEntry *e2 = (ClipEntry *)g_object_get_data(G_OBJECT(row2), ROW_DATA_KEY);
    if (!e1 || !e2) return 0;

    if (e1->pinned != e2->pinned) {
        return e1->pinned ? -1 : 1; /* Pinned items come first */
    }

    if (e1->created_at != e2->created_at) {
        return (e1->created_at > e2->created_at) ? -1 : 1; /* Newer items first */
    }

    return (e1->id > e2->id) ? -1 : 1;
}

/**
 * Action 1: "Copy Again" button handler.
 */
static void on_btn_copy_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    GtkWidget *row = GTK_WIDGET(user_data);
    UiClipContext *ctx = (UiClipContext *)g_object_get_data(G_OBJECT(row), "ui_ctx");
    ClipEntry *entry = (ClipEntry *)g_object_get_data(G_OBJECT(row), ROW_DATA_KEY);
    if (!entry || !ctx) return;

    GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    /* Tell monitor to ignore next capture to avoid self-echoing */
    if (ctx->monitor && entry->content_hash) {
        clipboard_monitor_ignore_next(ctx->monitor, entry->content_hash);
    }

    if (entry->type == CLIP_TYPE_TEXT && entry->content) {
        gtk_clipboard_set_text(cb, entry->content, -1);
        gtk_clipboard_store(cb);
        trigger_toast(ctx, "Copied text to clipboard");
    } else if (entry->type == CLIP_TYPE_IMAGE && entry->file_path) {
        if (g_file_test(entry->file_path, G_FILE_TEST_EXISTS)) {
            GError *err = NULL;
            GdkPixbuf *pb = gdk_pixbuf_new_from_file(entry->file_path, &err);
            if (pb) {
                gtk_clipboard_set_image(cb, pb);
                gtk_clipboard_store(cb);
                g_object_unref(pb);
                trigger_toast(ctx, "Copied image to clipboard");
            } else {
                g_warning("Could not load image %s: %s", entry->file_path, err ? err->message : "");
                if (err) g_error_free(err);
            }
        }
    }
}

/**
 * Action 2: "Save As..." button handler.
 */
static void on_btn_save_as_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    GtkWidget *row = GTK_WIDGET(user_data);
    UiClipContext *ctx = (UiClipContext *)g_object_get_data(G_OBJECT(row), "ui_ctx");
    ClipEntry *entry = (ClipEntry *)g_object_get_data(G_OBJECT(row), ROW_DATA_KEY);
    if (!entry || !ctx) return;

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Save Clipboard Entry As...",
        ctx->parent_window,
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL
    );
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    char default_filename[128];
    if (entry->type == CLIP_TYPE_TEXT) {
        snprintf(default_filename, sizeof(default_filename), "clipboard_clip_%d_%ld.txt", entry->id, (long)entry->created_at);
    } else {
        snprintf(default_filename, sizeof(default_filename), "clipboard_clip_%d_%ld.png", entry->id, (long)entry->created_at);
    }
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), default_filename);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *target_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (target_path) {
            if (entry->type == CLIP_TYPE_TEXT && entry->content) {
                GError *err = NULL;
                if (g_file_set_contents(target_path, entry->content, -1, &err)) {
                    trigger_toast(ctx, "Text clip saved successfully");
                } else {
                    g_warning("Failed to save text file: %s", err ? err->message : "unknown");
                    if (err) g_error_free(err);
                }
            } else if (entry->type == CLIP_TYPE_IMAGE && entry->file_path) {
                GFile *src = g_file_new_for_path(entry->file_path);
                GFile *dst = g_file_new_for_path(target_path);
                GError *err = NULL;
                if (g_file_copy(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err)) {
                    trigger_toast(ctx, "Image clip saved successfully");
                } else {
                    g_warning("Failed to save image file: %s", err ? err->message : "unknown");
                    if (err) g_error_free(err);
                }
                g_object_unref(src);
                g_object_unref(dst);
            }
            g_free(target_path);
        }
    }
    gtk_widget_destroy(dialog);
}

/**
 * Action 3: "Delete" button handler.
 */
static void on_btn_delete_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    GtkWidget *row = GTK_WIDGET(user_data);
    UiClipContext *ctx = (UiClipContext *)g_object_get_data(G_OBJECT(row), "ui_ctx");
    ClipEntry *entry = (ClipEntry *)g_object_get_data(G_OBJECT(row), ROW_DATA_KEY);
    if (!entry || !ctx) return;

    int clip_id = entry->id;
    db_delete_clip(clip_id);
    gtk_widget_destroy(row);

    refresh_counts_label(ctx);
    trigger_toast(ctx, "Clip removed from history");
}

/**
 * Action 4: "Pin" toggle button handler.
 */
static void on_btn_pin_toggled(GtkButton *btn, gpointer user_data)
{
    GtkWidget *row = GTK_WIDGET(user_data);
    UiClipContext *ctx = (UiClipContext *)g_object_get_data(G_OBJECT(row), "ui_ctx");
    ClipEntry *entry = (ClipEntry *)g_object_get_data(G_OBJECT(row), ROW_DATA_KEY);
    if (!entry || !ctx) return;

    gboolean new_pinned = FALSE;
    if (db_toggle_pin(entry->id, &new_pinned)) {
        entry->pinned = new_pinned;

        /* Update button icon and tooltip */
        GtkWidget *icon = gtk_image_new_from_icon_name(
            new_pinned ? "starred-symbolic" : "non-starred-symbolic",
            GTK_ICON_SIZE_BUTTON
        );
        gtk_button_set_image(btn, icon);
        gtk_widget_set_tooltip_text(GTK_WIDGET(btn), new_pinned ? "Unpin this clip" : "Pin this clip to top");

        GtkStyleContext *b_ctx = gtk_widget_get_style_context(GTK_WIDGET(btn));
        if (new_pinned) {
            gtk_style_context_add_class(b_ctx, "btn-pinned");
        } else {
            gtk_style_context_remove_class(b_ctx, "btn-pinned");
        }

        /* Invalidate sort to float pinned item to top */
        gtk_list_box_invalidate_sort(ctx->list_box);
        refresh_counts_label(ctx);
        trigger_toast(ctx, new_pinned ? "Clip pinned to top" : "Clip unpinned");
    }
}

/**
 * Creates a single styled GtkListBoxRow for a ClipEntry.
 */
static GtkWidget *create_clip_row(UiClipContext *ctx, ClipEntry *entry)
{
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);

    /* Store entry copy inside row */
    ClipEntry *entry_copy = clip_entry_new();
    entry_copy->id = entry->id;
    entry_copy->type = entry->type;
    entry_copy->content = entry->content ? g_strdup(entry->content) : NULL;
    entry_copy->file_path = entry->file_path ? g_strdup(entry->file_path) : NULL;
    entry_copy->preview = entry->preview ? g_strdup(entry->preview) : NULL;
    entry_copy->content_hash = entry->content_hash ? g_strdup(entry->content_hash) : NULL;
    entry_copy->created_at = entry->created_at;
    entry_copy->pinned = entry->pinned;
    entry_copy->image_width = entry->image_width;
    entry_copy->image_height = entry->image_height;
    entry_copy->file_size_bytes = entry->file_size_bytes;

    g_object_set_data_full(G_OBJECT(row), ROW_DATA_KEY, entry_copy, (GDestroyNotify)clip_entry_free);
    g_object_set_data(G_OBJECT(row), "ui_ctx", ctx);

    /* Main row layout */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);

    /* 1. Pin Button (leftmost) */
    GtkWidget *btn_pin = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(btn_pin), GTK_RELIEF_NONE);
    GtkWidget *pin_icon = gtk_image_new_from_icon_name(
        entry->pinned ? "starred-symbolic" : "non-starred-symbolic",
        GTK_ICON_SIZE_BUTTON
    );
    gtk_button_set_image(GTK_BUTTON(btn_pin), pin_icon);
    gtk_widget_set_tooltip_text(btn_pin, entry->pinned ? "Unpin this clip" : "Pin this clip to top");
    GtkStyleContext *pin_style = gtk_widget_get_style_context(btn_pin);
    if (entry->pinned) {
        gtk_style_context_add_class(pin_style, "btn-pinned");
    }
    g_signal_connect(btn_pin, "clicked", G_CALLBACK(on_btn_pin_toggled), row);
    gtk_box_pack_start(GTK_BOX(hbox), btn_pin, FALSE, FALSE, 0);

    /* 2. Type Icon or Scaled Image Thumbnail (48x48) */
    GtkWidget *thumb_widget = NULL;
    if (entry->type == CLIP_TYPE_IMAGE && entry->file_path && g_file_test(entry->file_path, G_FILE_TEST_EXISTS)) {
        GError *err = NULL;
        GdkPixbuf *scaled = gdk_pixbuf_new_from_file_at_scale(entry->file_path, 48, 48, TRUE, &err);
        if (scaled) {
            thumb_widget = gtk_image_new_from_pixbuf(scaled);
            g_object_unref(scaled);
        } else {
            if (err) g_error_free(err);
            thumb_widget = gtk_image_new_from_icon_name("image-x-generic", GTK_ICON_SIZE_DND);
        }
    } else {
        thumb_widget = gtk_image_new_from_icon_name(
            (entry->type == CLIP_TYPE_IMAGE) ? "image-x-generic" : "text-x-generic",
            GTK_ICON_SIZE_DND
        );
    }
    gtk_widget_set_size_request(thumb_widget, 48, 48);
    gtk_box_pack_start(GTK_BOX(hbox), thumb_widget, FALSE, FALSE, 0);

    /* 3. Text Preview or Image Metadata Details */
    GtkWidget *vbox_text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    if (entry->type == CLIP_TYPE_TEXT) {
        /* Truncated preview (first 100 chars) */
        char *disp_preview = entry->preview ? g_strdup(entry->preview) :
                             clip_generate_text_preview(entry->content ? entry->content : "", 100);
        GtkWidget *lbl_preview = gtk_label_new(disp_preview);
        g_free(disp_preview);
        gtk_label_set_xalign(GTK_LABEL(lbl_preview), 0.0);
        gtk_label_set_line_wrap(GTK_LABEL(lbl_preview), TRUE);
        gtk_label_set_line_wrap_mode(GTK_LABEL(lbl_preview), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_max_width_chars(GTK_LABEL(lbl_preview), 65);
        GtkStyleContext *p_style = gtk_widget_get_style_context(lbl_preview);
        gtk_style_context_add_class(p_style, "clip-preview-text");
        gtk_box_pack_start(GTK_BOX(vbox_text), lbl_preview, FALSE, FALSE, 0);

        /* Subtitle: character count + relative time */
        size_t len = entry->content ? strlen(entry->content) : 0;
        char *rel_time = clip_format_relative_time(entry->created_at);
        char *sub_text = g_strdup_printf("Text • %zu characters • %s", len, rel_time);
        g_free(rel_time);

        GtkWidget *lbl_sub = gtk_label_new(sub_text);
        g_free(sub_text);
        gtk_label_set_xalign(GTK_LABEL(lbl_sub), 0.0);
        GtkStyleContext *sub_style = gtk_widget_get_style_context(lbl_sub);
        gtk_style_context_add_class(sub_style, "size-label");
        gtk_box_pack_start(GTK_BOX(vbox_text), lbl_sub, FALSE, FALSE, 0);
    } else {
        /* Image: dimensions + file size + relative time */
        char *rel_time = clip_format_relative_time(entry->created_at);
        char *title_text = g_strdup_printf("Image (%dx%d PNG)", entry->image_width, entry->image_height);
        GtkWidget *lbl_title = gtk_label_new(title_text);
        g_free(title_text);
        gtk_label_set_xalign(GTK_LABEL(lbl_title), 0.0);
        GtkStyleContext *t_style = gtk_widget_get_style_context(lbl_title);
        gtk_style_context_add_class(t_style, "clip-preview-text");
        gtk_box_pack_start(GTK_BOX(vbox_text), lbl_title, FALSE, FALSE, 0);

        double size_kb = (double)entry->file_size_bytes / 1024.0;
        char *sub_text = g_strdup_printf("Saved to disk • %.1f KB • %s", size_kb, rel_time);
        g_free(rel_time);

        GtkWidget *lbl_sub = gtk_label_new(sub_text);
        g_free(sub_text);
        gtk_label_set_xalign(GTK_LABEL(lbl_sub), 0.0);
        GtkStyleContext *sub_style = gtk_widget_get_style_context(lbl_sub);
        gtk_style_context_add_class(sub_style, "size-label");
        gtk_box_pack_start(GTK_BOX(vbox_text), lbl_sub, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(hbox), vbox_text, TRUE, TRUE, 0);

    /* 4. Action Buttons Box (Copy Again, Save As, Delete) */
    GtkWidget *actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_valign(actions_box, GTK_ALIGN_CENTER);

    /* Button 1: Copy Again */
    GtkWidget *btn_copy = gtk_button_new_with_label("Copy Again");
    gtk_button_set_image(GTK_BUTTON(btn_copy), gtk_image_new_from_icon_name("edit-copy-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(btn_copy), TRUE);
    gtk_widget_set_tooltip_text(btn_copy, "Put this clip back onto the system clipboard");
    g_signal_connect(btn_copy, "clicked", G_CALLBACK(on_btn_copy_clicked), row);
    gtk_box_pack_start(GTK_BOX(actions_box), btn_copy, FALSE, FALSE, 0);

    /* Button 2: Save As... */
    GtkWidget *btn_save = gtk_button_new_with_label("Save As...");
    gtk_button_set_image(GTK_BUTTON(btn_save), gtk_image_new_from_icon_name("document-save-as-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(btn_save), TRUE);
    gtk_widget_set_tooltip_text(btn_save, "Export this clip to a file on your disk");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_btn_save_as_clicked), row);
    gtk_box_pack_start(GTK_BOX(actions_box), btn_save, FALSE, FALSE, 0);

    /* Button 3: Delete */
    GtkWidget *btn_del = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(btn_del), gtk_image_new_from_icon_name("user-trash-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_widget_set_tooltip_text(btn_del, "Delete this clip from history");
    GtkStyleContext *del_style = gtk_widget_get_style_context(btn_del);
    gtk_style_context_add_class(del_style, "btn-delete-clip");
    g_signal_connect(btn_del, "clicked", G_CALLBACK(on_btn_delete_clicked), row);
    gtk_box_pack_start(GTK_BOX(actions_box), btn_del, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(hbox), actions_box, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(row), hbox);
    gtk_widget_show_all(row);

    return row;
}

static void refresh_counts_label(UiClipContext *ctx)
{
    if (!ctx || !ctx->lbl_counts) return;

    int pinned = 0;
    int total = db_get_clip_counts(&pinned);

    char *text = g_strdup_printf("%d saved clips (%d pinned)", total, pinned);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_counts), text);
    g_free(text);

    if (ctx->empty_state_box) {
        gtk_widget_set_visible(ctx->empty_state_box, (total == 0));
    }
}

void ui_cliplist_reload(UiClipContext *ctx)
{
    if (!ctx || !ctx->list_box) return;

    /* Remove existing rows */
    GList *children = gtk_container_get_children(GTK_CONTAINER(ctx->list_box));
    for (GList *l = children; l; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    /* Fetch clips from SQLite */
    GList *clips = db_get_recent_clips(ctx->settings.max_history_items, NULL);
    for (GList *l = clips; l; l = l->next) {
        ClipEntry *entry = (ClipEntry *)l->data;
        GtkWidget *row = create_clip_row(ctx, entry);
        gtk_list_box_insert(ctx->list_box, row, -1);
    }
    clip_entry_list_free(clips);

    gtk_list_box_invalidate_filter(ctx->list_box);
    gtk_list_box_invalidate_sort(ctx->list_box);
    refresh_counts_label(ctx);
}

void ui_cliplist_add_entry(UiClipContext *ctx, const ClipEntry *entry)
{
    if (!ctx || !ctx->list_box || !entry) return;

    GtkWidget *row = create_clip_row(ctx, (ClipEntry *)entry);
    gtk_list_box_prepend(ctx->list_box, row);
    gtk_list_box_invalidate_filter(ctx->list_box);
    gtk_list_box_invalidate_sort(ctx->list_box);
    refresh_counts_label(ctx);
}

/**
 * Clear History button callback: confirms with user before clearing non-pinned clips.
 */
static void on_clear_history_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    UiClipContext *ctx = (UiClipContext *)user_data;
    if (!ctx) return;

    GtkWidget *dialog = gtk_message_dialog_new(
        ctx->parent_window,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE,
        "Clear Clipboard History?"
    );
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dialog),
        "Are you sure you want to remove all non-pinned clips?\nPinned clips will be kept safe."
    );

    gtk_dialog_add_button(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_CANCEL);
    GtkWidget *btn_confirm = gtk_dialog_add_button(GTK_DIALOG(dialog), "Clear Non-Pinned", GTK_RESPONSE_YES);
    GtkStyleContext *b_style = gtk_widget_get_style_context(btn_confirm);
    gtk_style_context_add_class(b_style, "destructive-action");

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES) {
        db_clear_history(TRUE);
        ui_cliplist_reload(ctx);
        trigger_toast(ctx, "Cleared history (pinned clips preserved)");
    }
}

/**
 * Monitor callback invoked when a new clip is detected and inserted.
 */
static void on_new_clip_captured(ClipEntry *entry, gpointer user_data)
{
    UiClipContext *ctx = (UiClipContext *)user_data;
    if (!ctx || !entry) return;

    ui_cliplist_add_entry(ctx, entry);
}

void ui_cliplist_set_toast_callback(UiClipContext *ctx, void (*toast_cb)(const char *, gpointer), gpointer user_data)
{
    if (!ctx) return;
    ctx->show_toast = toast_cb;
    ctx->toast_user_data = user_data;
}

GtkWidget *ui_cliplist_create_panel(UiClipContext **out_ctx, GtkWindow *parent_window)
{
    UiClipContext *ctx = g_new0(UiClipContext, 1);
    ctx->parent_window = parent_window;
    settings_load(&ctx->settings);

    /* Main vertical container */
    ctx->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* 1. Wayland InfoBar (if Wayland session and wl-clipboard missing) */
    ctx->infobar_wayland = gtk_info_bar_new();
    gtk_info_bar_set_message_type(GTK_INFO_BAR(ctx->infobar_wayland), GTK_MESSAGE_WARNING);
    gtk_info_bar_set_show_close_button(GTK_INFO_BAR(ctx->infobar_wayland), TRUE);
    g_signal_connect_swapped(ctx->infobar_wayland, "response", G_CALLBACK(gtk_widget_hide), ctx->infobar_wayland);

    GtkWidget *info_content = gtk_info_bar_get_content_area(GTK_INFO_BAR(ctx->infobar_wayland));
    GtkWidget *lbl_wayland = gtk_label_new(
        "Wayland session detected: install 'wl-clipboard' for real-time clipboard monitoring: sudo apt install wl-clipboard"
    );
    gtk_label_set_line_wrap(GTK_LABEL(lbl_wayland), TRUE);
    gtk_box_pack_start(GTK_BOX(info_content), lbl_wayland, TRUE, TRUE, 0);

    gtk_widget_set_no_show_all(ctx->infobar_wayland, TRUE);
    gtk_box_pack_start(GTK_BOX(ctx->container), ctx->infobar_wayland, FALSE, FALSE, 0);

    /* 2. Top Controls Bar: Search Entry + Clear History + Counts */
    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(top_bar), 12);

    ctx->search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx->search_entry), "Search clipboard text, snippets, or image clips...");
    gtk_box_pack_start(GTK_BOX(top_bar), GTK_WIDGET(ctx->search_entry), TRUE, TRUE, 0);

    ctx->lbl_counts = gtk_label_new("0 saved clips");
    GtkStyleContext *c_style = gtk_widget_get_style_context(ctx->lbl_counts);
    gtk_style_context_add_class(c_style, "size-label");
    gtk_box_pack_start(GTK_BOX(top_bar), ctx->lbl_counts, FALSE, FALSE, 6);

    ctx->btn_clear_history = gtk_button_new_with_label("Clear History");
    gtk_button_set_image(GTK_BUTTON(ctx->btn_clear_history), gtk_image_new_from_icon_name("edit-clear-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(ctx->btn_clear_history), TRUE);
    gtk_widget_set_tooltip_text(ctx->btn_clear_history, "Remove all non-pinned items from clipboard history");
    g_signal_connect(ctx->btn_clear_history, "clicked", G_CALLBACK(on_clear_history_clicked), ctx);
    gtk_box_pack_end(GTK_BOX(top_bar), ctx->btn_clear_history, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(ctx->container), top_bar, FALSE, FALSE, 0);

    /* 3. Main ScrolledWindow + GtkListBox */
    GtkWidget *overlay = gtk_overlay_new();
    gtk_box_pack_start(GTK_BOX(ctx->container), overlay, TRUE, TRUE, 0);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(overlay), scrolled_window);

    ctx->list_box = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(ctx->list_box, GTK_SELECTION_NONE);
    gtk_list_box_set_filter_func(ctx->list_box, clip_filter_func, ctx, NULL);
    gtk_list_box_set_sort_func(ctx->list_box, clip_sort_func, ctx, NULL);

    g_signal_connect_swapped(ctx->search_entry, "search-changed",
                             G_CALLBACK(gtk_list_box_invalidate_filter), ctx->list_box);

    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(ctx->list_box));

    /* Empty state display */
    ctx->empty_state_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(ctx->empty_state_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(ctx->empty_state_box, GTK_ALIGN_CENTER);

    GtkWidget *empty_icon = gtk_image_new_from_icon_name("edit-paste-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start(GTK_BOX(ctx->empty_state_box), empty_icon, FALSE, FALSE, 0);

    GtkWidget *lbl_empty = gtk_label_new("Clipboard History is empty");
    GtkStyleContext *e_style = gtk_widget_get_style_context(lbl_empty);
    gtk_style_context_add_class(e_style, "size-label");
    gtk_box_pack_start(GTK_BOX(ctx->empty_state_box), lbl_empty, FALSE, FALSE, 0);

    GtkWidget *lbl_hint = gtk_label_new("Any text or image copied to your clipboard will appear here automatically.");
    GtkStyleContext *h_style = gtk_widget_get_style_context(lbl_hint);
    gtk_style_context_add_class(h_style, "size-label");
    gtk_box_pack_start(GTK_BOX(ctx->empty_state_box), lbl_hint, FALSE, FALSE, 0);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), ctx->empty_state_box);

    /* 4. Start Clipboard Monitor */
    ctx->monitor = clipboard_monitor_new(on_new_clip_captured, ctx);
    clipboard_monitor_start(ctx->monitor);

    /* Show Wayland warning banner if missing tools */
    if (clipboard_monitor_is_wayland(ctx->monitor) &&
        clipboard_monitor_is_wayland_missing_tools(ctx->monitor)) {
        gtk_widget_show_all(ctx->infobar_wayland);
    }

    /* Initial load of stored clips */
    ui_cliplist_reload(ctx);

    if (out_ctx) *out_ctx = ctx;
    return ctx->container;
}

void ui_cliplist_free(UiClipContext *ctx)
{
    if (!ctx) return;
    if (ctx->monitor) {
        clipboard_monitor_free(ctx->monitor);
        ctx->monitor = NULL;
    }
    g_free(ctx);
}
