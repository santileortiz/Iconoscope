#include <cairo.h>
#include <gtk/gtk.h>

#include "common.h"

#define ICON_PATH "/usr/share/icons/hicolor/"

char *consume_line (char *c)
{
    while (*c && *c != '\n') {
           c++;
    }
    return ++c;
}

bool is_space (char *c)
{
    return *c == ' ' ||  *c == '\t';
}

char *consume_spaces (char *c)
{
    while (is_space(c)) {
           c++;
    }
    return c;
}

bool is_end_of_line_or_file (char *c)
{
    c = consume_spaces (c);
    return *c == '\n' || *c == '\0';
}

bool is_end_of_line (char *c)
{
    c = consume_spaces (c);
    return *c == '\n';
}

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

    *section_name = c;
    *section_name_len = len;
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

bool is_end_of_section (char *c)
{
    return *c == '[' || *c == '\0';
}

void print_icon_sizes (char *icon_name)
{
    mem_pool_t pool = {0};

    string_t path = str_new (ICON_PATH);
    str_cat_c (&path, "index.theme");

    char *theme_index = full_file_read (&pool, str_data(&path));
    char *c = theme_index;

    int sizes [50];
    int num_sizes = 0;
    char buff [4];
    while (*c) {
        char *section_name;
        uint32_t section_name_len;
        c = seek_next_section (c, &section_name, &section_name_len);

        while ((c = consume_ignored_lines (c)) && !is_end_of_section(c)) {
            char *key, *value;
            uint32_t key_len, value_len;
            c = seek_next_key_value (c, &key, &key_len, &value, &value_len);
            if (strncmp (key, "Size", MIN(4, key_len)) == 0) {
                memcpy (buff, value, value_len);
                buff [value_len] = '\0';

                int n = atoi (buff);
                bool found = false;
                int low = 0;
                int up = num_sizes;
                while (low != up) {
                    int mid = (low + up)/2;
                    if (n == sizes[mid]) {
                        found = true;
                        break;
                    } else if (n < sizes[mid]) {
                        up = mid;
                    } else {
                        low = mid + 1;
                    }
                }

                if (!found) {
                    assert (num_sizes < ARRAY_SIZE (sizes) - 1);
                    uint32_t i;
                    for (i=num_sizes; i>low; i--) {
                        sizes[i] = sizes[i-1];
                    }
                    sizes[low] = n;
                    num_sizes++;
                }
            }
        }
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
    gint *sizes = gtk_icon_theme_get_icon_sizes (icon_theme, "utilities-terminal");

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

    print_icon_sizes ("utilities-terminal");

    gtk_widget_set_size_request (window, 700, 700);
    //GtkWidget *image = gtk_image_new_from_file (".svg");
    //gtk_container_add(GTK_CONTAINER(window), image);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}
