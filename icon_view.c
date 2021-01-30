/*
 * Copyright (C) 2018 Santiago León O.
 */

#define UNSELECTED_ICON_BOX_STYLE       \
"box {"                                 \
"    padding: 6px;"                     \
"    border-radius: 3px;"               \
"    border: 1px solid rgba(0,0,0,0);"  \
"}"

#define SELECTED_ICON_BOX_STYLE \
"box {"                         \
"    padding: 6px;"             \
"    border-radius: 3px;"       \
"    border: 1px solid #777;"   \
"}"

GtkWidget *spaced_grid_new (int spacing)
{
    GtkWidget *new_grid = gtk_grid_new ();
    gtk_widget_set_margin_start (GTK_WIDGET(new_grid), spacing);
    gtk_widget_set_margin_end (GTK_WIDGET(new_grid), spacing);
    gtk_widget_set_margin_top (GTK_WIDGET(new_grid), spacing);
    gtk_widget_set_margin_bottom (GTK_WIDGET(new_grid), spacing);
    gtk_grid_set_row_spacing (GTK_GRID(new_grid), spacing);
    gtk_grid_set_column_spacing (GTK_GRID(new_grid), spacing);
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

// NOTE: requires #include <locale.h>
void bytes_to_human_readable (off_t size, char *buff, size_t buff_len)
{
    assert (buff_len > 9);

    int i = 0;
    float hr = size;
    while (hr > 1024) {
        i++;
        hr /= 1024;
    }

    char *unit = "?";
    if (i == 0) {
        unit = "B";
    } else if (i == 1) {
        unit = "K";
    } else if (i == 2) {
        unit = "M";
    } else if (i == 3) {
        unit = "G";
    } else if (i == 4) {
        unit = "T";
    } else if (i == 5) {
        unit = "P";
    } else if (i == 6) {
        unit = "E";
    } else if (i == 7) {
        unit = "Z";
    } else if (i == 8) {
        unit = "Y";
    }

    struct lconv *locale = localeconv();
    int int_part = (int)hr;
    int dec_part = (int)((hr - int_part)*10);
    if (dec_part != 0) {
        snprintf (buff, buff_len, "%d%s%d %s", int_part, locale->decimal_point, dec_part, unit);
    } else {
        snprintf (buff, buff_len, "%d %s", int_part, unit);
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

    bytes_to_human_readable (img->file_size, buff, ARRAY_SIZE(buff));
    data_dpy_append (data, "File Size:", buff, i++);

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

void set_icon_box_border (struct icon_image_t *img)
{
    img->custom_css = replace_custom_css (img->box, img->custom_css, SELECTED_ICON_BOX_STYLE);
}

void unset_icon_box_border (struct icon_image_t *img)
{
    img->custom_css = replace_custom_css (img->box, img->custom_css, UNSELECTED_ICON_BOX_STYLE);
}

gboolean on_image_clicked (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    struct icon_image_t *img = (struct icon_image_t *)user_data;
    if (img->view->selected_img != img) {
        GtkWidget *image_data_dpy = image_data_dpy_new (img);
        g_assert (img->view->image_data_dpy != NULL);

        unset_icon_box_border (img->view->selected_img);

        img->view->selected_img = img;
        set_icon_box_border (img);

        // Replace image_data_dpy widget
        GtkWidget *parent = gtk_widget_get_parent (img->view->image_data_dpy);
        gtk_container_remove (GTK_CONTAINER(parent), img->view->image_data_dpy);
        gtk_container_add (GTK_CONTAINER(parent), image_data_dpy);
        img->view->image_data_dpy = image_data_dpy;
        gtk_widget_show_all (image_data_dpy);
    }

    // NOTE: We allways let the event go through so we have drag and drop even
    // if the image wasn't selected.
    return FALSE;
}

char* new_bgcolor_style_str (mem_pool_t *pool, char *node_name, dvec4 color)
{
    char *str =
        pprintf (pool,
                 "%s {"
                 "    background-color: rgba(%d,%d,%d,%d);"
                 "}",
                 node_name,
                 (int)(color.r*255), (int)(color.g*255), (int)(color.b*255), (int)(color.a*255));
    return str;
}

void icon_view_update_bg_color (struct icon_view_t *icon_view, dvec4 color)
{
    mem_pool_t pool = {0};
    char *style = new_bgcolor_style_str (&pool, "scrolledwindow", color);
    icon_view->scrolled_window_custom_css =
        replace_custom_css (icon_view->scrolled_window, icon_view->scrolled_window_custom_css,
                            style);
    mem_pool_destroy (&pool);
}

void on_color_button_clicked (GtkColorButton *button, gpointer user_data)
{
    struct icon_view_t *icon_view = (struct icon_view_t*)user_data;
    GdkRGBA gdk_color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &gdk_color);
    dvec4 color = RGBA(gdk_color.red, gdk_color.green, gdk_color.blue, gdk_color.alpha);
    icon_view_update_bg_color (icon_view, color);
    app.bg_color = color;
}

void on_drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *data,
                       guint info, guint time, gpointer user_data)
{
    struct icon_image_t *img = (struct icon_image_t *)user_data;
    string_t uri = str_new ("file://");
    str_cat_c (&uri, img->full_path);

    char *uris[] = {str_data(&uri), NULL};
    gtk_selection_data_set_uris (data, uris);

    str_free (&uri);
}

GtkWidget* icon_view_create_icon_dpy (struct icon_view_t *icon_view, int scale)
{
    // NOTE: At least one package (aptdaemon-data) provides animated icons in a
    // single file by appending the frames side by side.  Here we detect that
    // case and instead display these icons vertically.
    GtkOrientation all_icons_or = icon_view->images[scale-1]->width/icon_view->images[scale-1]->height > 2 ?
        GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
    GtkWidget *all_icons = gtk_box_new (all_icons_or, 12);

    struct icon_image_t *last_img = NULL;
    struct icon_image_t *img = icon_view->images[scale-1];

    while (img != NULL) {
        GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
        img->box = box;
        img->custom_css = add_custom_css (box, UNSELECTED_ICON_BOX_STYLE);

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
        g_signal_connect (G_OBJECT(hitbox), "button-press-event", G_CALLBACK(on_image_clicked), img);

        // Setup DnD of images
        {
            gtk_drag_source_set (hitbox, GDK_BUTTON1_MASK, NULL, 0, GDK_ACTION_COPY);
            gtk_drag_source_add_uri_targets (hitbox);
            GdkPixbuf *pixbuf = gtk_image_get_pixbuf (GTK_IMAGE(img->image));
            gtk_drag_source_set_icon_pixbuf (hitbox, pixbuf);
            g_signal_connect (G_OBJECT(hitbox), "drag-data-get", G_CALLBACK(on_drag_data_get), img);
        }

        gtk_container_add (GTK_CONTAINER(hitbox), box);

        gtk_container_add (GTK_CONTAINER(all_icons), hitbox);

        if(img->next == NULL) {
            icon_view->selected_img = img;
            set_icon_box_border (img);
            last_img = img;
        }
        img = img->next;
    }

    // Place the icon list inside a GtkGrid so they are centered
    GtkWidget *icon_dpy = spaced_grid_new (12);
    gtk_widget_set_valign (icon_dpy, GTK_ALIGN_CENTER);
    gtk_widget_set_halign (icon_dpy, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (icon_dpy, TRUE);
    gtk_widget_set_vexpand (icon_dpy, TRUE);
    gtk_grid_attach (GTK_GRID(icon_dpy), all_icons, 0, 0, 1, 1);

    if (icon_view->image_data_dpy == NULL) {
        icon_view->image_data_dpy = image_data_dpy_new (last_img);
    } else {
        replace_wrapped_widget (&icon_view->image_data_dpy, image_data_dpy_new (last_img));
    }

    // Wrap icon_dpy into a GtkScrolledWindow
    GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    mem_pool_t pool = {0};
    icon_view->scrolled_window_custom_css =
        add_custom_css (scrolled_window, new_bgcolor_style_str (&pool, "scrolledwindow", app.bg_color));
    mem_pool_destroy (&pool);
    icon_view->scrolled_window = scrolled_window;

    gtk_widget_set_hexpand (scrolled_window, TRUE);
    gtk_widget_set_vexpand (scrolled_window, TRUE);
    gtk_container_add (GTK_CONTAINER (scrolled_window), icon_dpy);

    GdkRGBA c = GDK_RGBA_FROM_RGBA(app.bg_color);
    GtkWidget *button = gtk_color_button_new_with_rgba (&c);
    gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER(button), TRUE);
    add_custom_css (button,
                    "button {"
                    "    background-color: @bg_color;"
                    "}");

    g_signal_connect (G_OBJECT(button), "color-set", G_CALLBACK(on_color_button_clicked), icon_view);
    GtkWidget *toolbar = spaced_grid_new (6);
    gtk_container_add (GTK_CONTAINER (toolbar), button);

    GtkWidget *overlay = gtk_overlay_new ();
    gtk_container_add (GTK_CONTAINER (overlay), scrolled_window);
    gtk_overlay_add_overlay (GTK_OVERLAY(overlay), toolbar);
    gtk_overlay_set_overlay_pass_through (GTK_OVERLAY(overlay), toolbar, TRUE);

    return overlay;
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

void on_icon_view_theme_changed (GtkComboBox *themes_combobox, gpointer user_data)
{
    const char* theme_name = gtk_combo_box_get_active_id (themes_combobox);
    if (app.selected_theme_type == THEME_TYPE_ALL) {
        app_set_selected_theme (&app, theme_name);
        app_set_icon_view (&app, app.selected_icon);

    } else {
        app_set_normal_theme (&app, theme_name, app.selected_icon);
    }
}

GtkWidget* draw_icon_view (struct icon_view_t *icon_view)
{
    icon_view->icon_dpy = icon_view_create_icon_dpy (icon_view, 1);

    // Create the icon data pane
    GtkWidget *data_pane = spaced_grid_new (12);
    GtkWidget *icon_name_label = gtk_label_new (icon_view->icon_name);
    add_css_class (icon_name_label, "h2");
    gtk_label_set_selectable (GTK_LABEL(icon_name_label), TRUE);
    gtk_label_set_ellipsize (GTK_LABEL(icon_name_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign (icon_name_label, GTK_ALIGN_START);
    gtk_grid_attach (GTK_GRID(data_pane), icon_name_label, 0, 0, 1, 1);

    GtkWidget *icon_widgets = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 18);
    GtkWidget *theme_selector = NULL;
    if (app.selected_theme_type == THEME_TYPE_ALL || app.selected_theme_type == THEME_TYPE_NORMAL) {
        GtkWidget *themes_combobox;
        theme_selector = labeled_combobox_new ("Theme:", &themes_combobox);
        for (struct icon_theme_t *curr_theme = app.themes; curr_theme; curr_theme = curr_theme->next) {
            if (g_hash_table_contains (curr_theme->icon_names, icon_view->icon_name)) {
                combo_box_text_append_text_with_id (GTK_COMBO_BOX_TEXT(themes_combobox), curr_theme->name);
            }
        }

        gtk_combo_box_set_active_id (GTK_COMBO_BOX(themes_combobox), app.selected_theme->name);
        g_signal_connect (G_OBJECT(themes_combobox), "changed", G_CALLBACK (on_icon_view_theme_changed), NULL);
        gtk_container_add (GTK_CONTAINER(icon_widgets), theme_selector);
    }

    GtkWidget *scale_selector = scale_selector_new (icon_view);

    GtkWidget *widget_to_align = theme_selector != NULL ? theme_selector : scale_selector;
    gtk_widget_set_halign (widget_to_align, GTK_ALIGN_END);
    gtk_widget_set_hexpand (widget_to_align, TRUE);

    gtk_container_add (GTK_CONTAINER(icon_widgets), scale_selector);

    gtk_grid_attach (GTK_GRID(data_pane), icon_widgets, 1, 0, 1, 1);

    gtk_grid_attach (GTK_GRID(data_pane),
                     wrap_gtk_widget(icon_view->image_data_dpy),
                     0, 1, 1, 1);

    return fk_paned (GTK_ORIENTATION_VERTICAL,
                     wrap_gtk_widget(icon_view->icon_dpy),
                     data_pane);
}
