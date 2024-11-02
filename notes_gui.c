#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cmark.h>
#include <glib.h>
#include <glib/gstdio.h>

#define MAX_NOTES 100
#define MAX_LENGTH 10000

struct Note {
    char content[MAX_LENGTH];
    int id;
};

// Global variables
struct Note notes[MAX_NOTES];
int noteCount = 0;
GtkWidget *window;
GtkWidget *web_view;
GtkWidget *list_box;
GtkWidget *file_tree;
char *vault_directory = NULL;
GtkTreeStore *tree_store;
GtkTreeView *tree_view;
char *current_file_path = NULL;
char *default_save_directory = NULL;
gboolean is_content_saved = TRUE;
gboolean autosave_enabled = TRUE;
guint autosave_timeout_id = 0;
// Add this with the other global variables at the top
GtkWidget *save_indicator_label;
GtkWidget *dark_mode_switch;
GtkWidget *vault_label;
gboolean dark_mode_enabled = FALSE;
char *config_file_path = NULL;
GSettings *settings;
gboolean preview_hidden = TRUE;  // Default to hidden preview
GtkWidget *preview_toggle_switch;
GtkCssProvider *css_provider = NULL;

// Function prototypes
// Basic note operations
void add_note(GtkWidget *widget, gpointer data);
void delete_note(GtkWidget *widget, gpointer data);
void note_selected(GtkListBox *box, GtkListBoxRow *row, gpointer data);
void update_notes_list();

// Editor operations
void load_editor();
void get_editor_content(GtkWidget *widget, gpointer data);
void handle_editor_content(GObject *source_object, GAsyncResult *result, gpointer user_data);
void show_editor();
void show_start_page();
void setup_css_provider(void);


// File operations
void save_note(GtkWidget *widget, gpointer data);
void save_note_as(GtkWidget *widget, gpointer data);
void save_current_content_to_file(const char *filepath);
void handle_save_content(GObject *source_object, GAsyncResult *result, gpointer user_data);
void rename_file(GtkWidget *menuitem, gpointer userdata);
void delete_file(GtkWidget *menuitem, gpointer userdata);

// Asset management
char* get_asset_path(const char* filename);
char* get_file_contents(const char* path);

// UI handlers
void show_error_dialog(const char *message);
void choose_vault_directory(GtkWidget *widget, gpointer data);
void refresh_file_tree();
void file_tree_selection_changed(GtkTreeSelection *selection, gpointer data);
void update_window_title();
gboolean on_tree_button_press(GtkWidget *widget, GdkEventButton *event, gpointer userdata);
void update_vault_label();
void update_recent_files();

// Settings and configuration
void init_config();
void load_config();
void save_config();
void toggle_dark_mode(GtkWidget *widget, gpointer data);
void apply_dark_mode();
void toggle_preview(GtkWidget *widget, gpointer data);

// Autosave functionality
gboolean autosave_callback(gpointer user_data);
void toggle_autosave(GtkWidget *widget, gpointer data);
void update_save_indicator();
void mark_content_unsaved();
gboolean check_unsaved_changes();

// WebKit handlers
void register_web_handlers(WebKitUserContentManager *manager);
void web_view_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, gpointer user_data);
void handle_new_note(WebKitUserContentManager *manager, 
                    WebKitJavascriptResult *js_result, 
                    gpointer user_data);
void handle_open_file(WebKitUserContentManager *manager, 
                     WebKitJavascriptResult *js_result, 
                     gpointer user_data);

void apply_gtk_css();

void handle_editor_initialized(WebKitUserContentManager *manager, 
                             WebKitJavascriptResult *js_result, 
                             gpointer user_data);


static void ignore_webkit_messages(const gchar *log_domain,
                                 GLogLevelFlags log_level,
                                 const gchar *message,
                                 gpointer user_data) {
    if (g_str_has_prefix(message, "GFileInfo created without standard::size") ||
        g_str_has_prefix(message, "file ../gio/gfileinfo.c")) {
        return;
    }
    g_log_default_handler(log_domain, log_level, message, user_data);
}


