#include <cairo.h>
#include <gtk/gtk.h>

#include "common.h"

#define ICON_PATH "/usr/share/icons/hicolor/"

// INI or desktop file format parser.
//
// The idea of the following functions is to allow seeking through a INI format
// file without making any allocations. All returned strings point into the
// original file string and are NOT null terminated. The following code shows
// how to use them by printing back the input file:
//
//    char *theme_index = full_file_read (&pool, str_data(&path));
//    char *c = theme_index;
//    while (*c) {
//        char *section_name;
//        uint32_t section_name_len;
//        c = seek_next_section (c, &section_name, &section_name_len);
//        printf ("[%.*s]\n", section_name_len, section_name);
//
//        while ((c = consume_ignored_lines (c)) && !is_end_of_section(c)) {
//            char *key, *value;
//            uint32_t key_len, value_len;
//            c = seek_next_key_value (c, &key, &key_len, &value, &value_len);
//            printf ("%.*s=%.*s\n", key_len, key, value_len, value);
//        }
//        printf ("\n");
//    }

char *seek_next_section (char *c, char **section_name, uint32_t *section_name_len)
{
    while (*c && *c != '[') {
        c = consume_line (c);
    }

    if (*c == '\0') {
        // NOTE: There are no more sections
        return c;
    }

    c++;

    uint32_t len = 0;
    while (*(c + len) && *(c + len) != ']') {
        len++;
    }

    if (*(c + len) == '\0' || !is_end_of_line_or_file(c + len + 1)) {
        printf ("Syntax error in INI/desktop file.\n");
        return c;
    }

    if (section_name != NULL) {
        *section_name = c;
    }
    if (section_name_len != NULL) {
        *section_name_len = len;
    }
    return consume_line (c);
}

char *seek_next_key_value (char *c,
                           char **key, uint32_t *key_len,
                           char **value, uint32_t *value_len)
{
    if (*c == '[') {
        // NOTE: End of section
        return c;
    }

    uint32_t len = 0;
    while (*(c + len) && *(c + len) != '=' && !is_space (c)) {
        len++;
    }

    *key = c;
    *key_len = len;

    c = consume_spaces (c+len);

    if (*c != '=') {
        printf ("Syntax error in INI/desktop file.\n");
        return c;
    }

    c++;
    c = consume_spaces (c);

    len = 0;
    while (*(c + len) && !is_end_of_line_or_file (c + len)) {
        len++;
    }

    *value = c;
    *value_len = len;
    return consume_line (c);
}

char *consume_ignored_lines (char *c)
{
    while (*c && (is_end_of_line(c) || *c == ';' || *c == '#')) {
        c = consume_line (c);
    }
    return c;
}

char *consume_section (char *c)
{
    while (*c && *c == '[') {
        c = consume_line (c);
    }
    return c;
}

bool is_end_of_section (char *c)
{
    return *c == '[' || *c == '\0';
}

// Returns the number of times file exists inside dir.
// NOTE: The value can be >1 because file MUST not contain any extension so
// there may be repetitions.
int file_lookup_no_ext (char *dir, char *file)
{
    struct stat st;
    if (stat(dir, &st) == -1 && errno == ENOENT) {
        // NOTE: There are index.theme files that have entries for @2
        // directories, even though such directories do not exist in the system.
        //printf ("No directory named: %s\n", dir);
        return 0;
    }

    int res = 0;
    DIR *d = opendir (dir);
    struct dirent entry_info, *info_res;
    while (readdir_r (d, &entry_info, &info_res) == 0 && info_res != NULL) {
        uint32_t file_len = strlen(file);
        uint32_t entry_len = strlen(entry_info.d_name);
        int cmp = strncmp (file, entry_info.d_name, MIN (file_len, entry_len));
        if (cmp == 0) {
            if (file_len == entry_len ||
                (file_len < entry_len && entry_info.d_name[file_len] == '.')) {
                res++;
            }
        }
    }
    closedir (d);
    return res;
}

