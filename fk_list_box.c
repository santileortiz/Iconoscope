/*
 * Copiright (C) 2018 Santiago León O.
 */

// GtkListBox gets very slow as the number of row increases. Creation of the
// widget, the call to gtk_widget_show(), and then gtk_widget_destroy() are the
// slowest parts. For a widget of ~7000 rows it took more than 2 seconds to
// create it and show it. Destruction took 1 second.
//
// This is a much faster implementation of this widget based on GtkDrawingArea.
// It takes ~330us to create the same ~7000 row list. Render takes about 25ms,
// and destruction about 20us. More than 6000 times faster than GtkListBox!.
//
// Memory wise, it allocates 24 bytes per row, that's ~160KB for the ~7000 row
// widget.
//
// This doesn't duplicate any data from outside. Instead, each row stores a
// pointer to data owned by the caller. We don't allocate/free this data
// anywhere.
//
// TODO:
//  - Render time can be improved a lot by prerendering the list and just
//    blitting it when fk_list_box_draw_text_data is called. Then rendering the
//    selected row on top. @fast_render
//
//  - Define an API to let the user render rows with data different than text.
//    Right now if it's required, the user can create a new render function
//    based on fk_list_box_draw_text_data() but this may get a bit complex if
//    @fast_render is implemented.
//
//  - Don't hardcode styling, get it from the active CSS stylesheet.
//
//  - Add example code to show how the API works.
//
//  - Add test code that compares this to GtkListBox so it's easy to check when
//    it's better to use fk_list_box_t.

// If the following is defined fk_list_box_destroy() is called automatically when
// the widget gets destroyed.
#define FK_LIST_BOX_DESTROY_WITH_WIDGET

struct fk_list_box_t;

#define FK_LIST_BOX_ROW_SELECTED_CB(name) void name(struct fk_list_box_t *fk_list_box, int idx)
typedef FK_LIST_BOX_ROW_SELECTED_CB(fk_list_box_row_selected_cb_t);

struct fk_list_box_row_t {
    bool hidden;
    void *data;
};

struct fk_list_box_t {
    mem_pool_t pool;
    int num_rows;
    struct fk_list_box_row_t *rows;
    int num_visible_rows;
    struct fk_list_box_row_t **visible_rows;

    int selected_row_idx;
    struct fk_list_box_row_t *selected_row;
    double row_height;

    GtkWidget *widget;

    fk_list_box_row_selected_cb_t *row_selected_cb;

    // Number of rows that have been created
    int row_cnt;
};