int main(int argc, char *argv[]) {
    g_log_set_handler("GLib-GIO",
                     G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING,
                     ignore_webkit_messages,
                     NULL);


    gtk_init(&argc, &argv);


    setup_css_provider();


    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Markdown Notes App");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    // Create left panel
    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), left_panel, FALSE, FALSE, 5);

    // Vault selection section
    GtkWidget *choose_vault_button = gtk_button_new_with_label("Choose Vault Directory");
    gtk_box_pack_start(GTK_BOX(left_panel), choose_vault_button, FALSE, FALSE, 0);

    vault_label = gtk_label_new("No vault selected");
    gtk_label_set_ellipsize(GTK_LABEL(vault_label), PANGO_ELLIPSIZE_START);
    gtk_box_pack_start(GTK_BOX(left_panel), vault_label, FALSE, FALSE, 5);

    // File tree section
    tree_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store)));
    g_signal_connect(GTK_WIDGET(tree_view), "button-press-event", 
                    G_CALLBACK(on_tree_button_press), NULL);

    

    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", 0);
    gtk_tree_view_column_set_title(column, "Files");
    gtk_tree_view_append_column(tree_view, column);

    GtkWidget *scroll_tree = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_tree),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll_tree), GTK_WIDGET(tree_view));
    gtk_widget_set_size_request(scroll_tree, 200, 300);
    gtk_box_pack_start(GTK_BOX(left_panel), scroll_tree, TRUE, TRUE, 0);

    // Settings section
    GtkWidget *settings_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(left_panel), settings_box, FALSE, FALSE, 5);

    GtkWidget *preview_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *preview_label = gtk_label_new("Hide Preview");
    preview_toggle_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(preview_toggle_switch), TRUE);  // Default to hidden
    gtk_box_pack_start(GTK_BOX(preview_box), preview_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(preview_box), preview_toggle_switch, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(settings_box), preview_box, FALSE, FALSE, 0);

    // Connect preview toggle signal
    g_signal_connect(preview_toggle_switch, "notify::active", G_CALLBACK(toggle_preview), NULL);

    // Dark mode toggle
    GtkWidget *dark_mode_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *dark_mode_label = gtk_label_new("Dark Mode");
    dark_mode_switch = gtk_switch_new();
    gtk_box_pack_start(GTK_BOX(dark_mode_box), dark_mode_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dark_mode_box), dark_mode_switch, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(settings_box), dark_mode_box, FALSE, FALSE, 0);

    // Autosave toggle
    GtkWidget *autosave_check = gtk_check_button_new_with_label("Autosave");
    gtk_box_pack_start(GTK_BOX(settings_box), autosave_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autosave_check), TRUE);

    init_config();

    if (dark_mode_enabled) {
    gtk_switch_set_active(GTK_SWITCH(dark_mode_switch), TRUE);
    apply_dark_mode();
  }

    // Save indicator
    save_indicator_label = gtk_label_new("Saved");
    gtk_box_pack_start(GTK_BOX(settings_box), save_indicator_label, FALSE, FALSE, 0);

    // Action buttons
    GtkWidget *buttons_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(left_panel), buttons_box, FALSE, FALSE, 5);

    GtkWidget *add_button = gtk_button_new_with_label("Add Note");
    GtkWidget *delete_button = gtk_button_new_with_label("Delete Note");
    GtkWidget *save_button = gtk_button_new_with_label("Save");
    GtkWidget *save_as_button = gtk_button_new_with_label("Save As");

    gtk_box_pack_start(GTK_BOX(buttons_box), add_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons_box), delete_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons_box), save_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons_box), save_as_button, FALSE, FALSE, 0);

    // Editor section
    // Create the user content manager and register handlers
    WebKitUserContentManager *manager = webkit_user_content_manager_new();
    register_web_handlers(manager);

    // Create the web view with the user content manager
    web_view = webkit_web_view_new_with_user_content_manager(manager);

    // Set settings
    WebKitSettings *settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(web_view));
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_allow_file_access_from_file_urls(settings, TRUE);
    webkit_settings_set_enable_write_console_messages_to_stdout(settings, TRUE);
    webkit_settings_set_allow_universal_access_from_file_urls(settings, TRUE);

    GtkWidget *scrolled_window_web = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_hexpand(scrolled_window_web, TRUE);
    gtk_widget_set_vexpand(scrolled_window_web, TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled_window_web), web_view);
    gtk_box_pack_start(GTK_BOX(main_box), scrolled_window_web, TRUE, TRUE, 5);

    // Now that web_view is initialized, call load_editor()
    load_editor();

    // Connect to load-changed signal
    g_signal_connect(web_view, "load-changed", G_CALLBACK(web_view_load_changed), NULL);

    // Connect all signals
    g_signal_connect(choose_vault_button, "clicked", G_CALLBACK(choose_vault_directory), NULL);
    g_signal_connect(add_button, "clicked", G_CALLBACK(add_note), NULL);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(delete_note), NULL);
    g_signal_connect(save_button, "clicked", G_CALLBACK(save_note), NULL);
    g_signal_connect(save_as_button, "clicked", G_CALLBACK(save_note_as), NULL);
    g_signal_connect(dark_mode_switch, "notify::active", G_CALLBACK(toggle_dark_mode), NULL);
    g_signal_connect(autosave_check, "toggled", G_CALLBACK(toggle_autosave), NULL);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    g_signal_connect(selection, "changed", G_CALLBACK(file_tree_selection_changed), NULL);

    // Apply settings from config
    if (dark_mode_enabled) {
        gtk_switch_set_active(GTK_SWITCH(dark_mode_switch), TRUE);
    }
    update_vault_label();

    // Show window
    gtk_widget_show_all(window);
    gtk_main();

    if (css_provider) {
        g_object_unref(css_provider);
    }

    // Save config before exit
    save_config();

    return 0;
}