bool file_lookup (char *dir, char *file)
{
    struct stat st;
    if (stat(dir, &st) == -1 && errno == ENOENT) {
        //printf ("No directory named: %s\n", dir);
        return 0;
    }

    bool res = false;
    DIR *d = opendir (dir);
    struct dirent entry_info, *info_res;
    while (readdir_r (d, &entry_info, &info_res) == 0 && info_res != NULL) {
        uint32_t file_len = strlen(file);
        uint32_t entry_len = strlen(entry_info.d_name);
        int cmp = strncmp (file, entry_info.d_name, MIN (file_len, entry_len));
        if (cmp == 0) {
            if (file_len == entry_len) {
                res = true;
                break;
            }
        }
    }
    closedir (d);
    return res;
}

char* get_theme_name (mem_pool_t *pool, char *path)
{
    char *theme_index = full_file_read (NULL, path);
    char *c = theme_index;

    char *section_name;
    uint32_t section_name_len;
    c = seek_next_section (c, &section_name, &section_name_len);

    char *res;
    char *name_str = "Name";
    while ((c = consume_ignored_lines (c)) && *c && !is_end_of_section(c)) {
        char *key, *value;
        uint32_t key_len, value_len;
        c = seek_next_key_value (c, &key, &key_len, &value, &value_len);
        if (strlen (name_str) == key_len && strncmp (name_str, key, key_len) == 0) {
            res = pom_push_size (pool, value_len+1);
            memcpy (res, value, value_len);
            res[value_len] = '\0';
        }

    }
    free (theme_index);
    return res;
}

struct icon_theme_t {
    char *name;
    char *dir;
};

struct it_da_t {
    struct icon_theme_t *data;
    uint32_t len;
    uint32_t capacity;
};

void it_da_append (struct it_da_t *arr, struct icon_theme_t *e)
{
    if (arr->capacity == 0) {
        arr->capacity = 10;
        arr->data = malloc (arr->capacity * sizeof (struct icon_theme_t));

    } else if (arr->len + 1 == arr->capacity) {
        arr->capacity *= 2;
        struct icon_theme_t *tmp = realloc (arr->data, arr->capacity*sizeof (struct icon_theme_t));
        if (tmp == NULL) {
            printf ("Realloc failed.\n");
        } else {
            arr->data = tmp;
        }
    }

    arr->data[arr->len] = *e;
    arr->len++;
}

void it_da_free (struct it_da_t *arr)
{
    free (arr->data);
}

void append_icon_themes (mem_pool_t *pool, char *path_c, struct it_da_t *themes_arr)
{
    string_t path = str_new (path_c);
    if (str_last(&path) != '/') {
        str_cat_c (&path, "/");
    }
    uint32_t path_len = str_len (&path);

    struct stat st;
    if (stat(path_c, &st) == -1 && errno == ENOENT) {
        // NOTE: Current search paths may contain non existent directories.
        //printf ("Search path does not exist.\n");
        return;
    }

    DIR *d = opendir (path_c);
    struct dirent entry_info, *info_res;
    while (readdir_r (d, &entry_info, &info_res) == 0 && info_res != NULL) {
        if (strcmp ("default", entry_info.d_name) == 0) {
            continue;
        } else if (entry_info.d_name[0] != '.') {
            string_t theme_dir = str_new (entry_info.d_name);
            str_put (&path, path_len, &theme_dir);

            if (stat(str_data(&path), &st) == 0 && S_ISDIR(st.st_mode) &&
                file_lookup (str_data(&path), "index.theme")) {

                struct icon_theme_t theme;
                theme.dir = pom_push_size (pool, str_len(&path)+1);
                memcpy (theme.dir, str_data(&path), str_len(&path)+1);

                str_cat_c (&path, "/index.theme");
                theme.name = get_theme_name(pool, str_data(&path));
                it_da_append (themes_arr, &theme);
            }
            str_free (&theme_dir);
        }
    }

    closedir (d);
    str_free (&path);
}

