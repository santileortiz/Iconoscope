/*
 * Copiright (C) 2018 Santiago León O.
 */

struct icon_view_t {
    char *name;
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

    struct icon_info_t icon_info = get_icon_info (selected_theme, icon_view->name);

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
            gtk_grid_attach (GTK_GRID(wdgt), image, 0, 2*i, 1, 1);
            gtk_grid_attach (GTK_GRID(wdgt), label, 0, 2*i+1, 1, 1);
        } else {
            gtk_grid_attach (GTK_GRID(wdgt), image, i, 0, 1, 1);
            gtk_grid_attach (GTK_GRID(wdgt), label, i, 1, 1, 1);
        }
        //printf ("%s (%d)\n", f->fname, f->size);
    }
    //printf ("\n");

    gtk_widget_show_all(wdgt);

    GtkWidget *parent = gtk_widget_get_parent (*widget);
    gtk_container_remove (GTK_CONTAINER(parent), *widget);
    gtk_paned_pack2 (GTK_PANED(parent), wdgt, TRUE, FALSE);
    *widget = wdgt;
}