void load_editor() {
    char *css_path = get_asset_path("editor.css");
    char *js_path = get_asset_path("editor.js");
    
    if (!css_path || !js_path) {
        show_error_dialog("Failed to locate assets");
        g_free(css_path);
        g_free(js_path);
        return;
    }
    
    // Read CSS file
    char *css_content = get_file_contents(css_path);
    if (!css_content) {
        show_error_dialog("Failed to load editor.css");
        g_free(css_path);
        g_free(js_path);
        return;
    }
    
    // Read JS file
    char *js_content = get_file_contents(js_path);
    if (!js_content) {
        show_error_dialog("Failed to load editor.js");
        g_free(css_path);
        g_free(js_path);
        g_free(css_content);
        return;
    }
    
    const char *html_template = 
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<meta http-equiv=\"Content-Security-Policy\" "
        "content=\"default-src 'self' 'unsafe-inline' 'unsafe-eval' "
        "https://uicdn.toast.com data: blob:;\">"
        "<title>Markdown Editor</title>"
        "<link rel=\"stylesheet\" href=\"https://uicdn.toast.com/editor/latest/toastui-editor.min.css\">"
        "<style>%s</style>"
        "</head>"
        "<body>"
        "<div id=\"start-page\" class=\"start-page\">"
        "  <h1>Welcome to Markdown Notes</h1>"
        "  <div class=\"start-actions\">"
        "    <button onclick=\"window.webkit.messageHandlers.newNote.postMessage('')\" "
        "            class=\"start-button\">New Note</button>"
        "  </div>"
        "  <div class=\"recent-files\" id=\"recent-files\">"
        "    <h2>Recent Notes</h2>"
        "    <div id=\"recent-files-list\"></div>"
        "  </div>"
        "</div>"
        "<div id=\"editor\"></div>"
        "<script src=\"https://uicdn.toast.com/editor/latest/toastui-editor-all.min.js\"></script>"
        "<script>%s</script>"
        "</body>"
        "</html>";
    
    char *html_content = g_strdup_printf(html_template, css_content, js_content);
    
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(web_view), html_content, "file:///");
    
    g_free(css_path);
    g_free(js_path);
    g_free(css_content);
    g_free(js_content);
    g_free(html_content);
}

void web_view_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, gpointer user_data) {
    if (load_event == WEBKIT_LOAD_FINISHED) {
        // Now that the content is loaded, we can call JavaScript functions
        show_start_page();
        apply_dark_mode(); // Apply dark mode after content is loaded
    }
}

void choose_vault_directory(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Choose Vault Directory",
                                                   GTK_WINDOW(window),
                                                   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                   "_Cancel", GTK_RESPONSE_CANCEL,
                                                   "_Open", GTK_RESPONSE_ACCEPT,
                                                   NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        g_free(vault_directory);
        vault_directory = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        refresh_file_tree();
    }

    gtk_widget_destroy(dialog);
}

void refresh_file_tree() {
    gtk_tree_store_clear(tree_store);
    if (!vault_directory) return;

    GDir *dir = g_dir_open(vault_directory, 0, NULL);
    if (!dir) return;

    const gchar *filename;
    GtkTreeIter iter;

    while ((filename = g_dir_read_name(dir))) {
        if (g_str_has_suffix(filename, ".md")) {
            char *full_path = g_build_filename(vault_directory, filename, NULL);
            gtk_tree_store_append(tree_store, &iter, NULL);
            gtk_tree_store_set(tree_store, &iter,
                             0, filename,
                             1, full_path,
                             -1);
            g_free(full_path);
        }
    }
    g_dir_close(dir);
}

void file_tree_selection_changed(GtkTreeSelection *selection, gpointer data) {
    // Check for unsaved changes before switching files
    if (!is_content_saved && !check_unsaved_changes()) {
        // User cancelled, reselect the previous file
        if (current_file_path) {
            GtkTreeModel *model = GTK_TREE_MODEL(tree_store);
            GtkTreeIter iter;
            gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
            while (valid) {
                char *filepath;
                gtk_tree_model_get(model, &iter, 1, &filepath, -1);
                if (g_strcmp0(filepath, current_file_path) == 0) {
                    gtk_tree_selection_select_iter(selection, &iter);
                    g_free(filepath);
                    return;
                }
                g_free(filepath);
                valid = gtk_tree_model_iter_next(model, &iter);
            }
        }
        return;
    }

    GtkTreeIter iter;
    GtkTreeModel *model;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *filepath;
        gtk_tree_model_get(model, &iter, 1, &filepath, -1);
        g_free(current_file_path);
        current_file_path = g_strdup(filepath);

        char *content = NULL;
        g_file_get_contents(filepath, &content, NULL, NULL);
        if (content) {
            char *escaped_content = g_markup_escape_text(content, -1);
            char *script = g_strdup_printf("editor.setMarkdown(`%s`);", escaped_content);
            webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view), 
                                             script, -1, NULL, NULL, NULL, NULL, NULL);
            g_free(script);
            g_free(escaped_content);
            g_free(content);
            is_content_saved = TRUE;
            update_save_indicator();
            update_window_title();
        }
        g_free(filepath);
        show_editor(); // Show the editor when a file is selected
    }
}

