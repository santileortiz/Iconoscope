#include <cairo.h>
#include <gtk/gtk.h>

#include "common.h"

void add_custom_css (GtkWidget *widget, gchar *css_data)
{
    GtkStyleContext *style_context = gtk_widget_get_style_context (widget);
    GtkCssProvider *css_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (css_provider, css_data, -1, NULL);
    gtk_style_context_add_provider (style_context,
                                    GTK_STYLE_PROVIDER(css_provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
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

bool read_dir (DIR *dirp, struct dirent **res)
{
    errno = 0;
    *res = readdir (dirp);
    if (*res == NULL) {
        if (errno != 0) {
            printf ("Error while reading directory: %s", strerror (errno));
        }
        return false;
    }
    return true;
}

bool icon_lookup (mem_pool_t *pool, char *dir, const char *icon_name, char **found_file)
{
    struct stat st;
    if (stat(dir, &st) == -1 && errno == ENOENT) {
        // NOTE: There are index.theme files that have entries for @2
        // directories, even though such directories do not exist in the system.
        //printf ("No directory named: %s\n", dir);
        return 0;
    }


    char *ext_priority[] = {"svg", "png", "xpm"};
    uint32_t ext_id = ARRAY_SIZE(ext_priority);
    DIR *d = opendir (dir);
    struct dirent *entry_info;
    while (read_dir (d, &entry_info)) {
        uint32_t name_len = strlen(icon_name);
        uint32_t entry_len = strlen(entry_info->d_name);
        int cmp = strncmp (icon_name, entry_info->d_name, MIN (name_len, entry_len));
        if (cmp == 0) {
            if (name_len == entry_len ||
                (name_len < entry_len && entry_info->d_name[name_len] == '.')) {

                char *entry_ext = get_extension (entry_info->d_name);
                int i;
                for (i=0; i<ARRAY_SIZE (ext_priority); i++) {
                    if (strcmp (ext_priority[i], entry_ext) == 0 &&
                        i < ext_id) {
                        ext_id = i;
                    }
                }

                if (ext_id == 0) {
                    break;
                }
            }
        }
    }

    closedir (d);

    if (ext_id == ARRAY_SIZE(ext_priority)) {
        // No icon found with that name
        return false;
    } else {
        string_t found_path = str_new (dir);
        if (str_last (&found_path) != '/') {
            str_cat_c (&found_path, "/");
        }
        str_cat_c (&found_path, icon_name);
        str_cat_c (&found_path, ".");
        str_cat_c (&found_path, ext_priority[ext_id]);

        *found_file = mem_pool_push_size (pool, str_len(&found_path) + 1);
        memcpy (*found_file, str_data(&found_path), str_len(&found_path) + 1);
        str_free (&found_path);
        return true;
    }
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
    struct dirent *entry_info;
    while (read_dir (d, &entry_info)) {
        uint32_t file_len = strlen(file);
        uint32_t entry_len = strlen(entry_info->d_name);
        int cmp = strncmp (file, entry_info->d_name, MIN (file_len, entry_len));
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

gboolean delete_callback (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    gtk_main_quit ();
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
        struct dirent *entry_info;
        while (read_dir (d, &entry_info)) {
            if (strcmp ("default", entry_info->d_name) == 0) {
                continue;
            } else if (entry_info->d_name[0] != '.') {
                string_t theme_dir = str_new (entry_info->d_name);
                str_put (&path_str, path_len, &theme_dir);

                if (stat(str_data(&path_str), &st) == 0 && S_ISDIR(st.st_mode) &&
                    file_lookup (str_data(&path_str), "index.theme")) {

                    struct icon_theme_t theme;
                    theme.dir_name = pom_push_size (pool, strlen(entry_info->d_name)+1);
                    memcpy (theme.dir_name, entry_info->d_name, strlen(entry_info->d_name)+1);

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
        struct dirent *entry_info;
        while (read_dir (d, &entry_info)) {
            str_put_c (&path_str, path_len, entry_info->d_name);

            if (stat(str_data(&path_str), &st) == 0 && S_ISREG(st.st_mode)) {
                char* ext = get_extension (entry_info->d_name);
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
    return g_ascii_strcasecmp ((const char*)a, (const char*)b);
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
              struct dirent *entry_info;
              while (read_dir (d, &entry_info)) {
                  if (entry_info->d_name[0] != '.') {
                      str_put_c (&theme_dir, curr_dir_len, entry_info->d_name);
                      if (stat(str_data(&theme_dir), &st) == 0 && S_ISREG(st.st_mode)) {
                          char *icon_name = remove_extension (&local_pool, entry_info->d_name);
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
        struct dirent *entry_info;
        while (read_dir (d, &entry_info)) {
            struct stat st;
            str_put_c (&path_str, path_len, entry_info->d_name);

            if (stat(str_data(&path_str), &st) == 0 && S_ISREG(st.st_mode)) {
                char* ext = get_extension (entry_info->d_name);
                char *icon_name = remove_extension (&local_pool, entry_info->d_name);
                if (strcmp(ext, "svg") || strcmp(ext, "png") || strcmp(ext, "xpm")) {
                    g_hash_table_insert (ht, icon_name, NULL);
                }
            }
        }
        str_free (&path_str);
        closedir (d);
      }
  }

  GList *res = NULL;
  g_hash_table_foreach (ht, copy_key_into_list, &res);
  mem_pool_destroy (&local_pool);

  return res;
}

struct icon_file_t {
    char *fname;
    bool scalable;
    uint32_t size;
    uint32_t scale;
};

struct icon_info_t {
    mem_pool_t pool;
    char *icon_name;
    uint32_t num_files;
    struct icon_file_t *files;
};

struct icon_theme_t *selected_theme = NULL;
struct icon_info_t get_icon_info (struct icon_theme_t *theme, const char *icon_name)
{
    struct icon_info_t res = {0};
    GArray *files = g_array_new (FALSE, FALSE, sizeof(struct icon_file_t));

    if (theme->index_file != NULL) {
        int i;
        for (i = 0; i < theme->num_dirs; i++) {
            string_t path = str_new (theme->dirs[i]);
            if (str_last (&path) != '/') {
                str_cat_c (&path, "/");
            }
            uint32_t path_len = str_len (&path);
            char *c = theme->index_file;

            // Ignore the first section: [Icon Theme]
            c = seek_next_section (c, NULL, NULL);
            c = consume_section (c);

            while (*c) {
                char *section_name;
                uint32_t section_name_len;
                c = seek_next_section (c, &section_name, &section_name_len);
                string_t dir = strn_new (section_name, section_name_len);
                str_put (&path, path_len, &dir);

                char *icon_path;
                if (icon_lookup (&res.pool, str_data (&path), icon_name, &icon_path)) {
                    // TODO: Maybe get this information before looking up the directory
                    // and conditionally call icon_lookup() depending on the information
                    // we get.
                    struct icon_file_t f;
                    f.scale = 1;
                    while ((c = consume_ignored_lines (c)) && !is_end_of_section(c)) {
                        char *key, *value;
                        uint32_t key_len, value_len;
                        c = seek_next_key_value (c, &key, &key_len, &value, &value_len);
                        if (strncmp (key, "Size", MIN(4, key_len)) == 0) {
                            sscanf (value, "%"SCNu32, &f.size);

                        } else if (strncmp (key, "Scale", MIN(5, key_len)) == 0) {
                            sscanf (value, "%"SCNu32, &f.scale);

                        }

                        // NOTE: We now ignore the Scalable type information
                        // from the directory because it seems meaningless with
                        // respect to what users do. Instead we should display
                        // this only as information of the directory.
                        //
                        //else if (strncmp (key, "Type", MIN(4, key_len)) == 0 &&
                        //           strncmp (value, "Scalable", MIN(8, value_len)) == 0) {
                        //    f.scalable = true;
                        //    scalable_dir = true;
                        //}
                    }

                    f.scalable = false;
                    if (strstr (str_data(&dir), "scalable") != NULL) {
                        f.scalable = true;
                    }

                    if (f.scale == 1) {
                        f.fname = icon_path;
                        g_array_append_val (files, f);
                    }

                } else {
                    c = consume_section (c);
                }

                str_free (&dir);
            }

            if (files->len > 0) {
                // If we found something in a search path then stop looking in the
                // other ones.
                break;
            }

            str_free (&path);
        }

    } else {
        int i;
        for (i = 0; i < theme->num_dirs; i++) {
            string_t path = str_new (theme->dirs[i]);
            if (str_last (&path) != '/') {
                str_cat_c (&path, "/");
            }

            char *icon_path;
            if (icon_lookup (&res.pool, str_data (&path), icon_name, &icon_path)) {
                struct icon_file_t f;
                f.scale = 1;
                f.scalable = false;
                f.fname = icon_path;
                g_array_append_val (files, f);
            }

            str_free (&path);
        }
    }

    res.num_files = files->len;
    res.files = (struct icon_file_t*)mem_pool_push_size (&res.pool, sizeof(struct icon_file_t)*files->len);
    memcpy (res.files, files->data, sizeof(struct icon_file_t)*files->len);

    g_array_free (files, TRUE);

    return res;
}

GtkWidget *icon_view = NULL;
void on_icon_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
    if (row == NULL) {
        return;
    }

    GtkWidget *parent = gtk_widget_get_parent (icon_view);
    gtk_container_remove (GTK_CONTAINER(parent), icon_view);

    icon_view = gtk_grid_new ();
    gtk_widget_set_valign (icon_view, GTK_ALIGN_CENTER);
    gtk_widget_set_halign (icon_view, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (icon_view, TRUE);
    gtk_widget_set_vexpand (icon_view, TRUE);
    gtk_grid_set_row_spacing (GTK_GRID(icon_view), 6);
    gtk_grid_set_column_spacing (GTK_GRID(icon_view), 12);

    GtkWidget *icon_label = gtk_bin_get_child (GTK_BIN(row));
    const char * icon_name = gtk_label_get_text (GTK_LABEL(icon_label));
    struct icon_info_t icon_info = get_icon_info (selected_theme, icon_name);

    int i;
    for (i=0; i < icon_info.num_files; i++) {
        struct icon_file_t *f = &icon_info.files[i];


        GtkWidget *image = gtk_image_new_from_file (f->fname);
        gtk_widget_set_valign (image, GTK_ALIGN_END);

        GdkPixbuf *pixbuf = gtk_image_get_pixbuf (GTK_IMAGE(image));
        int image_width = gdk_pixbuf_get_width(pixbuf);
        int image_height = gdk_pixbuf_get_height(pixbuf);
        char buff[16];
        if (selected_theme->dir_name != NULL) {
            if (f->scalable) {
                sprintf (buff, "Scalable");
                gtk_widget_set_size_request (image, image_width, image_height);
            } else {
                gtk_widget_set_size_request (image, f->size, f->size);
                sprintf (buff, "%d", f->size);
            }
        } else {
            // The theme is the one that contains unthemed icons. Set the label
            // to the empty string.
            buff[0] = '\0';
        }

        GtkWidget *label = gtk_label_new (buff);
        gtk_widget_set_valign (label, GTK_ALIGN_START);
        gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_CENTER);

        if (image_width/image_height > 1) {
            // NOTE: At least one package (aptdaemon-data) provides animated
            // icons in a single file by appending the frames side by side.
            // Here we detect that case and instead display these icons
            // vertically.
            gtk_grid_attach (GTK_GRID(icon_view), image, 0, 2*i, 1, 1);
            gtk_grid_attach (GTK_GRID(icon_view), label, 0, 2*i+1, 1, 1);
        } else {
            gtk_grid_attach (GTK_GRID(icon_view), image, i, 0, 1, 1);
            gtk_grid_attach (GTK_GRID(icon_view), label, i, 1, 1, 1);
        }
        //printf ("%s (%d)\n", f->fname, f->size);
    }
    //printf ("\n");

    gtk_widget_show_all(icon_view);
    gtk_paned_pack2 (GTK_PANED(parent), icon_view, TRUE, FALSE);
}

GtkWidget *icon_list = NULL, *search_entry = NULL;
struct icon_theme_t *themes;
uint32_t num_themes;

gboolean on_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (event->keyval == GDK_KEY_Escape){
        gtk_entry_set_text (GTK_ENTRY(search_entry), "");
        return TRUE;
    }
    return FALSE;
}

gboolean search_filter (GtkListBoxRow *row, gpointer user_data)
{
    const gchar *search_str = gtk_entry_get_text (GTK_ENTRY(search_entry));
    GtkWidget *row_label = gtk_bin_get_child (GTK_BIN(row));
    const char * icon_name = gtk_label_get_text (GTK_LABEL(row_label));

    if (strstr (icon_name, search_str) != NULL) {
        return TRUE;
    } else {
        return FALSE;
    }
}

void on_theme_changed (GtkComboBox *themes_combobox, gpointer user_data)
{
    // NOTE: It's better to query who the prent widget is (rather than keeping a
    // reference to it) because it can change in unexpected ways. For instance,
    // in this case GtkScrolledWindow wraps icon_list in a GtkViewPort.
    GtkWidget *parent = gtk_widget_get_parent (icon_list);
    gtk_container_remove (GTK_CONTAINER(parent), icon_list);

    icon_list = gtk_list_box_new ();
    add_custom_css (icon_list, ".list { background-color: transparent; }");
    gtk_widget_set_vexpand (icon_list, TRUE);
    gtk_widget_set_hexpand (icon_list, TRUE);
    g_signal_connect (G_OBJECT(icon_list), "row-selected", G_CALLBACK (on_icon_selected), NULL);
    gtk_list_box_set_filter_func (GTK_LIST_BOX(icon_list), search_filter, NULL, NULL);

    selected_theme =
        themes + gtk_combo_box_get_active (themes_combobox);
    GList *icon_names = get_theme_icon_names (selected_theme);
    icon_names = g_list_sort (icon_names, str_cmp_callback);

    bool first = true;
    uint32_t i = 0;
    GList *l = NULL;
    for (l = icon_names; l != NULL; l = l->next)
    {
        // NOTE: Ignore symbolic icons for now
        if (g_str_has_suffix (l->data, "symbolic") ||
            g_str_has_suffix (l->data, "symbolic-rtl")) {
            continue;
        }
        //printf ("%s\n", (char*)l->data);

        GtkWidget *row = gtk_label_new (l->data);
        gtk_container_add (GTK_CONTAINER(icon_list), row);
        gtk_widget_set_halign (row, GTK_ALIGN_START);

        if (first) {
            first = false;
            GtkWidget *r = gtk_widget_get_parent (row);
            gtk_list_box_select_row (GTK_LIST_BOX(icon_list), GTK_LIST_BOX_ROW(r));
        }

        gtk_widget_set_margin_start (row, 6);
        gtk_widget_set_margin_end (row, 6);
        gtk_widget_set_margin_top (row, 3);
        gtk_widget_set_margin_bottom (row, 3);
        i++;
    }
    //printf ("%u\n", i);

    gtk_container_add (GTK_CONTAINER(parent), icon_list);
    gtk_widget_show_all(icon_list);
}

void on_search_changed (GtkEditable *search_entry, gpointer user_data)
{
    gtk_list_box_invalidate_filter (GTK_LIST_BOX(icon_list));
}

int main(int argc, char *argv[])
{
    GtkWidget *window;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_resize (GTK_WINDOW(window), 970, 650);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    GtkWidget *header_bar = gtk_header_bar_new ();
    gtk_header_bar_set_title (GTK_HEADER_BAR(header_bar), "Iconoscope");
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR(header_bar), TRUE);
    gtk_window_set_titlebar (GTK_WINDOW(window), header_bar);

    g_signal_connect (G_OBJECT(window), "delete-event", G_CALLBACK (delete_callback), NULL);
    g_signal_connect (G_OBJECT(window), "key_press_event", G_CALLBACK (on_key_press), NULL);

    mem_pool_t pool = {0};
    get_all_icon_themes (&pool, &themes, &num_themes);

    icon_list = gtk_list_box_new ();
    gtk_widget_set_vexpand (icon_list, TRUE);
    gtk_widget_set_hexpand (icon_list, TRUE);
    g_signal_connect (G_OBJECT(icon_list), "row-selected", G_CALLBACK (on_icon_selected), NULL);

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

    GtkWidget *scrolled_icon_list = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scrolled_icon_list), icon_list);

    search_entry = gtk_search_entry_new ();
    add_custom_css (search_entry, ".entry { border-radius: 13px; }");
    g_signal_connect (G_OBJECT(search_entry), "changed", G_CALLBACK (on_search_changed), NULL);

    GtkWidget *sidebar = gtk_grid_new ();
    gtk_grid_attach (GTK_GRID(sidebar), search_entry, 0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID(sidebar), scrolled_icon_list, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID(sidebar), theme_selector, 0, 2, 1, 1);

    icon_view = gtk_grid_new ();
    GtkWidget *paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1 (GTK_PANED(paned), sidebar, FALSE, FALSE);
    gtk_paned_pack2 (GTK_PANED(paned), icon_view, TRUE, TRUE);

    gtk_combo_box_set_active (GTK_COMBO_BOX(themes_combobox), 0);

    gtk_container_add(GTK_CONTAINER(window), paned);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}
