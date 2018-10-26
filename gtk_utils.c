/*
 * Copiright (C) 2018 Santiago León O.
 */

#define RGBA DVEC4
#define RGB(r,g,b) DVEC4(r,g,b,1)
#define ARGS_RGBA(c) (c).r, (c).g, (c).b, (c).a
#define ARGS_RGB(c) (c.r), (c).g, (c).b
#define RGB_HEX(hex) DVEC4(((double)(((hex)&0xFF0000) >> 16))/255, \
                           ((double)(((hex)&0x00FF00) >>  8))/255, \
                           ((double)((hex)&0x0000FF))/255, 1)

#define GDK_RGBA(r,g,b,a) ((GdkRGBA){.red=r, .green=g, .blue=b, .alpha=a})
#define GDK_RGBA_FROM_RGBA(rgba) ((GdkRGBA){.red=rgba.r, .green=rgba.g, .blue=rgba.b, .alpha=rgba.a})

void add_css_class (GtkWidget *widget, char *class)
{
    GtkStyleContext *ctx = gtk_widget_get_style_context (widget);
    gtk_style_context_add_class (ctx, class);
}

void remove_css_class (GtkWidget *widget, char *class)
{
    GtkStyleContext *ctx = gtk_widget_get_style_context (widget);
    gtk_style_context_remove_class (ctx, class);
}