void toggle_preview(GtkWidget *widget, gpointer data) {
    preview_hidden = gtk_switch_get_active(GTK_SWITCH(widget));
    char *script = g_strdup_printf("togglePreview(!Boolean(%s));", 
                                 preview_hidden ? "true" : "false");
    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
        script, -1, NULL, NULL, NULL, NULL, NULL);
    g_free(script);
}



void save_note(GtkWidget *widget, gpointer data) {
    if (current_file_path == NULL) {
        save_note_as(widget, data);
        return;
    }
    save_current_content_to_file(current_file_path);
}

void save_note_as(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save Note As",
                                                   GTK_WINDOW(window),
                                                   GTK_FILE_CHOOSER_ACTION_SAVE,
                                                   "_Cancel", GTK_RESPONSE_CANCEL,
                                                   "_Save", GTK_RESPONSE_ACCEPT,
                                                   NULL);

    if (default_save_directory) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_save_directory);
    }

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Markdown files");
    gtk_file_filter_add_pattern(filter, "*.md");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "note.md");

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        if (!g_str_has_suffix(filename, ".md")) {
            char *new_filename = g_strconcat(filename, ".md", NULL);
            g_free(filename);
            filename = new_filename;
        }

        save_current_content_to_file(filename);
        
        g_free(current_file_path);
        current_file_path = filename;
        
        char *dir = g_path_get_dirname(filename);
        g_free(default_save_directory);
        default_save_directory = dir;
    }

    gtk_widget_destroy(dialog);
}

void save_current_content_to_file(const char *filepath) {
    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
        "editor.getMarkdown();",
        -1,
        NULL,
        NULL,
        NULL,
        handle_save_content,
        (gpointer)filepath);
}

void handle_save_content(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    const char *filepath = (const char *)user_data;
    GError *error = NULL;
    JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source_object), 
                                                               result, 
                                                               &error);
    
    if (error) {
        show_error_dialog(error->message);
        g_error_free(error);
        return;
    }

    if (jsc_value_is_string(value)) {
        char *content = jsc_value_to_string(value);
        
        FILE *file = fopen(filepath, "w");
        if (file) {
            fputs(content, file);
            fclose(file);
            
            is_content_saved = TRUE;
            update_save_indicator();
            
            // Update window title to show current file
            char *filename = g_path_get_basename(filepath);
            char *title = g_strdup_printf("Markdown Notes App - %s", filename);
            gtk_window_set_title(GTK_WINDOW(window), title);
            g_free(filename);
            g_free(title);
        } else {
            show_error_dialog("Failed to save file");
        }
        g_free(content);
    }
    
    g_object_unref(value);
}

void show_error_dialog(const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_CLOSE,
                                             "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void add_note(GtkWidget *widget, gpointer data) {
    if (!vault_directory) {
        show_error_dialog("Please select a vault directory first");
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("New Note",
                                                   GTK_WINDOW(window),
                                                   GTK_DIALOG_MODAL,
                                                   "_Cancel",
                                                   GTK_RESPONSE_CANCEL,
                                                   "_Create",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), "new_note.md");
    gtk_container_add(GTK_CONTAINER(content_area), entry);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *filename = gtk_entry_get_text(GTK_ENTRY(entry));
        char *filepath = g_build_filename(vault_directory, filename, NULL);
        
        // Ensure .md extension
        if (!g_str_has_suffix(filepath, ".md")) {
            char *new_filepath = g_strconcat(filepath, ".md", NULL);
            g_free(filepath);
            filepath = new_filepath;
        }

        // Create empty file
        FILE *file = fopen(filepath, "w");
        if (file) {
            fclose(file);
            refresh_file_tree();
            
            // Select the new file
            GtkTreeModel *model = GTK_TREE_MODEL(tree_store);
            GtkTreeIter iter;
            gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
            while (valid) {
                char *path;
                gtk_tree_model_get(model, &iter, 1, &path, -1);
                if (g_strcmp0(path, filepath) == 0) {
                    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
                    gtk_tree_selection_select_iter(selection, &iter);
                    g_free(path);
                    break;
                }
                g_free(path);
                valid = gtk_tree_model_iter_next(model, &iter);
            }
        } else {
            show_error_dialog("Failed to create new note");
        }
        g_free(filepath);
    }
    gtk_widget_destroy(dialog);
}

