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

    int scale;
    struct icon_image_t *images[3];

    //int num_other_themes;
    //char **other_themes;

    // UI Widgets
    GtkWidget *icon_dpy;
    GtkWidget *image_data_dpy;
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
    gtk_label_set_ellipsize (GTK_LABEL(value_label), PANGO_ELLIPSIZE_END);
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

GtkWidget* image_data_dpy_new (struct icon_image_t *img)
{
    GtkWidget *data = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID(data), 12);

    struct icon_image_t l_img = ZERO_INIT(struct icon_image_t);
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
    GtkWidget *image_data_dpy = image_data_dpy_new (img);
    g_assert (img->view->image_data_dpy != NULL);

    // Replace image_data_dpy widget
    GtkWidget *parent = gtk_widget_get_parent (img->view->image_data_dpy);
    gtk_container_remove (GTK_CONTAINER(parent), img->view->image_data_dpy);
    gtk_container_add (GTK_CONTAINER(parent), image_data_dpy);
    img->view->image_data_dpy = image_data_dpy;
    gtk_widget_show_all (image_data_dpy);
}

GtkWidget* icon_view_create_icon_dpy (struct icon_view_t *icon_view, int scale)
{
    GtkWidget *icon_dpy = spaced_grid_new ();
    gtk_widget_set_valign (icon_dpy, GTK_ALIGN_CENTER);
    gtk_widget_set_halign (icon_dpy, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (icon_dpy, TRUE);
    gtk_widget_set_vexpand (icon_dpy, TRUE);

    // NOTE: At least one package (aptdaemon-data) provides animated icons in a
    // single file by appending the frames side by side.  Here we detect that
    // case and instead display these icons vertically.
    GtkOrientation all_icons_or = icon_view->images[scale-1]->width/icon_view->images[scale-1]->height > 1 ?
        GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
    GtkWidget *all_icons = gtk_box_new (all_icons_or, 24);

    struct icon_image_t *last_img = NULL;
    struct icon_image_t *img = icon_view->images[scale-1];

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

    if (icon_view->image_data_dpy == NULL) {
        icon_view->image_data_dpy = image_data_dpy_new (last_img);
    } else {
        replace_wrapped_widget (&icon_view->image_data_dpy, image_data_dpy_new (last_img));
    }

    return icon_dpy;
}

void on_scale_toggled (GtkToggleButton *button, gpointer user_data)
{
    struct icon_view_t *icon_view = (struct icon_view_t *) user_data;

    if (gtk_toggle_button_get_active(button)) {
        int scale = gtk_radio_button_get_idx (GTK_RADIO_BUTTON(button));
        GtkWidget *icon_dpy = icon_view_create_icon_dpy (icon_view, scale);
        replace_wrapped_widget (&icon_view->icon_dpy, icon_dpy);
    }
}

GtkWidget* scale_selector_new (struct icon_view_t *icon_view)
{
    GtkWidget *selector = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_halign (selector, GTK_ALIGN_END);
    gtk_widget_set_hexpand (selector, TRUE);
    gtk_button_box_set_layout (GTK_BUTTON_BOX(selector), GTK_BUTTONBOX_EXPAND);

    GSList *group = NULL;
    for (int i=0; i<ARRAY_SIZE(icon_view->images); i++) {
        char buff[3];
        snprintf (buff, ARRAY_SIZE(buff), "%iX", i+1);
        GtkWidget *scale_button = gtk_radio_button_new_with_label (group, buff);
        gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON(scale_button), FALSE);
        gtk_container_add (GTK_CONTAINER(selector), scale_button);
        if (icon_view->images[i] != NULL) {
            g_signal_connect (G_OBJECT(scale_button), "toggled", G_CALLBACK(on_scale_toggled), icon_view);
        } else {
            gtk_widget_set_sensitive (scale_button, FALSE);
        }
        group = gtk_radio_button_get_group (GTK_RADIO_BUTTON(scale_button));
    }

    return selector;
}

GtkWidget* draw_icon_view (struct icon_view_t *icon_view)
{
    icon_view->icon_dpy = icon_view_create_icon_dpy (icon_view, 1);

    // Create the icon data pane
    GtkWidget *data_pane = spaced_grid_new ();
    GtkWidget *icon_name_label = gtk_label_new (icon_view->icon_name);
    add_css_class (icon_name_label, "h2");
    gtk_label_set_selectable (GTK_LABEL(icon_name_label), TRUE);
    gtk_label_set_ellipsize (GTK_LABEL(icon_name_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign (icon_name_label, GTK_ALIGN_START);
    gtk_grid_attach (GTK_GRID(data_pane), icon_name_label, 0, 0, 1, 1);

    GtkWidget *scale_selector = scale_selector_new (icon_view);
    gtk_grid_attach (GTK_GRID(data_pane), scale_selector, 1, 0, 1, 1);

    gtk_grid_attach (GTK_GRID(data_pane),
                     wrap_gtk_widget(icon_view->image_data_dpy),
                     0, 1, 1, 1);

    return fake_paned (GTK_ORIENTATION_VERTICAL,
                       wrap_gtk_widget(icon_view->icon_dpy),
                       data_pane);
}
