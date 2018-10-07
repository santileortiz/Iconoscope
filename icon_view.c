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

GtkWidget *spaced_grid_new ()
{
    GtkWidget *new_grid = gtk_grid_new ();
    gtk_widget_set_margin_start (GTK_WIDGET(new_grid), 12);
    gtk_widget_set_margin_end (GTK_WIDGET(new_grid), 12);
    gtk_widget_set_margin_top (GTK_WIDGET(new_grid), 12);
    gtk_widget_set_margin_bottom (GTK_WIDGET(new_grid), 12);
    gtk_grid_set_row_spacing (GTK_GRID(new_grid), 12);
    gtk_grid_set_column_spacing (GTK_GRID(new_grid), 12);
    return new_grid;
}

struct paned_set_percent_closure_t {
    int percent;
    gulong handler_id;
};

void data_dpy_append (GtkWidget *data, char *title, char *value, int id)
{
    GtkWidget *title_label = gtk_label_new (title);
    gtk_widget_set_halign (title_label, GTK_ALIGN_END);
    add_css_class (title_label, "h4");

    GtkWidget *value_label = gtk_label_new (value);
    gtk_widget_set_halign (value_label, GTK_ALIGN_START);
    add_css_class (value_label, "h5");

    gtk_grid_attach (GTK_GRID(data), title_label, 0, id, 1, 1);
    gtk_grid_attach (GTK_GRID(data), value_label, 1, id, 1, 1);
}

void draw_icon_view (GtkWidget **widget, struct icon_view_t *icon_view)
{
    // Create the icon display
    GtkWidget *icon_dpy = spaced_grid_new ();
    gtk_widget_set_valign (icon_dpy, GTK_ALIGN_CENTER);
    gtk_widget_set_halign (icon_dpy, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (icon_dpy, TRUE);
    gtk_widget_set_vexpand (icon_dpy, TRUE);

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
        gtk_grid_attach (GTK_GRID(icon_dpy), img->image, img_x_idx, img_y_idx, 1, 1);

        if (img->label != NULL) {
            GtkWidget *label = gtk_label_new (img->label);
            gtk_widget_set_valign (label, GTK_ALIGN_START);
            gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_CENTER);
            gtk_grid_attach (GTK_GRID(icon_dpy), label, img_x_idx, img_y_idx + 1, 1, 1);
        }


        i++;
        img = img->next;
    }

    // Create the icon data display
    GtkWidget *data_dpy = spaced_grid_new ();
    GtkWidget *icon_name_label = gtk_label_new (icon_view->icon_name);
    add_css_class (icon_name_label, "h2");
    gtk_label_set_selectable (GTK_LABEL(icon_name_label), TRUE);
    gtk_widget_set_halign (icon_name_label, GTK_ALIGN_START);
    gtk_grid_attach (GTK_GRID(data_dpy), icon_name_label, 0, 0, 1, 1);

    // TODO: Show actual data here
    GtkWidget *data = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID(data), 12);
    data_dpy_append (data, "Path:", "/", 1);
    data_dpy_append (data, "Image Size:", "16x16", 2);
    data_dpy_append (data, "Size:", "16", 3);
    data_dpy_append (data, "Scale:", "1", 4);
    data_dpy_append (data, "Type:", "None", 5);
    gtk_grid_attach (GTK_GRID(data_dpy), data, 0, 1, 1, 1);

    GtkWidget *icon_view_widget = fake_paned (GTK_ORIENTATION_VERTICAL,
                                              icon_dpy, data_dpy);

    // Delete the old widget and attach the new one in its place.
    GtkWidget *parent = gtk_widget_get_parent (*widget);
    gtk_container_remove (GTK_CONTAINER(parent), *widget);
    gtk_paned_pack2 (GTK_PANED(parent), icon_view_widget, TRUE, TRUE);
    *widget = icon_view_widget;

    gtk_widget_show_all(parent);
}