void delete_note(GtkWidget *widget, gpointer data) {
    GtkListBoxRow *selected_row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list_box));
    if (selected_row != NULL) {
        int index = gtk_list_box_row_get_index(selected_row);
        for (int i = index; i < noteCount - 1; i++) {
            notes[i] = notes[i + 1];
        }
        noteCount--;
        update_notes_list();

        webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
            "editor.setMarkdown('');",
            -1,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL);
    }
}

void note_selected(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    // Check for unsaved changes before switching
    if (!is_content_saved && !check_unsaved_changes()) {
        // User cancelled, reselect the previous row
        if (current_file_path) {
            // Find and select the row corresponding to current_file_path
            GtkTreeModel *model = GTK_TREE_MODEL(tree_store);
            GtkTreeIter iter;
            gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
            while (valid) {
                char *filepath;
                gtk_tree_model_get(model, &iter, 1, &filepath, -1);
                if (g_strcmp0(filepath, current_file_path) == 0) {
                    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
                    gtk_tree_selection_select_iter(selection, &iter);
                    g_free(filepath);
                    break;
                }
                g_free(filepath);
                valid = gtk_tree_model_iter_next(model, &iter);
            }
        }
        return;
    }

    if (row != NULL) {
        int index = gtk_list_box_row_get_index(row);
        char *escaped_content = g_markup_escape_text(notes[index].content, -1);
        char *script = g_strdup_printf("editor.setMarkdown(`%s`);", escaped_content);
        
        webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
            script,
            -1,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL);

        g_free(escaped_content);
        g_free(script);
        
        is_content_saved = TRUE;
        update_save_indicator();
    } else {
        webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
            "editor.setMarkdown('');",
            -1,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL);
    }
}

void update_notes_list() {
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(list_box));
    for (iter = children; iter != NULL; iter = g_list_next(iter))
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    g_list_free(children);

    for (int i = 0; i < noteCount; i++) {
        char label[50];
        snprintf(label, sizeof(label), "Note %d", notes[i].id);
        GtkWidget *label_widget = gtk_label_new(label);
        gtk_list_box_insert(GTK_LIST_BOX(list_box), label_widget, -1);
    }
    gtk_widget_show_all(list_box);
}

void get_editor_content(GtkWidget *widget, gpointer data) {
    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
        "editor.getMarkdown();",
        -1,
        NULL,
        NULL,
        NULL,
        handle_editor_content,
        NULL);
}

void handle_editor_content(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    GError *error = NULL;
    JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source_object), 
                                                               result, 
                                                               &error);
    if (error) {
        g_warning("Error getting content: %s", error->message);
        g_error_free(error);
        return;
    }

    if (jsc_value_is_string(value)) {
        char *markdown_content = jsc_value_to_string(value);
        GtkListBoxRow *selected_row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list_box));
        if (selected_row != NULL) {
            int index = gtk_list_box_row_get_index(selected_row);
            strncpy(notes[index].content, markdown_content, MAX_LENGTH - 1);
            notes[index].content[MAX_LENGTH - 1] = '\0';
        }
        g_free(markdown_content);
    }
    g_object_unref(value);
}

void update_window_title() {
    char *title;
    if (current_file_path) {
        char *filename = g_path_get_basename(current_file_path);
        title = g_strdup_printf("Markdown Notes App - %s%s", 
                               filename,
                               is_content_saved ? "" : " *");
        g_free(filename);
    } else {
        title = g_strdup_printf("Markdown Notes App%s", 
                               is_content_saved ? "" : " *");
    }
    gtk_window_set_title(GTK_WINDOW(window), title);
    g_free(title);
}

void mark_content_unsaved() {
    is_content_saved = FALSE;
    update_save_indicator();
    update_window_title();

    if (autosave_enabled && current_file_path) {
        save_current_content_to_file(current_file_path);
    }
}

void update_save_indicator() {
    gtk_label_set_text(GTK_LABEL(save_indicator_label), 
                      is_content_saved ? "Saved" : "Unsaved*");
}

gboolean autosave_callback(gpointer user_data) {
    if (autosave_enabled && current_file_path && !is_content_saved) {
        save_current_content_to_file(current_file_path);
    }
    return G_SOURCE_CONTINUE;
}

void toggle_autosave(GtkWidget *widget, gpointer data) {
    autosave_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    
    if (autosave_enabled) {
        if (autosave_timeout_id == 0) {
            // Set up autosave every 30 seconds
            autosave_timeout_id = g_timeout_add_seconds(30, autosave_callback, NULL);
        }
    } else {
        if (autosave_timeout_id > 0) {
            g_source_remove(autosave_timeout_id);
            autosave_timeout_id = 0;
        }
    }
}