GtkCssProvider* add_global_css (gchar *css_data)
{
    GdkScreen *screen = gdk_screen_get_default ();
    GtkCssProvider *css_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (css_provider, css_data, -1, NULL);
    gtk_style_context_add_provider_for_screen (screen,
                                    GTK_STYLE_PROVIDER(css_provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    return css_provider;
}

// NOTE: I tried to dynamiccally change CSS styles in a GtkBox, and had a hard
// time. I added a custom CSS node named box with add_custom_css(), then I added
// a new CSS node named .my_class to the same widget. I thought adding/removing
// the class to the widget would switch styles but it didn't work, even when
// specifying priorities both the box node and the .my_class node conflicted
// with each other. I decided to only add custom CSS once to a widget, keep a
// reference to the GtkCssProvider and then use replace_custom_css(). I wish it
// wasn't so cumbersome.
GtkCssProvider* add_custom_css (GtkWidget *widget, gchar *css_data)
{
    GtkStyleContext *style_context = gtk_widget_get_style_context (widget);
    GtkCssProvider *css_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (css_provider, css_data, -1, NULL);
    gtk_style_context_add_provider (style_context,
                                    GTK_STYLE_PROVIDER(css_provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    return css_provider;
}

GtkCssProvider* replace_custom_css (GtkWidget *widget, GtkCssProvider *custom_css, gchar *css_data)
{
    GtkStyleContext *style_context = gtk_widget_get_style_context (widget);
    gtk_style_context_remove_provider (style_context, GTK_STYLE_PROVIDER(custom_css));

    return add_custom_css (widget, css_data);
}

int gtk_radio_button_get_idx (GtkRadioButton *button)
{
    GSList *group = gtk_radio_button_get_group (GTK_RADIO_BUTTON(button));
    // Items are added at the beginning of the group, so the index is reversed
    // from the order in which they were added.
    return g_slist_length (group) - g_slist_index (group, button);
}

// Widget Wrapping Idiom
// ---------------------
//
// Gtk uses reference counting of its objects. As a convenience for C
// programming they have a feature called "floating references", this feature
// makes most objects be created with a floating reference, instead of a normal
// one. Then the object that will take ownership of it, calls
// g_object_ref_sink() and becomes the owner of this floating reference making
// it a normal one. This allows C code to not call g_object_unref() on the newly
// created object and not leak a reference (and thus, not leak memory).
//
// The problem is this only work for objects that inherit from the
// GInitialyUnowned class. In all the Gtk class hierarchy ~150 classes inherit
// from GInitiallyUnowned and ~100 don't, which makes floating references the
// common case. What I normally do is assume things use floating references, and
// never call g_object_ref_sink() under the assumption that the function I'm
// passing the object to will sink it.
//
// Doing this for GtkWidgets is very useful to implement changes in the UI
// without creating a tangle of signals. What I do is completely replace widgets
// that need to be updated, instead of trying to update the state of objects to
// reflect changes. I keep weak references (C pointers, not even Gtk weak
// references) to widgets that may need to be replaced, then I remove the widget
// from it's container and replace it by a new one. Here not taking references
// to any widget becomes useful as removing the widget form it's parent will
// have the effect of destroying all the children objects (as long as Gtk
// internally isn't leaking memory).
//
// The only issue I've found is that containers like GtkPaned or GtkGrid use
// special _add methods with different declaration than gtk_container_add().
// It's very common to move widgets accross different kinds of containers, which
// adds the burden of having to kneo where in the code we assumed something was
// stored in a GtkGrid or a GtkPaned etc. What I do to alieviate this pain is
// wrap widgets that will be replaced into a GtkBox with wrap_gtk_widget() and
// then use replace_wrapped_widget() to replace it.
//
// Doing this ties the lifespan of an GInitiallyUnowned object to the lifespan
// of the parent. Sometimes this is not desired, in those cases we must call
// g_object_ref_sink() (NOT g_object_ref(), because as far as I understand it
// will set refcount to 2 and never free them). Fortunatelly cases where this is
// required will print lots of Critical warnings or crash the application so
// they are easy to detect and fix, a memory leak is harder to detect/fix.
//
// NOTE: The approach of storing a pointer to the parent widget not only doubles
// the number of pointers that need to be stored for a replacable widget, but
// also doesn't work because we can't know the true parent of a widget. For
// instance, GtkScrolledWindow wraps its child in a GtkViewPort.
//
// TODO: I need a way to detect when I'm using objects that don't inherit from
// GInitiallyUnowned and be sure we are not leaking references. Also make sure
// g_object_ref_sink() is used instead of g_object_ref().
GtkWidget* wrap_gtk_widget (GtkWidget *widget)
{
    GtkWidget *wrapper = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER(wrapper), widget);
    return wrapper;
}

void replace_wrapped_widget (GtkWidget **original, GtkWidget *new_widget)
{
    BEGIN_WALL_CLOCK;
    GtkWidget *parent = gtk_widget_get_parent (*original);
    gtk_container_remove (GTK_CONTAINER(parent), *original);
    PROBE_WALL_CLOCK("    Remove");

    *original = new_widget;
    gtk_container_add (GTK_CONTAINER(parent), new_widget);
    PROBE_WALL_CLOCK("    Add");

    gtk_widget_show_all (new_widget);
    PROBE_WALL_CLOCK("    Show");
    printf ("\n");
}

// An issue with the wrapped widget idiom is that if a widget triggers a replace
// of one of its ancestors, then we are effectiveley destroying ourselves from
// the signal handler. I've found cases where Gtk does not like that and shows
// lots of critical errors. I think these errors happen because the signal where
// we triggered the destruction of the ancestor is not the only one being
// handled, then after destruction, other handlers try to trigger inside the
// widget that does not exist anymore.
//
// I don't know if this is a Gtk bug, or we are not supposed to trigger the
// destruction of an object from the signal handler of a signal of the object
// that will be destroyed. The case I found not to work was a GtkCombobox
// destroying itself from the "changed" signal. A button destroying itself from
// the "clicked" signal worked fine though.
//
// In any case, if this happens, the function replace_wrapped_widget_defered()
// splits widget replacement so that widget destruction is triggered, and then
// in the handler for the destroy signal, we actually replace the widget. Not
// nice, but seems to solve the issue.
struct replace_wrapped_widget_closure_t {
    GtkWidget **original;
    GtkWidget *parent;
    GtkWidget *new_widget;
};

gboolean idle_widget_destroy (gpointer user_data)
{
    gtk_widget_destroy ((GtkWidget *) user_data);
    return FALSE;
}

void replace_wrapped_widget_defered_cb (GtkWidget *object, gpointer user_data)
{
    struct replace_wrapped_widget_closure_t *clsr = (struct replace_wrapped_widget_closure_t *) user_data;
    *clsr->original = clsr->new_widget;
    gtk_container_add (GTK_CONTAINER(clsr->parent), clsr->new_widget);
    gtk_widget_show_all (clsr->new_widget);
    free (clsr);
}

void replace_wrapped_widget_defered (GtkWidget **original, GtkWidget *new_widget)
{
    struct replace_wrapped_widget_closure_t *clsr = malloc (sizeof (struct replace_wrapped_widget_closure_t));
    clsr->original = original;
    clsr->parent = gtk_widget_get_parent (*original);
    clsr->new_widget = new_widget;
    g_signal_connect (G_OBJECT(*original), "destroy", G_CALLBACK (replace_wrapped_widget_defered_cb), clsr);
    g_idle_add (idle_widget_destroy, *original);
}

// Convenience wrapper for setting properties in objects when they don't have
// setters.
void g_object_set_property_bool (GObject *object, const char *property_name, gboolean value)
{
    GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_BOOLEAN);
    g_value_set_boolean (&val, value);
    g_object_set_property (object, property_name, &val);
}

// FIXME: The following CSS is used to work around 2 issues with GtkPaned. One
// is a failed assert which is a bug in GTK (see
// https://github.com/elementary/stylesheet/issues/328). The other one is the
// vanishing of the separator, which seems to be related to elementary OS using
// a negative margin in the separator.
GtkWidget *fix_gtk_paned_new (GtkOrientation orientation)
{
    GtkWidget *res = gtk_paned_new (orientation);
    add_custom_css (res, "paned > separator {"
                    "    margin-right: 0;"
                    "    min-width: 2px;"
                    "    min-height: 2px;"
                    "}");
    return res;
}

// This is the only way I found to disable horizontal scrolling in a scrolled
// window. I think calling gtk_adjustment_set_upper() 'should' work, but it
// doesn't.
void _gtk_scrolled_window_disable_hscroll_cb (GtkAdjustment *adjustment, gpointer user_data)
{
    if (gtk_adjustment_get_value (adjustment) != 0) {
        gtk_adjustment_set_value (adjustment, 0);
    }
}

void gtk_scrolled_window_disable_hscroll (GtkScrolledWindow *scrolled_window)
{
    gtk_scrolled_window_set_policy (scrolled_window, GTK_POLICY_EXTERNAL, GTK_POLICY_AUTOMATIC);
    GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment (scrolled_window);
#if 1
    g_signal_connect (G_OBJECT(hadj),
                      "value-changed",
                      G_CALLBACK (_gtk_scrolled_window_disable_hscroll_cb),
                      NULL);
#else
    // FIXME: Why U not work!?
    gtk_adjustment_set_upper (hadj, 0);
#endif
}

// The implementation of GtkPaned is so broken that faking it by using a GtkGrid
// works better. This still lacks the resizability which is why I use
// fix_gtk_paned_new(), but at this point I think it will be much simpler to
// just make my own implementation of a Paned container than try to make
// GtkPaned work. Some of the issues found are:
//
//   - There is no way to position the separator based on a percentage. Trying
//     to implement this required a lot of code, and even then, it didn't work.
//
//   - Trying to set the position as pixels works, except when doing it from
//     inside the handler for size-allocate. Which is what made the
//     implementation of a percentage positioning not work.
//
//   - Thin separators (1px) are broken. See the comment above in
//     fix_gtk_paned_new().
//
//   - Even after solving the bug and writing to the mailing list about it, I
//     got no response (it was a single line change). Which makes me think no
//     one really wants to touch that code. And with good reason, it's very
//     weird and has strange corner cases (why is there a position-set property
//     to activate/deactivate the position property?).
//
//   - Even when working around the bugs, 1px separators are still useless.
//     The hitbox is 1px wide which makes them extremely annoying.
//
//   - Sometimes (who knows why) the first child is not drawn, but it is
//     allocated.
//
//   - Reproducing these bugs on simpler test cases is hard, because they depend
//     on the order in which size allocations happen.
//
// I'm very tired of this. This is the only way I found to create something that
// looks like a static paned container, that actually shows both widgets at
// startup, and where I can control the position to be tight around one of the
// children.
GtkWidget *fake_paned (GtkOrientation orientation, GtkWidget *child1, GtkWidget *child2)
{
    GtkWidget *new_paned = gtk_grid_new ();
    GtkOrientation sep_or = orientation == GTK_ORIENTATION_VERTICAL?
        GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    GtkWidget *sep = gtk_separator_new (sep_or);
    if (orientation == GTK_ORIENTATION_VERTICAL) {
        gtk_grid_attach (GTK_GRID(new_paned), child1, 0, 0, 1, 1);
        gtk_grid_attach (GTK_GRID(new_paned), sep,    0, 1, 1, 1);
        gtk_grid_attach (GTK_GRID(new_paned), child2, 0, 2, 1, 1);
    } else {
        gtk_grid_attach (GTK_GRID(new_paned), child1, 0, 0, 1, 1);
        gtk_grid_attach (GTK_GRID(new_paned), sep,    1, 0, 1, 1);
        gtk_grid_attach (GTK_GRID(new_paned), child2, 2, 0, 1, 1);
    }
    return new_paned;
}

GtkWidget *labeled_combobox_new (char *label, GtkWidget **combobox)
{
    assert (combobox != NULL && "You really need the combobox to fill it.");

    GtkWidget *label_widget = gtk_label_new (label);
    add_css_class (label_widget, "h4");
    gtk_widget_set_margin_start (label_widget, 6);

    GtkWidget *combobox_widget = gtk_combo_box_text_new ();
    gtk_widget_set_margin_top (combobox_widget, 6);
    gtk_widget_set_margin_bottom (combobox_widget, 6);
    gtk_widget_set_margin_start (combobox_widget, 12);
    gtk_widget_set_margin_end (combobox_widget, 6);

    GtkWidget *container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add (GTK_CONTAINER(container), label_widget);
    gtk_container_add (GTK_CONTAINER(container), combobox_widget);

    *combobox = combobox_widget;
    return container;
}

void combo_box_text_append_text_with_id (GtkComboBoxText *combobox, const gchar *text)
{
    gtk_combo_box_text_append (combobox, text, text);
}
