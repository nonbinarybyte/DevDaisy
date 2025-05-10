#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    GtkWidget *view;
    GtkWidget *scroll;
    GtkWidget *label;
    char *filepath;
} EditorTab;

GtkWidget *notebook;

// Get current tab's EditorTab struct
EditorTab* get_current_editor() {
    GtkWidget *tab_content = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook),
                                                        gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
    return g_object_get_data(G_OBJECT(tab_content), "editor_tab");
}

void create_new_tab(const char *title, const char *content, const char *filepath, const char *lang_id) {
    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    GtkSourceLanguage *lang = lang_id ? gtk_source_language_manager_get_language(lm, lang_id) : NULL;

    GtkSourceBuffer *buffer = gtk_source_buffer_new_with_language(lang);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), content ? content : "", -1);

    GtkWidget *view = gtk_source_view_new_with_buffer(buffer);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(view), TRUE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(view), TRUE);
    gtk_source_view_set_auto_indent(GTK_SOURCE_VIEW(view), TRUE);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), view);

    GtkWidget *label = gtk_label_new(title);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scroll, label);
    gtk_widget_show_all(scroll);

    EditorTab *tab = g_new(EditorTab, 1);
    tab->view = view;
    tab->scroll = scroll;
    tab->label = label;
    tab->filepath = filepath ? g_strdup(filepath) : NULL;

    g_object_set_data(G_OBJECT(scroll), "editor_tab", tab);
}

void open_file(GtkWidget *widget, gpointer user_data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open File", NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char *content;
        gsize length;

        if (g_file_get_contents(filename, &content, &length, NULL)) {
            // Detect language
            const char *ext = strrchr(filename, '.');
            const char *lang_id = NULL;
            if (ext) {
                if (strcmp(ext, ".c") == 0) lang_id = "c";
                else if (strcmp(ext, ".cpp") == 0) lang_id = "cpp";
                else if (strcmp(ext, ".html") == 0) lang_id = "html";
                else if (strcmp(ext, ".css") == 0) lang_id = "css";
                else if (strcmp(ext, ".js") == 0) lang_id = "javascript";
            }

            create_new_tab(g_path_get_basename(filename), content, filename, lang_id);
            g_free(content);
        }

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

void save_file(GtkWidget *widget, gpointer user_data) {
    EditorTab *tab = get_current_editor();
    if (!tab) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab->view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    if (tab->filepath) {
        g_file_set_contents(tab->filepath, text, -1, NULL);
    } else {
        GtkWidget *dialog = gtk_file_chooser_dialog_new("Save File", NULL,
            GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL,
            "_Save", GTK_RESPONSE_ACCEPT, NULL);
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            tab->filepath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            g_file_set_contents(tab->filepath, text, -1, NULL);
        }
        gtk_widget_destroy(dialog);
    }

    g_free(text);
}

void build_and_run(GtkWidget *widget, gpointer user_data) {
    EditorTab *tab = get_current_editor();
    if (!tab || !tab->filepath) {
        g_print("Save the file before running.\n");
        return;
    }

    const char *ext = strrchr(tab->filepath, '.');
    if (!ext) return;

    char command[512];
    char *output;

    if (strcmp(ext, ".c") == 0 || strcmp(ext, ".cpp") == 0) {
        snprintf(command, sizeof(command),
            "gcc \"%s\" -o /tmp/editor_run && /tmp/editor_run", tab->filepath);
    } else if (strcmp(ext, ".html") == 0 || strcmp(ext, ".js") == 0) {
        snprintf(command, sizeof(command), "xdg-open \"%s\"", tab->filepath);
    } else {
        g_print("Build/Run not supported for this file type.\n");
        return;
    }

    g_print("Running: %s\n", command);
    system(command);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Tabbed Code Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *menu_bar = gtk_menu_bar_new();
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_item = gtk_menu_item_new_with_label("File");
    GtkWidget *open_item = gtk_menu_item_new_with_label("Open");
    GtkWidget *save_item = gtk_menu_item_new_with_label("Save");
    GtkWidget *run_item = gtk_menu_item_new_with_label("Build/Run");

    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), run_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_item);
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    g_signal_connect(open_item, "activate", G_CALLBACK(open_file), NULL);
    g_signal_connect(save_item, "activate", G_CALLBACK(save_file), NULL);
    g_signal_connect(run_item, "activate", G_CALLBACK(build_and_run), NULL);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}

