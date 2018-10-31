/*
 * Copiright (C) 2018 Santiago León O.
 */

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
GtkWidget *fk_paned (GtkOrientation orientation, GtkWidget *child1, GtkWidget *child2)
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