gboolean check_unsaved_changes() {
    if (autosave_enabled && current_file_path) {
        save_current_content_to_file(current_file_path);
        return TRUE;
    }

    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_WARNING,
                                             GTK_BUTTONS_NONE,
                                             "There are unsaved changes. What would you like to do?");
    
    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                          "_Save", GTK_RESPONSE_YES,
                          "_Discard", GTK_RESPONSE_NO,
                          "_Cancel", GTK_RESPONSE_CANCEL,
                          NULL);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    switch (response) {
        case GTK_RESPONSE_YES:
            if (current_file_path) {
                save_current_content_to_file(current_file_path);
            } else {
                save_note_as(NULL, NULL);
            }
            return TRUE;
        case GTK_RESPONSE_NO:
            return TRUE;
        case GTK_RESPONSE_CANCEL:
        default:
            return FALSE;
    }
}

void rename_file(GtkWidget *menuitem, gpointer userdata) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *filepath;
        char *filename;
        gtk_tree_model_get(model, &iter, 
                          0, &filename,
                          1, &filepath, 
                          -1);

        GtkWidget *dialog = gtk_dialog_new_with_buttons("Rename File",
                                                       GTK_WINDOW(window),
                                                       GTK_DIALOG_MODAL,
                                                       "_Cancel",
                                                       GTK_RESPONSE_CANCEL,
                                                       "_Rename",
                                                       GTK_RESPONSE_ACCEPT,
                                                       NULL);

        GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        GtkWidget *entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), filename);
        gtk_container_add(GTK_CONTAINER(content_area), entry);
        gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            const char *new_filename = gtk_entry_get_text(GTK_ENTRY(entry));
            char *new_filepath = g_build_filename(vault_directory, new_filename, NULL);
            
            // Ensure .md extension
            if (!g_str_has_suffix(new_filepath, ".md")) {
                char *temp = g_strconcat(new_filepath, ".md", NULL);
                g_free(new_filepath);
                new_filepath = temp;
            }

            if (g_rename(filepath, new_filepath) == 0) {
                refresh_file_tree();
            } else {
                show_error_dialog("Failed to rename file");
            }
            g_free(new_filepath);
        }
        
        gtk_widget_destroy(dialog);
        g_free(filename);
        g_free(filepath);
    }
}

void delete_file(GtkWidget *menuitem, gpointer userdata) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *filepath;
        char *filename;
        gtk_tree_model_get(model, &iter, 
                          0, &filename,
                          1, &filepath, 
                          -1);

        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_QUESTION,
                                                 GTK_BUTTONS_YES_NO,
                                                 "Delete file '%s'?", filename);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
            if (g_unlink(filepath) == 0) {
                refresh_file_tree();
            } else {
                show_error_dialog("Failed to delete file");
            }
        }
        
        gtk_widget_destroy(dialog);
        g_free(filename);
        g_free(filepath);
    }
}

gboolean on_tree_button_press(GtkWidget *widget, GdkEventButton *event, gpointer userdata) {
    if (event->button == 3) { // Right click
        GtkWidget *menu = gtk_menu_new();
        GtkWidget *rename_item = gtk_menu_item_new_with_label("Rename");
        GtkWidget *delete_item = gtk_menu_item_new_with_label("Delete");

        g_signal_connect(rename_item, "activate", G_CALLBACK(rename_file), NULL);
        g_signal_connect(delete_item, "activate", G_CALLBACK(delete_file), NULL);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), rename_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
        return TRUE;
    }
    return FALSE;
}

void apply_dark_mode() {
    // Get the main window style context
    GtkStyleContext *window_context = gtk_widget_get_style_context(window);
    
    // Remove opposite theme class
    gtk_style_context_remove_class(window_context, dark_mode_enabled ? "light" : "dark");
    // Add current theme class
    gtk_style_context_add_class(window_context, dark_mode_enabled ? "dark" : "light");

    // Apply dark theme to WebKit content if available
    if (WEBKIT_IS_WEB_VIEW(web_view)) {
        const char *script = dark_mode_enabled ?
            "document.body.classList.add('dark-theme');" :
            "document.body.classList.remove('dark-theme');";
        
        webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
            script, -1, NULL, NULL, NULL, NULL, NULL);
    }

    // Force redraw
    if (GTK_IS_WIDGET(window)) {
        gtk_widget_queue_draw(window);
    }
}


void toggle_dark_mode(GtkWidget *widget, gpointer data) {
    dark_mode_enabled = gtk_switch_get_active(GTK_SWITCH(widget));
    apply_dark_mode();
}

void init_config() {
    // Create config directory if it doesn't exist
    char *config_dir = g_build_filename(g_get_home_dir(), ".config", "notes-gui", NULL);
    g_mkdir_with_parents(config_dir, 0755);
    
    // Set config file path
    config_file_path = g_build_filename(config_dir, "user.conf", NULL);
    g_free(config_dir);
    
    // Load config if it exists, otherwise create default
    if (g_file_test(config_file_path, G_FILE_TEST_EXISTS)) {
        load_config();
    } else {
        save_config();
    }
}