gboolean fk_list_box_draw_text_data (GtkWidget *widget, cairo_t *cr, gpointer data)
{
    double margin_h = 6;
    double margin_v = 3;
    dvec4 text_color = RGB_255(66,66,66);
    dvec4 active_color = RGB_255(62,161,239);
    dvec4 active_text_color = RGB_255(255,255,255);
    dvec4 unfocused_color = RGB_255(204,204,204);
    dvec4 unfocused_text_color = RGB_255(51,51,51);

    struct fk_list_box_t *fk_list_box = (struct fk_list_box_t *)data;
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_paint (cr);

    if (fk_list_box->num_visible_rows == 0) {
        // TODO: Show a "list empty" message
        return TRUE;
    }

    cairo_set_source_rgb (cr, ARGS_RGB(text_color));

    cairo_select_font_face (cr, "Open Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size (cr, 12);

    cairo_font_extents_t font_extents;
    cairo_font_extents (cr, &font_extents);

    double width = 0;
    double y = font_extents.ascent + margin_v;
    int i;
    for (i=0; i<fk_list_box->num_visible_rows; i++) {
        cairo_move_to (cr, margin_h, y);
        cairo_show_text (cr, fk_list_box->visible_rows[i]->data);

        cairo_text_extents_t extents;
        cairo_text_extents (cr, fk_list_box->visible_rows[i]->data, &extents);
        width = MAX(width, extents.width + 2*margin_h);
        y += font_extents.ascent + font_extents.descent + 2*margin_v;
    }
    y -= font_extents.ascent + margin_v;

    fk_list_box->row_height = font_extents.ascent + font_extents.descent + 2*margin_v;

    gtk_widget_set_size_request (widget, width, y);

    if (!fk_list_box->selected_row->hidden) {
        assert (fk_list_box->selected_row_idx != -1);

        gboolean has_focus = gtk_widget_has_focus (widget);
        dvec4 selected_bg = has_focus ? active_color : unfocused_color;
        dvec4 selected_color = has_focus ? active_text_color : unfocused_text_color;

        // The rectangle for the selected row must go all the way across the
        // widget, INFINITY would be good width but Cairo doesn't draw this, so
        // we just use a very big number.
        double infinity_width = 1000000;
        cairo_rectangle (cr,
                         0, fk_list_box->selected_row_idx*fk_list_box->row_height,
                         infinity_width, fk_list_box->row_height);
        cairo_set_source_rgb (cr, ARGS_RGB(selected_bg));
        cairo_fill (cr);

        cairo_move_to (cr, margin_h,
                       fk_list_box->selected_row_idx*fk_list_box->row_height +
                       font_extents.ascent + margin_v);
        cairo_set_source_rgb (cr, ARGS_RGB(selected_color));
        cairo_show_text (cr, fk_list_box->visible_rows[fk_list_box->selected_row_idx]->data);
    }

    return TRUE;
}

void fk_list_box_rows_start (struct fk_list_box_t *fk_list_box, int num_rows)
{
    fk_list_box->row_cnt = 0;
    fk_list_box->num_rows = num_rows;
    fk_list_box->num_visible_rows = num_rows;
    fk_list_box->rows =
        mem_pool_push_size (&fk_list_box->pool,
                            fk_list_box->num_rows*sizeof(struct fk_list_box_row_t));
    fk_list_box->visible_rows =
        mem_pool_push_size (&fk_list_box->pool,
                            fk_list_box->num_rows*sizeof(struct fk_list_box_row_t*));
    fk_list_box->selected_row_idx = 0;
    fk_list_box->selected_row = &fk_list_box->rows[0];
}

struct fk_list_box_row_t* fk_list_box_row_new (struct fk_list_box_t *fk_list_box)
{
    struct fk_list_box_row_t *new_row = NULL;
    if (fk_list_box->row_cnt < fk_list_box->num_rows) {
        new_row = &fk_list_box->rows[fk_list_box->row_cnt];
        *new_row = ZERO_INIT (struct fk_list_box_row_t);
        fk_list_box->visible_rows[fk_list_box->row_cnt] = new_row;
        fk_list_box->row_cnt++;

    } else {
        printf ("fk_list_box_row_new(): Tried to allocate more rows than expected.\n");
    }

    return new_row;
}

void fk_list_box_refresh_hidden (struct fk_list_box_t *fk_list_box)
{
    int visible_cnt = 0;
    for (int i=0; i<fk_list_box->num_rows; i++) {
        if (!fk_list_box->rows[i].hidden) {
            fk_list_box->visible_rows[visible_cnt] = &fk_list_box->rows[i];
            visible_cnt++;
        }
    }
    fk_list_box->num_visible_rows = visible_cnt;

    if (!fk_list_box->selected_row->hidden &&
        fk_list_box->selected_row != fk_list_box->visible_rows[fk_list_box->selected_row_idx]) {
        // The selected row is visible and its index changed, compute the new one.
        // TODO: We can speed this up with binary search of pointers, as rows
        // have increasing addresses because they are in an array. Note that
        // this may break stuff if the user decides so sort rows by using the
        // visible_rows array. @performance
        for (int i=0; i<fk_list_box->num_visible_rows; i++) {
            if (fk_list_box->visible_rows[i] == fk_list_box->selected_row) {
                fk_list_box->selected_row_idx = i;
                break;
            }
        }
    }

    gtk_widget_queue_draw (fk_list_box->widget);
}

// This is used when the caller doesn't want the callbak to be called or a
// redraw to be queried. Use fk_list_box_change_selected() if you do.
// NOTE: idx is the index of the selected row in the visible_rows array.
void fk_list_box_set_selected (struct fk_list_box_t *fk_list_box, int idx)
{
    assert (idx >= 0 && idx < fk_list_box->num_visible_rows);

    fk_list_box->selected_row_idx = idx;
    fk_list_box->selected_row = fk_list_box->visible_rows[idx];

    GtkWidget *parent = gtk_widget_get_parent (fk_list_box->widget);
    if (parent && GTK_IS_SCROLLABLE (parent)) {
        GtkAdjustment *adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (parent));

        double y = fk_list_box->selected_row_idx*fk_list_box->row_height;
        gtk_adjustment_clamp_page (adjustment, y, y + fk_list_box->row_height);
    }
}

// NOTE: idx is the index of the selected row in the visible_rows array.
void fk_list_box_change_selected (struct fk_list_box_t *fk_list_box, int idx)
{
    assert (idx >= 0 && idx < fk_list_box->num_visible_rows);

    fk_list_box_set_selected (fk_list_box, idx);
    fk_list_box->row_selected_cb (fk_list_box, fk_list_box->selected_row_idx);
    gtk_widget_queue_draw (fk_list_box->widget);
}

gboolean fk_list_box_button_release (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    GdkEventButton *e = (GdkEventButton*)event;
    struct fk_list_box_t *fk_list_box = (struct fk_list_box_t *)data;
    int idx = (int) (e->y/fk_list_box->row_height);
    gtk_widget_grab_focus (widget);

    if (idx < fk_list_box->num_visible_rows) {
        fk_list_box_change_selected (fk_list_box, idx);
    }
    return TRUE;
}

