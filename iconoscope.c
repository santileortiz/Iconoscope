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
int file_lookup (char *dir, char *file)
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
        int cmp = strncmp (file, entry_info.d_name, MIN (strlen(file), strlen(entry_info.d_name)));
        if (cmp == 0) {
            res++;
        }
    }
    return res;
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

        if (file_lookup (str_data (&path), icon_name)) {
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

int main(int argc, char *argv[])
{
    GtkWidget *window;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(window), "GTK window");

    g_signal_connect (G_OBJECT(window), "delete-event", G_CALLBACK (delete_callback), NULL);
    g_signal_connect (G_OBJECT(window), "key_press_event", G_CALLBACK (on_key_press), NULL);

    GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
    gint *sizes = gtk_icon_theme_get_icon_sizes (icon_theme, "io.elementary.code");

    gchar **path;
    gint num_paths;
    gtk_icon_theme_get_search_path (icon_theme, &path, &num_paths);
    int i;
    for (i=0; i<num_paths; i++) {
        printf ("%s\n", path[i]);
    }

    while (*sizes) {
        printf ("%d ", *sizes++);
    }
    printf ("\n");

    print_icon_sizes ("io.elementary.code");

    gtk_widget_set_size_request (window, 700, 700);
    //GtkWidget *image = gtk_image_new_from_file (".svg");
    //gtk_container_add(GTK_CONTAINER(window), image);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}