void print_icon_sizes (char *icon_name)
{
    mem_pool_t pool = {0};

    string_t path = str_new (ICON_PATH);
    uint32_t path_len = str_len (&path);
    str_cat_c (&path, "index.theme");

    char *theme_index = full_file_read (&pool, str_data(&path));
    char *c = theme_index;

    // Ignore the first section: [Icon Theme]
    c = seek_next_section (c, NULL, NULL);
    c = consume_section (c);

    int sizes [50];
    int num_sizes = 0;
    char buff [4];
    while (*c) {
        char *section_name;
        uint32_t section_name_len;
        c = seek_next_section (c, &section_name, &section_name_len);
        string_t dir = strn_new (section_name, section_name_len);
        str_put (&path, path_len, &dir);

        if (file_lookup_no_ext (str_data (&path), icon_name)) {
            while ((c = consume_ignored_lines (c)) && !is_end_of_section(c)) {
                char *key, *value;
                uint32_t key_len, value_len;
                c = seek_next_key_value (c, &key, &key_len, &value, &value_len);
                if (strncmp (key, "Size", MIN(4, key_len)) == 0) {
                    memcpy (buff, value, value_len);
                    buff [value_len] = '\0';
                    int_array_set_insert (atoi (buff), sizes, &num_sizes, ARRAY_SIZE (sizes));
                }
            }
        } else {
            c = consume_section (c);
        }

        str_free (&dir);
    }

    int i;
    for (i=0; i < num_sizes; i++) {
        printf ("%d ", sizes[i]);
    }
    printf ("\n");
}

gboolean delete_callback (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    gtk_main_quit ();
    return FALSE;
}

gboolean on_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (event->keyval == GDK_KEY_q){
        gtk_main_quit ();
        return TRUE;
    }
    return FALSE;
}

void get_all_icon_themes (mem_pool_t *pool, struct icon_theme_t **themes, uint32_t *num_themes)
{
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
    gchar **path;
    gint num_paths;
    gtk_icon_theme_get_search_path (icon_theme, &path, &num_paths);

    struct it_da_t themes_arr = {0};
    int i;
    for (i=0; i<num_paths; i++) {
        append_icon_themes (pool, path[i], &themes_arr);
    }

    if (themes_arr.len > 0) {
        *num_themes = themes_arr.len;
        *themes = pom_push_size (pool, sizeof (struct icon_theme_t) * themes_arr.len);
        memcpy (*themes, themes_arr.data, sizeof(struct icon_theme_t) * themes_arr.len);
    }

    it_da_free (&themes_arr);
}

gint str_cmp_callback (gconstpointer a, gconstpointer b)
{
    // NOTE: The flip in arguments' order is intentional, we want to sort
    // backwards because GtkListBox has only a prepend method.
    return g_strcmp0 ((const char*)b, (const char*)a);
}

