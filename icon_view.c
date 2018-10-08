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
    int min_size;
    int max_size;
    int scale;
    char* type; // can be NULL
    char* context; // can be NULL
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

static inline
char* str_or_dash (char *str)
{
    if (str == NULL || *str == '\0') {
        static char *str = "-";
        return str;
    } else {
        return str;
    }
}

GtkWidget* image_data_new (struct icon_image_t *img)
{
    GtkWidget *data = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID(data), 12);

    struct icon_image_t l_img;
    if (img == NULL) {
        img = &l_img;
    }

    int i = 0;
    char *str;
    char buff[10];

    data_dpy_append (data, "Theme Path:", str_or_dash(img->theme_dir), i++);
    data_dpy_append (data, "File Path:", str_or_dash(img->path), i++);

    snprintf (buff, ARRAY_SIZE(buff), "%d x %d", img->width, img->height);
    str = img->width < 1 || img->height < 1 ?  "-" : buff;
    data_dpy_append (data, "Image Size:", str, i++);

    snprintf (buff, ARRAY_SIZE(buff), "%d", img->size);
    str = img->size < 1 ?  "-" : buff;
    data_dpy_append (data, "Size:", str, i++);

    snprintf (buff, ARRAY_SIZE(buff), "%d", img->scale);
    str = img->scale < 1 ?  "-" : buff;
    data_dpy_append (data, "Scale:", str, i++);

    data_dpy_append (data, "Context:", str_or_dash(img->context), i++);
    data_dpy_append (data, "Type:", str_or_dash(img->type), i++);

    snprintf (buff, ARRAY_SIZE(buff), "%d - %d", img->min_size, img->max_size);
    str = img->min_size < 1 || img->max_size < 1 ? "-" : buff;
    data_dpy_append (data, "Size Range:", str, i++);
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

    struct icon_image_t *last_img;
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

        if(img->next == NULL) {
            last_img = img;
        }
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
    icon_view->image_data = image_data_new (last_img);
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
