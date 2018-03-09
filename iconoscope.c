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
    while (*c && *c != '[') {
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

struct icon_theme_t {
    char *name;
    uint32_t num_dirs;
    char **dirs;
    char *index_file;
    char *dir_name;
};

void set_theme_name (mem_pool_t *pool, struct icon_theme_t *theme)
{
    char *c = theme->index_file;

    char *section_name;
    uint32_t section_name_len;
    c = seek_next_section (c, &section_name, &section_name_len);

    char *name_str = "Name";
    while ((c = consume_ignored_lines (c)) && *c && !is_end_of_section(c)) {
        char *key, *value;
        uint32_t key_len, value_len;
        c = seek_next_key_value (c, &key, &key_len, &value, &value_len);
        if (strlen (name_str) == key_len && strncmp (name_str, key, key_len) == 0) {
            theme->name = pom_push_size (pool, value_len+1);
            memcpy (theme->name, value, value_len);
            theme->name[value_len] = '\0';
        }

    }
}

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

    // Locate all index.theme files that are in the search paths, and append a
    // new icon_theme_t struct for each one.
    struct it_da_t themes_arr = {0};
    int i;
    for (i=0; i<num_paths; i++) {
        char *curr_search_path = path[i];
        string_t path_str = str_new (curr_search_path);
        if (str_last(&path_str) != '/') {
            str_cat_c (&path_str, "/");
        }
        uint32_t path_len = str_len (&path_str);

        struct stat st;
        if (stat(curr_search_path, &st) == -1 && errno == ENOENT) {
            // NOTE: Current search paths may contain non existent directories.
            //printf ("Search path does not exist.\n");
            continue;
        }

        DIR *d = opendir (curr_search_path);
        struct dirent entry_info, *info_res;
        while (readdir_r (d, &entry_info, &info_res) == 0 && info_res != NULL) {
            if (strcmp ("default", entry_info.d_name) == 0) {
                continue;
            } else if (entry_info.d_name[0] != '.') {
                string_t theme_dir = str_new (entry_info.d_name);
                str_put (&path_str, path_len, &theme_dir);

                if (stat(str_data(&path_str), &st) == 0 && S_ISDIR(st.st_mode) &&
                    file_lookup (str_data(&path_str), "index.theme")) {

                    struct icon_theme_t theme;
                    theme.dir_name = pom_push_size (pool, strlen(entry_info.d_name)+1);
                    memcpy (theme.dir_name, entry_info.d_name, strlen(entry_info.d_name)+1);

                    str_cat_c (&path_str, "/index.theme");
                    theme.index_file = full_file_read (pool, str_data(&path_str));
                    set_theme_name(pool, &theme);
                    it_da_append (&themes_arr, &theme);
                }
                str_free (&theme_dir);
            }
        }

        closedir (d);
        str_free (&path_str);
    }

    // A theme can be spread across multiple search paths. Now that we know the
    // internal name for each theme, we look for subdirectories with this
    // internal name to know which directories a theme is spread across.
    for (i=0; i<themes_arr.len; i++) {
        struct icon_theme_t *curr_theme = &themes_arr.data[i];

        char *found_dirs[num_paths];
        uint32_t num_found = 0;
        int j;
        for (j=0; j<num_paths; j++) {
            char *curr_search_path = path[j];
            string_t path_str = str_new (curr_search_path);
            if (str_last(&path_str) != '/') {
                str_cat_c (&path_str, "/");
            }
            str_cat_c (&path_str, curr_theme->dir_name);

            struct stat st;
            if (stat(str_data(&path_str), &st) == 0 && S_ISDIR(st.st_mode)) {
                found_dirs[num_found] = (char*)pom_push_size (pool, str_len(&path_str)+1);
                memcpy (found_dirs[num_found], str_data(&path_str), str_len(&path_str)+1);
                num_found++;
            }
            str_free (&path_str);
        }

        curr_theme->dirs = (char**)pom_push_size (pool, sizeof(char*)*num_found);
        memcpy (curr_theme->dirs, found_dirs, sizeof(char*)*num_found);
        curr_theme->num_dirs = num_found;
    }

    // Unthemed icons are found inside search path directories but not in a
    // directory. For these icons we add a zero initialized theme, and set as
    // dirs all search paths with icons in them.
    //
    // NOTE: Search paths are not explored recursiveley for icons.
    struct icon_theme_t no_theme = {0};
    no_theme.name = "None";

    char *found_dirs[num_paths];
    uint32_t num_found = 0;
    for (i=0; i<num_paths; i++) {
        string_t path_str = str_new (path[i]);
        if (str_last(&path_str) != '/') {
            str_cat_c (&path_str, "/");
        }
        uint32_t path_len = str_len (&path_str);

        struct stat st;
        if (stat(str_data(&path_str), &st) == -1 && errno == ENOENT) {
            // NOTE: Current search paths may contain non existent directories.
            //printf ("Search path does not exist.\n");
            continue;
        }

        DIR *d = opendir (str_data(&path_str));
        struct dirent entry_info, *info_res;
        while (readdir_r (d, &entry_info, &info_res) == 0 && info_res != NULL) {
            str_put_c (&path_str, path_len, entry_info.d_name);

            if (stat(str_data(&path_str), &st) == 0 && S_ISREG(st.st_mode)) {
                char* ext = get_extension (entry_info.d_name);
                if (strcmp(ext, "svg") || strcmp(ext, "png") || strcmp(ext, "xpm")) {
                    uint32_t res_len = strlen (path[i]) + 1;
                    found_dirs[num_found] = (char*)pom_push_size (pool, res_len);
                    memcpy (found_dirs[num_found], path[i], res_len);
                    num_found++;
                    break;
                }
            }
        }
        str_free (&path_str);
        closedir (d);
    }

    no_theme.dirs = (char**)pom_push_size (pool, sizeof(char*)*num_found);
    memcpy (no_theme.dirs, found_dirs, sizeof(char*)*num_found);
    no_theme.num_dirs = num_found;
    it_da_append (&themes_arr, &no_theme);

    // Finally we copy the dynamic array created into the pool as a fixed size
    // one.
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

void copy_key_into_list (gpointer key, gpointer value, gpointer user_data)
{
  GList **list = user_data;
  *list = g_list_prepend (*list, g_strdup (key));
}

// I have to find this information directly from the icon directories and
// index.theme files. The alternative of using GtkIconTheme with a custom theme
// and then calling gtk_icon_theme_list_icons() on it does not only return icons
// from the chosen theme. Instead it also includes:
//
//      * Unthemed icons
//      * Deprecated stock id's (see GTK/testsuite/gtk/check-icon-names.c)
//      * Internal icons (see GTK/testsuite/gtk/check-icon-names.c)
//      * All icons from Hicolor, GNOME and Adwaita themes
//
// I expected Hicolor icons to be there because it's the fallback theme, but I
// didn't expect any of the rest. All this is probably done for backward
// compatibility reasons but it does not work for what we want.
GList* get_theme_icon_names (struct icon_theme_t *theme)
{
  GHashTable *ht = g_hash_table_new (g_str_hash, g_str_equal);
  mem_pool_t local_pool = {0};

  if (theme->dir_name != NULL) {
      int i;
      for (i=0; i<theme->num_dirs; i++) {
          char *c = theme->index_file;

          // Ignore the first section: [Icon Theme]
          c = seek_next_section (c, NULL, NULL);
          c = consume_section (c);

          string_t theme_dir = str_new (theme->dirs[i]);
          str_cat_c (&theme_dir, "/");
          uint32_t theme_dir_len = str_len (&theme_dir);
          while ((c = consume_section (c)) && *c) {
              char *section_name;
              uint32_t section_name_len;
              c = seek_next_section (c, &section_name, &section_name_len);
              string_t curr_dir = strn_new (section_name, section_name_len);
              str_put (&theme_dir, theme_dir_len, &curr_dir);
              if (section_name[section_name_len-1] != '/') {
                  str_cat_c (&theme_dir, "/");
              }
              uint32_t curr_dir_len = str_len (&theme_dir);

              struct stat st;
              if (stat(str_data(&theme_dir), &st) == -1 && errno == ENOENT) {
                  continue;
              }

              DIR *d = opendir (str_data(&theme_dir));
              struct dirent entry_info, *info_res;
              while (readdir_r (d, &entry_info, &info_res) == 0 && info_res != NULL) {
                  if (entry_info.d_name[0] != '.') {
                      str_put_c (&theme_dir, curr_dir_len, entry_info.d_name);
                      if (stat(str_data(&theme_dir), &st) == 0 && S_ISREG(st.st_mode)) {
                          char *icon_name = remove_extension (&local_pool, entry_info.d_name);
                          if (icon_name != NULL) {
                              g_hash_table_insert (ht, icon_name, NULL);
                          }
                      }
                  }
              }
              closedir (d);
          }
      }

  } else {
      // This is the case for non themed icons.
      int i;
      for (i=0; i<theme->num_dirs; i++) {
        string_t path_str = str_new (theme->dirs[i]);
        if (str_last(&path_str) != '/') {
            str_cat_c (&path_str, "/");
        }
        uint32_t path_len = str_len (&path_str);

        DIR *d = opendir (str_data(&path_str));
        struct dirent entry_info, *info_res;
        while (readdir_r (d, &entry_info, &info_res) == 0 && info_res != NULL) {
            struct stat st;
            str_put_c (&path_str, path_len, entry_info.d_name);

            if (stat(str_data(&path_str), &st) == 0 && S_ISREG(st.st_mode)) {
                char* ext = get_extension (entry_info.d_name);
                char *icon_name = remove_extension (&local_pool, entry_info.d_name);
                if (strcmp(ext, "svg") || strcmp(ext, "png") || strcmp(ext, "xpm")) {
                    g_hash_table_insert (ht, icon_name, NULL);
                }
            }
        }
        str_free (&path_str);
        closedir (d);
      }
  }

  GList *res;
  g_hash_table_foreach (ht, copy_key_into_list, &res);
  mem_pool_destroy (&local_pool);

  return res;
}

GtkWidget *scrolled_icon_list  = NULL, *icon_list = NULL;
struct icon_theme_t *themes;
uint32_t num_themes;
void on_theme_changed (GtkComboBox *themes_combobox, gpointer user_data)
{
    // NOTE: We can't use:
    //    gtk_container_remove(GTK_CONTAINER(scrolled_icon_list), icon_list)
    // because gtk_container_add() wraps icon_list in a GtkViewport so
    // scrolled_icon_list isn't actually the parent of icon_list.
    gtk_container_remove (GTK_CONTAINER(scrolled_icon_list),
                          gtk_bin_get_child (GTK_BIN (scrolled_icon_list)));

    icon_list = gtk_list_box_new ();
    gtk_widget_set_vexpand (icon_list, TRUE);
    gtk_widget_set_hexpand (icon_list, TRUE);

#if 1
    struct icon_theme_t *icon_theme =
        themes + gtk_combo_box_get_active (themes_combobox);
    GList *icon_names = get_theme_icon_names (icon_theme);

#else
    GtkIconTheme *icon_theme = gtk_icon_theme_new ();
    char *theme_name = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (themes_combobox));
    gtk_icon_theme_set_custom_theme (icon_theme, theme_name);
    GList *icon_names = gtk_icon_theme_list_icons (icon_theme, NULL);
#endif

    icon_names = g_list_sort (icon_names, str_cmp_callback);

    uint32_t i = 0;
    GList *l;
    for (l = icon_names; l != NULL; l = l->next)
    {
        // NOTE: Ignore symbolic icons for now
        if (g_str_has_suffix (l->data, "symbolic")) {
            continue;
        }
        //printf ("%s\n", (char*)l->data);

        GtkWidget *row = gtk_label_new (l->data);
        gtk_list_box_prepend (GTK_LIST_BOX(icon_list), row);
        gtk_widget_set_halign (row, GTK_ALIGN_START);

        gtk_widget_set_margin_start (row, 6);
        i++;
    }
    //printf ("%u\n", i);

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

    //GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
    //gint *sizes = gtk_icon_theme_get_icon_sizes (icon_theme, "io.elementary.code");
    //while (*sizes) {
    //    printf ("%d ", *sizes++);
    //}
    //printf ("\n");
    //print_icon_sizes ("io.elementary.code");

    mem_pool_t pool = {0};
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
        //printf ("%s: %s\n", themes[i].name, themes[i].dir_name);
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