void load_config() {
    GKeyFile *keyfile = g_key_file_new();
    GError *error = NULL;
    
    if (g_key_file_load_from_file(keyfile, config_file_path, G_KEY_FILE_NONE, &error)) {
        char *saved_vault = g_key_file_get_string(keyfile, "Settings", "vault_directory", NULL);
        if (saved_vault) {
            g_free(vault_directory);
            vault_directory = saved_vault;
            refresh_file_tree();
            update_vault_label();
        }
        
        dark_mode_enabled = g_key_file_get_boolean(keyfile, "Settings", "dark_mode", NULL);
        if (dark_mode_enabled) {
            gtk_switch_set_active(GTK_SWITCH(dark_mode_switch), TRUE);
        }
        
        preview_hidden = g_key_file_get_boolean(keyfile, "Settings", "preview_hidden", NULL);
        if (preview_toggle_switch) {
            gtk_switch_set_active(GTK_SWITCH(preview_toggle_switch), preview_hidden);
        }
        
        apply_dark_mode();
        
        char *last_file = g_key_file_get_string(keyfile, "Settings", "last_file", NULL);
        if (last_file && g_file_test(last_file, G_FILE_TEST_EXISTS)) {
            g_free(current_file_path);
            current_file_path = last_file;
            
            char *content = NULL;
            if (g_file_get_contents(current_file_path, &content, NULL, NULL)) {
                char *escaped_content = g_markup_escape_text(content, -1);
                char *script = g_strdup_printf("editor.setMarkdown(`%s`);", escaped_content);
                webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
                    script, -1, NULL, NULL, NULL, NULL, NULL);
                g_free(script);
                g_free(escaped_content);
                g_free(content);
                is_content_saved = TRUE;
                update_save_indicator();
                update_window_title();
                show_editor();
            }
        }
    }
    
    g_key_file_free(keyfile);
}
void save_config() {
    GKeyFile *keyfile = g_key_file_new();
    
    if (vault_directory) {
        g_key_file_set_string(keyfile, "Settings", "vault_directory", vault_directory);
    }
    
    g_key_file_set_boolean(keyfile, "Settings", "dark_mode", dark_mode_enabled);
    g_key_file_set_boolean(keyfile, "Settings", "preview_hidden", preview_hidden);
    
    if (current_file_path) {
        g_key_file_set_string(keyfile, "Settings", "last_file", current_file_path);
    }
    
    GError *error = NULL;
    if (!g_key_file_save_to_file(keyfile, config_file_path, &error)) {
        g_warning("Failed to save config: %s", error->message);
        g_error_free(error);
    }
    
    g_key_file_free(keyfile);
}



void update_vault_label() {
    if (vault_directory) {
        char *label_text = g_strdup_printf("Vault: %s", vault_directory);
        gtk_label_set_text(GTK_LABEL(vault_label), label_text);
        g_free(label_text);
    } else {
        gtk_label_set_text(GTK_LABEL(vault_label), "No vault selected");
    }
}

void register_web_handlers(WebKitUserContentManager *manager) {
    webkit_user_content_manager_register_script_message_handler(manager, "contentChanged");
    webkit_user_content_manager_register_script_message_handler(manager, "newNote");
    webkit_user_content_manager_register_script_message_handler(manager, "openFile");
    webkit_user_content_manager_register_script_message_handler(manager, "editorInitialized");
    
    g_signal_connect(manager, "script-message-received::contentChanged", 
                     G_CALLBACK(mark_content_unsaved), NULL);
    g_signal_connect(manager, "script-message-received::newNote", 
                     G_CALLBACK(handle_new_note), NULL);
    g_signal_connect(manager, "script-message-received::openFile", 
                     G_CALLBACK(handle_open_file), NULL);
    g_signal_connect(manager, "script-message-received::editorInitialized", 
                     G_CALLBACK(handle_editor_initialized), NULL);
}

void handle_editor_initialized(WebKitUserContentManager *manager, 
                             WebKitJavascriptResult *js_result, 
                             gpointer user_data) {
    // Apply initial settings
    if (dark_mode_enabled) {
        apply_dark_mode();
    }

    if (preview_hidden) {
        char *script = g_strdup_printf("window.togglePreview(!Boolean(%s));", 
                                     preview_hidden ? "true" : "false");
        webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
            script, -1, NULL, NULL, NULL, NULL, NULL);
        g_free(script);
    }

    // Show the appropriate view
    if (current_file_path) {
        show_editor();
    } else {
        show_start_page();
    }
}


