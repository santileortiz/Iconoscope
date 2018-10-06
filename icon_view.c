/*
 * Copiright (C) 2018 Santiago León O.
 */

struct icon_image_t {
    GtkWidget *image;
    int width, height;
    char *label; // can be NULL

    // Information found in the index file
    char *path;
    int size;
    int scale;
    char* type; // can be NULL
    bool is_scalable; // True if directory in index file contains "scalable"

    struct icon_image_t *next;
};

struct icon_view_t {
    char *icon_name;

    struct icon_image_t *images;

    //struct icon_image_t *symbolic_icons;

    //int num_other_themes;
    //char **other_themes;
};

void draw_icon_view (GtkWidget **widget, struct icon_view_t *icon_view)
{
    GtkWidget *wdgt = gtk_grid_new ();
    gtk_widget_set_valign (wdgt, GTK_ALIGN_CENTER);
    gtk_widget_set_halign (wdgt, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (wdgt, TRUE);
    gtk_widget_set_vexpand (wdgt, TRUE);
    gtk_grid_set_row_spacing (GTK_GRID(wdgt), 6);
    gtk_grid_set_column_spacing (GTK_GRID(wdgt), 12);

    int i = 0;
    struct icon_image_t *img = icon_view->images;
    while (img != NULL) {
        int img_x_idx, img_y_idx;
        if (img->width/img->height > 1) {
            // NOTE: At least one package (aptdaemon-data) provides animated
            // icons in a single file by appending the frames side by side.
            // Here we detect that case and instead display these icons
            // vertically.
            img_x_idx = 0;
            img_y_idx = 2*i;
        } else {
            img_x_idx = i;
            img_y_idx = 0;
        }
        gtk_grid_attach (GTK_GRID(wdgt), img->image, img_x_idx, img_y_idx, 1, 1);

        if (img->label != NULL) {
            GtkWidget *label = gtk_label_new (img->label);
            gtk_widget_set_valign (label, GTK_ALIGN_START);
            gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_CENTER);
            gtk_grid_attach (GTK_GRID(wdgt), label, img_x_idx, img_y_idx + 1, 1, 1);
        }

        i++;
        img = img->next;
    }

    gtk_widget_show_all(wdgt);

    // Delete the old widget and attach the new one in its place.
    GtkWidget *parent = gtk_widget_get_parent (*widget);
    gtk_container_remove (GTK_CONTAINER(parent), *widget);
    gtk_paned_pack2 (GTK_PANED(parent), wdgt, TRUE, FALSE);
    *widget = wdgt;
}
