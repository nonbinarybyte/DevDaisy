#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct {
    GtkWidget *view;
    GtkWidget *scroll;
    GtkWidget *label;
    char *filepath;
} EditorTab;

GtkWidget *notebook;
GtkWidget *file_tree;
GtkWidget *file_tree_store;
GtkWidget *file_tree_view;
GtkWidget *file_tree_scroller;
GtkWidget *main_box;
gboolean dark_mode_enabled = FALSE;
guint autosave_interval_id = 0;

EditorTab* get_current_editor() {
    GtkWidget *tab_content = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook),
                                                        gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
    return g_object_get_data(G_OBJECT(tab_content), "editor_tab");
}

void set_dark_mode(GtkSourceBuffer *buffer, gboolean enable_dark) {
    GtkSourceStyleSchemeManager *ssm = gtk_source_style_scheme_manager_get_default();
    GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(
        ssm, enable_dark ? "oblivion" : "classic");
    gtk_source_buffer_set_style_scheme(buffer, scheme);
}

void autosave_current_tab() {
    EditorTab *tab = get_current_editor();
    if (!tab || !tab->filepath) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab->view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    g_file_set_contents(tab->filepath, text, -1, NULL);
    g_free(text);
}

gboolean autosave_callback(gpointer data) {
    autosave_current_tab();
    return TRUE;
}

void close_tab(GtkButton *button, gpointer user_data) {
    gint page_num = GPOINTER_TO_INT(user_data);
    GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page_num);
    gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), page_num);
    gtk_widget_destroy(child);
}

GtkWidget* create_tab_label(const char *title, gint page_num) {
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *label = gtk_label_new(title);
    GtkWidget *close_button = gtk_button_new_with_label("x");
    gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);
    gtk_widget_set_size_request(close_button, 20, 20);
    g_signal_connect_swapped(close_button, "clicked", G_CALLBACK(close_tab), GINT_TO_POINTER(page_num));

    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), close_button, FALSE, FALSE, 0);
    gtk_widget_show_all(hbox);
    return hbox;
}

void create_new_tab(const char *title, const char *content, const char *filepath, const char *lang_id, gboolean dark_mode) {
    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    GtkSourceLanguage *lang = lang_id ? gtk_source_language_manager_get_language(lm, lang_id) : NULL;

    GtkSourceBuffer *buffer = gtk_source_buffer_new_with_language(lang);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), content ? content : "", -1);

    if (dark_mode) set_dark_mode(buffer, TRUE);

    GtkWidget *view = gtk_source_view_new_with_buffer(buffer);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(view), TRUE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(view), TRUE);
    gtk_source_view_set_auto_indent(GTK_SOURCE_VIEW(view), TRUE);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), view);

    gint page_num = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scroll, NULL);
    GtkWidget *tab_label = create_tab_label(title, page_num);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook), scroll, tab_label);
    gtk_widget_show_all(scroll);

    EditorTab *tab = g_new(EditorTab, 1);
    tab->view = view;
    tab->scroll = scroll;
    tab->label = tab_label;
    tab->filepath = filepath ? g_strdup(filepath) : NULL;

    g_object_set_data(G_OBJECT(scroll), "editor_tab", tab);
}

const char* detect_language_from_extension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return NULL;

    if (strcmp(ext, ".c") == 0) return "c";
    else if (strcmp(ext, ".cpp") == 0) return "cpp";
    else if (strcmp(ext, ".cs") == 0) return "c-sharp";
    else if (strcmp(ext, ".java") == 0) return "java";
    else if (strcmp(ext, ".go") == 0) return "go";
    else if (strcmp(ext, ".asm") == 0 || strcmp(ext, ".s") == 0) return "nasm";
    else if (strcmp(ext, ".html") == 0) return "html";
    else if (strcmp(ext, ".css") == 0) return "css";
    else if (strcmp(ext, ".js") == 0) return "javascript";

    return NULL;
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
            const char *lang_id = detect_language_from_extension(filename);
            create_new_tab(g_path_get_basename(filename), content, filename, lang_id, dark_mode_enabled);
            g_free(content);
        }

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

void save_file(GtkWidget *widget, gpointer user_data) {
    autosave_current_tab();
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

void toggle_dark_mode(GtkCheckMenuItem *item, gpointer user_data) {
    dark_mode_enabled = gtk_check_menu_item_get_active(item);

    int n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    for (int i = 0; i < n_pages; ++i) {
        GtkWidget *tab_content = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i);
        EditorTab *tab = g_object_get_data(G_OBJECT(tab_content), "editor_tab");
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab->view));
        set_dark_mode(GTK_SOURCE_BUFFER(buffer), dark_mode_enabled);
    }
}

void add_file_to_tree(const char *path, GtkTreeStore *store, GtkTreeIter *parent) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        GtkTreeIter iter;
        gtk_tree_store_append(store, &iter, parent);
        gtk_tree_store_set(store, &iter, 0, entry->d_name, 1, g_strdup(full_path), -1);

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            add_file_to_tree(full_path, store, &iter);
        }
    }

    closedir(dir);
}

void on_tree_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                           GtkTreeViewColumn *column, gpointer user_data) {
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gchar *file_path;
        gtk_tree_model_get(model, &iter, 1, &file_path, -1);

        char *content;
        gsize length;
        if (g_file_get_contents(file_path, &content, &length, NULL)) {
            const char *lang_id = detect_language_from_extension(file_path);
            create_new_tab(g_path_get_basename(file_path), content, file_path, lang_id, dark_mode_enabled);
            g_free(content);
        }

        g_free(file_path);
    }
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Dev Daisy");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);
    gtk_window_set_icon_from_file(GTK_WINDOW(window), "icon.png", NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(main_box), vbox, TRUE, TRUE, 0);

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

    GtkWidget *settings_menu = gtk_menu_new();
    GtkWidget *settings_item = gtk_menu_item_new_with_label("Settings");
    GtkWidget *dark_mode_item = gtk_check_menu_item_new_with_label("Dark Mode");
    gtk_menu_shell_append(GTK_MENU_SHELL(settings_menu), dark_mode_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(settings_item), settings_menu);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), settings_item);
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);

    GtkTreeStore *store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    file_tree_store = GTK_WIDGET(store);
    add_file_to_tree(".", store, NULL);

    file_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(file_tree_view), column);
    g_signal_connect(file_tree_view, "row-activated", G_CALLBACK(on_tree_row_activated), NULL);

    file_tree_scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(file_tree_scroller), file_tree_view);
    gtk_widget_set_size_request(file_tree_scroller, 200, -1);
    gtk_box_pack_start(GTK_BOX(main_box), file_tree_scroller, FALSE, FALSE, 0);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    g_signal_connect(open_item, "activate", G_CALLBACK(open_file), NULL);
    g_signal_connect(save_item, "activate", G_CALLBACK(save_file), NULL);
    g_signal_connect(run_item, "activate", G_CALLBACK(build_and_run), NULL);
    g_signal_connect(dark_mode_item, "toggled", G_CALLBACK(toggle_dark_mode), NULL);

    autosave_interval_id = g_timeout_add_seconds(60, autosave_callback, NULL);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