GtkWidget *scrolled_icon_list  = NULL, *icon_list = NULL;
void on_theme_changed (GtkComboBox *themes_combobox, gpointer user_data)
{
    // NOTE: We can't use:
    //    gtk_container_remove(GTK_CONTAINER(scrolled_icon_list), icon_list)
    // because gtk_container_add() wraps icon_list in a GtkViewport so
    // scrolled_icon_list isn't actually the parent of icon_list.
    gtk_container_remove (GTK_CONTAINER(scrolled_icon_list),
                          gtk_bin_get_child (GTK_BIN (scrolled_icon_list)));

    // Build a new widget with the list of icons for the chosen theme
    GtkIconTheme *icon_theme = gtk_icon_theme_new ();
    gchar *theme_name = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(themes_combobox));
    gtk_icon_theme_set_custom_theme (icon_theme, theme_name);

    icon_list = gtk_list_box_new ();
    gtk_widget_set_vexpand (icon_list, TRUE);
    gtk_widget_set_hexpand (icon_list, TRUE);
    GList *icon_names = gtk_icon_theme_list_icons (icon_theme, NULL);
    icon_names = g_list_sort (icon_names, str_cmp_callback);

    uint32_t i = 0;
    GList *l;
    for (l = icon_names; l != NULL; l = l->next)
    {
        // NOTE: Ignore symbolic icons for now
        if (g_str_has_suffix (l->data, "symbolic")) {
            continue;
        }

        GtkWidget *row = gtk_label_new (l->data);
        gtk_list_box_prepend (GTK_LIST_BOX(icon_list), row);
        gtk_widget_set_halign (row, GTK_ALIGN_START);

        gtk_widget_set_margin_start (row, 6);
        i++;
    }
    printf ("%u\n", i);

    gtk_container_add (GTK_CONTAINER(scrolled_icon_list), icon_list);
    gtk_widget_show_all(icon_list);
}

int main(int argc, char *argv[])
{
    GtkWidget *window;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(window), "Iconoscope");

    g_signal_connect (G_OBJECT(window), "delete-event", G_CALLBACK (delete_callback), NULL);
    g_signal_connect (G_OBJECT(window), "key_press_event", G_CALLBACK (on_key_press), NULL);

    GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
    gint *sizes = gtk_icon_theme_get_icon_sizes (icon_theme, "io.elementary.code");
    while (*sizes) {
        printf ("%d ", *sizes++);
    }
    printf ("\n");
    print_icon_sizes ("io.elementary.code");

    mem_pool_t pool = {0};
    struct icon_theme_t *themes;
    uint32_t num_themes;
    get_all_icon_themes (&pool, &themes, &num_themes);

    icon_list = gtk_list_box_new ();
    gtk_widget_set_vexpand (icon_list, TRUE);

    GtkWidget *themes_label = gtk_label_new ("Theme:");
    GtkStyleContext *ctx = gtk_widget_get_style_context (themes_label);
    gtk_style_context_add_class (ctx, "h5");
    gtk_widget_set_margin_start (themes_label, 6);

    GtkWidget *themes_combobox = gtk_combo_box_text_new ();
    gtk_widget_set_margin_top (themes_combobox, 6);
    gtk_widget_set_margin_bottom (themes_combobox, 6);
    gtk_widget_set_margin_start (themes_combobox, 12);
    gtk_widget_set_margin_end (themes_combobox, 6);
    int i;
    for (i=0; i<num_themes; i++) {
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(themes_combobox), themes[i].name);
    }
    g_signal_connect (G_OBJECT(themes_combobox), "changed", G_CALLBACK (on_theme_changed), NULL);

    GtkWidget *theme_selector = gtk_grid_new ();
    gtk_grid_attach (GTK_GRID(theme_selector), themes_label, 0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID(theme_selector), themes_combobox, 1, 0, 1, 1);

    GtkWidget *sidebar = gtk_grid_new ();
    gtk_grid_attach (GTK_GRID(sidebar), theme_selector, 0, 1, 1, 1);

    scrolled_icon_list = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scrolled_icon_list), icon_list);
    gtk_grid_attach (GTK_GRID(sidebar), scrolled_icon_list, 0, 0, 1, 1);

    GtkWidget *image = gtk_image_new_from_file ("/usr/share/icons/hicolor/48x48/apps/io.elementary.code.svg");
    gtk_widget_set_size_request (image, 500, 400);

    GtkWidget *paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1 (GTK_PANED(paned), sidebar, TRUE, FALSE);
    gtk_paned_pack2 (GTK_PANED(paned), image, TRUE, TRUE);

    gtk_combo_box_set_active (GTK_COMBO_BOX(themes_combobox), 0);

    gtk_container_add(GTK_CONTAINER(window), paned);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}
