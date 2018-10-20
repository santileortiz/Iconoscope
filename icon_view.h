/*
 * Copiright (C) 2018 Santiago León O.
 */

struct icon_image_t {
    GtkWidget *image;
    int width, height;
    char *label; // can be NULL

    // Information found in the index file
    char *theme_dir;
    char *path;
    char *full_path;
    off_t file_size;
    int size;
    int min_size;
    int max_size;
    int scale;
    char* type; // can be NULL
    char* context; // can be NULL
    bool is_scalable; // True if directory in index file contains "scalable"

    struct icon_image_t *next;
    struct icon_view_t *view; // Pointer to the icon_view_t this icon_image_t is member of.

    // UI Widgets
    GtkWidget *box;
    GtkCssProvider *custom_css;
};

struct icon_view_t {
    char *icon_name;

    int scale;
    struct icon_image_t *images[3];

    //int num_other_themes;
    //char **other_themes;

    // UI Widgets
    GtkWidget *icon_dpy;
    GtkWidget *image_data_dpy;
    struct icon_image_t *selected_img;

    GtkWidget *scrolled_window;
    GtkCssProvider *scrolled_window_custom_css;
};

