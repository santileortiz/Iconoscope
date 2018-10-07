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
    int size;
    int scale;
    char* type; // can be NULL
    bool is_scalable; // True if directory in index file contains "scalable"

    struct icon_image_t *next;
    struct icon_view_t *view; // Pointer to the icon_view_t this icon_image_t is member of.
};

struct icon_view_t {
    char *icon_name;

    struct icon_image_t *images;

    //struct icon_image_t *symbolic_icons;

    //int num_other_themes;
    //char **other_themes;

    // UI Widgets
    GtkWidget *image_data;
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

void data_dpy_append (GtkWidget *data, char *title, char *value, int id)
{
    GtkWidget *title_label = gtk_label_new (title);
    gtk_widget_set_halign (title_label, GTK_ALIGN_END);
    add_css_class (title_label, "h4");

    GtkWidget *value_label = gtk_label_new (value);
    gtk_widget_set_halign (value_label, GTK_ALIGN_START);
    gtk_label_set_selectable (GTK_LABEL(value_label), TRUE);
    add_css_class (value_label, "h5");

    gtk_grid_attach (GTK_GRID(data), title_label, 0, id, 1, 1);
    gtk_grid_attach (GTK_GRID(data), value_label, 1, id, 1, 1);
}

GtkWidget* image_data_new (struct icon_image_t *img)
{
    GtkWidget *data = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID(data), 12);

    if (img == NULL) {
        data_dpy_append (data, "Theme path:", "-", 0);
        data_dpy_append (data, "File path:", "-", 1);
        data_dpy_append (data, "Image Size:", "-", 2);
        data_dpy_append (data, "Size:", "-", 3);
        data_dpy_append (data, "Scale:", "-", 4);
        data_dpy_append (data, "Type:", "-", 5);

    } else {
        char buff[10];
        data_dpy_append (data, "Theme path:", img->theme_dir, 0);
        data_dpy_append (data, "File path:", img->path, 1);

        snprintf (buff, ARRAY_SIZE(buff), "%d x %d", img->width, img->height);
        data_dpy_append (data, "Image Size:", buff, 2);

        snprintf (buff, ARRAY_SIZE(buff), "%d", img->size);
        data_dpy_append (data, "Size:", buff, 3);

        snprintf (buff, ARRAY_SIZE(buff), "%d", img->scale);
        data_dpy_append (data, "Scale:", buff, 4);

        data_dpy_append (data, "Type:", img->type, 5);
    }
    return data;
}

void on_image_clicked (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    struct icon_image_t *img = (struct icon_image_t *)user_data;
    GtkWidget *image_data = image_data_new (img);
    g_assert (img->view->image_data != NULL);

    // Replace image_data widget
    GtkWidget *parent = gtk_widget_get_parent (img->view->image_data);
    gtk_container_remove (GTK_CONTAINER(parent), img->view->image_data);
    gtk_container_add (GTK_CONTAINER(parent), image_data);
    img->view->image_data = image_data;
    gtk_widget_show_all (image_data);
}

void draw_icon_view (GtkWidget **widget, struct icon_view_t *icon_view)
{
    // Create the icon display
    GtkWidget *icon_dpy = spaced_grid_new ();
    gtk_widget_set_valign (icon_dpy, GTK_ALIGN_CENTER);
    gtk_widget_set_halign (icon_dpy, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (icon_dpy, TRUE);
    gtk_widget_set_vexpand (icon_dpy, TRUE);

    // NOTE: At least one package (aptdaemon-data) provides animated icons in a
    // single file by appending the frames side by side.  Here we detect that
    // case and instead display these icons vertically.
    GtkOrientation all_icons_or = icon_view->images->width/icon_view->images->height > 1 ?
        GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
    GtkWidget *all_icons = gtk_box_new (all_icons_or, 24);

    struct icon_image_t *img = icon_view->images;
    while (img != NULL) {
        GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
        gtk_widget_set_valign (box,GTK_ALIGN_END);
        gtk_widget_set_vexpand (box, FALSE);

        gtk_container_add (GTK_CONTAINER(box), img->image);

        if (img->label != NULL) {
            GtkWidget *label = gtk_label_new (img->label);
            gtk_widget_set_valign (label, GTK_ALIGN_START);
            gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_CENTER);
            gtk_container_add (GTK_CONTAINER(box), label);
        }

        GtkWidget *hitbox = gtk_event_box_new ();
        gtk_container_add (GTK_CONTAINER(hitbox), box);
        g_signal_connect (G_OBJECT(hitbox), "button-press-event", G_CALLBACK(on_image_clicked), img);

        gtk_container_add (GTK_CONTAINER(all_icons), hitbox);

        img = img->next;
    }
    gtk_grid_attach (GTK_GRID(icon_dpy), all_icons, 0, 0, 1, 1);

    // Create the icon data display
    GtkWidget *data_dpy = spaced_grid_new ();
    GtkWidget *icon_name_label = gtk_label_new (icon_view->icon_name);
    add_css_class (icon_name_label, "h2");
    gtk_label_set_selectable (GTK_LABEL(icon_name_label), TRUE);
    gtk_widget_set_halign (icon_name_label, GTK_ALIGN_START);
    gtk_grid_attach (GTK_GRID(data_dpy), icon_name_label, 0, 0, 1, 1);

    GtkWidget *wrapper = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    icon_view->image_data = image_data_new (img);
    gtk_container_add (GTK_CONTAINER(wrapper), icon_view->image_data);
    gtk_grid_attach (GTK_GRID(data_dpy), wrapper, 0, 1, 1, 1);

    GtkWidget *icon_view_widget = fake_paned (GTK_ORIENTATION_VERTICAL,
                                              icon_dpy, data_dpy);

    // Delete the old widget and attach the new one in its place.
    GtkWidget *parent = gtk_widget_get_parent (*widget);
    gtk_container_remove (GTK_CONTAINER(parent), *widget);
    gtk_paned_pack2 (GTK_PANED(parent), icon_view_widget, TRUE, TRUE);
    *widget = icon_view_widget;

    gtk_widget_show_all(parent);
}