void update_recent_files() {
    if (!web_view) return;
    
    // Create JSON array of recent files
    GString *json = g_string_new("[");
    // Add your recent files logic here
    // Example:
    if (vault_directory) {
        GDir *dir = g_dir_open(vault_directory, 0, NULL);
        if (dir) {
            const gchar *filename;
            gboolean first = TRUE;
            while ((filename = g_dir_read_name(dir))) {
                if (g_str_has_suffix(filename, ".md")) {
                    if (!first) g_string_append(json, ",");
                    char *full_path = g_build_filename(vault_directory, filename, NULL);
                    g_string_append_printf(json, 
                        "{\"name\":\"%s\",\"path\":\"%s\"}", 
                        filename, full_path);
                    g_free(full_path);
                    first = FALSE;
                }
            }
            g_dir_close(dir);
        }
    }
    g_string_append(json, "]");

    char *script = g_strdup_printf("updateRecentFiles(%s)", json->str);
    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
        script, -1, NULL, NULL, NULL, NULL, NULL);
    
    g_string_free(json, TRUE);
    g_free(script);
}

void show_start_page() {
    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
        "showStartPage()", -1, NULL, NULL, NULL, NULL, NULL);
    update_recent_files();
}

void show_editor() {
    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(web_view),
        "showEditor()", -1, NULL, NULL, NULL, NULL, NULL);
}

void handle_new_note(WebKitUserContentManager *manager, 
                    WebKitJavascriptResult *js_result, 
                    gpointer user_data) {
    add_note(NULL, NULL);
}

void handle_open_file(WebKitUserContentManager *manager, 
                     WebKitJavascriptResult *js_result, 
                     gpointer user_data) {
    JSCValue *val = webkit_javascript_result_get_js_value(js_result);
    char *filepath = jsc_value_to_string(val);
    
    // Find and select the file in the tree view
    GtkTreeModel *model = GTK_TREE_MODEL(tree_store);
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        char *path;
        gtk_tree_model_get(model, &iter, 1, &path, -1);
        if (g_strcmp0(path, filepath) == 0) {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
            gtk_tree_selection_select_iter(selection, &iter);
            g_free(path);
            break;
        }
        g_free(path);
        valid = gtk_tree_model_iter_next(model, &iter);
    }
    
    g_free(filepath);
}

// Add function to get the resource file paths
char* get_resource_path(const char* filename) {
    char *exe_path = realpath("/proc/self/exe", NULL);
    if (!exe_path) return NULL;
    
    char *exe_dir = g_path_get_dirname(exe_path);
    free(exe_path);
    
    char *resources_dir = g_build_filename(exe_dir, "resources", NULL);
    char *file_path = g_build_filename(resources_dir, filename, NULL);
    
    g_free(resources_dir);
    g_free(exe_dir);
    
    return file_path;
}


char* get_asset_path(const char* filename) {
    char *exe_path = realpath("/proc/self/exe", NULL);
    if (!exe_path) {
        g_warning("Failed to get executable path");
        return NULL;
    }
    
    char *exe_dir = g_path_get_dirname(exe_path);
    free(exe_path);
    
    char *assets_dir = g_build_filename(exe_dir, "assets", NULL);
    char *file_path = g_build_filename(assets_dir, filename, NULL);
    
    if (!g_file_test(assets_dir, G_FILE_TEST_EXISTS)) {
        g_warning("Assets directory does not exist: %s", assets_dir);
        g_free(assets_dir);
        g_free(exe_dir);
        g_free(file_path);
        return NULL;
    }
    
    g_free(assets_dir);
    g_free(exe_dir);
    
    return file_path;
}

char* get_file_contents(const char* path) {
    char *contents = NULL;
    GError *error = NULL;
    
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_warning("File does not exist: %s", path);
        return NULL;
    }
    
    if (g_file_get_contents(path, &contents, NULL, &error)) {
        return contents;
    } else {
        g_warning("Failed to read file %s: %s", path, error->message);
        g_error_free(error);
        return NULL;
    }
}

void apply_gtk_css() {
    char *css_path = get_asset_path("style.css");
    if (!css_path) {
        g_warning("Could not find style.css");
        return;
    }

    GtkCssProvider *provider = gtk_css_provider_new();
    GError *error = NULL;

    gtk_css_provider_load_from_path(provider, css_path, &error);
    if (error) {
        g_warning("Failed to load CSS: %s", error->message);
        g_error_free(error);
        g_object_unref(provider);
        g_free(css_path);
        return;
    }

    // Apply to all screens
    GdkScreen *screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(screen,
                                            GTK_STYLE_PROVIDER(provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(provider);
    g_free(css_path);
}


void setup_css_provider(void) {
    if (css_provider != NULL) {
        return;
    }

    css_provider = gtk_css_provider_new();
    char *css_path = get_asset_path("main.css");
    if (css_path) {
        gtk_css_provider_load_from_path(css_provider, css_path, NULL);
        GdkScreen *screen = gdk_screen_get_default();
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        g_free(css_path);
    }
}