gboolean fk_list_box_key_press (GtkWidget *widget, GdkEventKey *e, gpointer data)
{
    struct fk_list_box_t *fk_list_box = (struct fk_list_box_t *)data;
    int idx = -1;
    if (e->keyval == GDK_KEY_Up || e->keyval == GDK_KEY_KP_Up) {
        idx = MAX(0, fk_list_box->selected_row_idx-1);

    } else if (e->keyval == GDK_KEY_Down || e->keyval == GDK_KEY_KP_Down) {
        idx = MIN(fk_list_box->num_visible_rows-1, fk_list_box->selected_row_idx+1);
    }

    if (idx != -1) {
        fk_list_box_change_selected (fk_list_box, idx);
        return TRUE;
    } else {
        return FALSE;
    }
}

gboolean fk_list_box_unfocus (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_widget_queue_draw (widget);
    return TRUE;
}

void fk_list_box_destroy (struct fk_list_box_t *fk_list_box);
void fk_list_box_destroy_cb (GtkWidget *object, gpointer data)
{
    struct fk_list_box_t *fk_list_box = (struct fk_list_box_t *)data;
    fk_list_box_destroy (fk_list_box);
}

// NOTE: The caller should allocate a fk_list_box_t structure, and this function
// will initialize it. If the caller is thinking about reusing this structure
// better not to do that, use fk_list_box_new() and then the caller is only
// responsible for a pointer, memory will be freed when fk_list_box_destroy() is
// called (maybe called automatically when the GtkWidget is destroyed if the
// macro FK_LIST_BOX_DESTROY_WITH_WIDGET was defined).
GtkWidget* fk_list_box_init (struct fk_list_box_t *fk_list_box,
                             fk_list_box_row_selected_cb_t *row_selected_cb)
{
    *fk_list_box = ZERO_INIT (struct fk_list_box_t);
    fk_list_box->widget = gtk_drawing_area_new ();
    gtk_widget_set_vexpand (fk_list_box->widget, TRUE);
    gtk_widget_set_hexpand (fk_list_box->widget, TRUE);

    fk_list_box->row_selected_cb = row_selected_cb;

    // For some reason just adding GDK_BUTTON_RELEASE_MASK does not work...
    // GDK_BUTTON_PRESS_MASK is required too.
    gtk_widget_add_events (fk_list_box->widget,
                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                           GDK_KEY_PRESS_MASK |
                           GDK_FOCUS_CHANGE_MASK );

    g_signal_connect (G_OBJECT (fk_list_box->widget),
                      "draw",
                      G_CALLBACK (fk_list_box_draw_text_data),
                      fk_list_box);

    g_signal_connect (G_OBJECT (fk_list_box->widget),
                      "button-release-event",
                      G_CALLBACK (fk_list_box_button_release),
                      fk_list_box);

    g_signal_connect (G_OBJECT (fk_list_box->widget),
                      "key-press-event",
                      G_CALLBACK (fk_list_box_key_press),
                      fk_list_box);

    gtk_widget_set_can_focus (fk_list_box->widget, TRUE);
    g_signal_connect (G_OBJECT (fk_list_box->widget),
                      "focus-out-event",
                      G_CALLBACK (fk_list_box_unfocus),
                      fk_list_box);

#ifdef FK_LIST_BOX_DESTROY_WITH_WIDGET
    g_signal_connect (G_OBJECT (fk_list_box->widget),
                      "destroy",
                      G_CALLBACK (fk_list_box_destroy_cb),
                      fk_list_box);
#endif

    return fk_list_box->widget;
}

// Create a fk_list_box_t with a different ownership pattern. Here the created
// fk_list_box_t owns the memory of the fk_list_box_t structure. This is done by
// bootstrapping it from its own pool.
GtkWidget* fk_list_box_new (struct fk_list_box_t **fk_list_box,
                            fk_list_box_row_selected_cb_t *row_selected_cb)
{
    assert (fk_list_box != NULL && "You should get a pointer here so you can add rows");

    mem_pool_t bootstrap = ZERO_INIT (mem_pool_t);
    struct fk_list_box_t *new_fk_list_box =
        mem_pool_push_size (&bootstrap, sizeof (struct fk_list_box_t));
    new_fk_list_box->pool = bootstrap;
    *fk_list_box = new_fk_list_box;

    return fk_list_box_init (new_fk_list_box, row_selected_cb);
}

void fk_list_box_destroy (struct fk_list_box_t *fk_list_box)
{
    mem_pool_destroy (&fk_list_box->pool);
}
