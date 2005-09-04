#define __SP_DESKTOP_WIDGET_C__

/** \file
 * Desktop widget implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   MenTaLguY <mental@rydia.net>
 *   bulia byak <buliabyak@users.sf.net>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *
 * Copyright (C) 2004 MenTaLguY
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <glibmm/i18n.h>

#include <gtk/gtk.h>

#include "macros.h"
#include "forward.h"
#include "inkscape-private.h"
#include "desktop.h"
#include "desktop-events.h"
#include "desktop-affine.h"
#include "document.h"
#include "selection.h"
#include "select-context.h"
#include "desktop-widget.h"
#include "sp-namedview.h"
#include "interface.h"
#include "toolbox.h"
#include "prefs-utils.h"
#include "file.h"
#include "display/canvas-arena.h"
#include "display/gnome-canvas-acetate.h"
#include <extension/db.h>
#include "helper/units.h"
#include "widgets/button.h"
#include "widgets/ruler.h"
#include "widgets/icon.h"
#include "widgets/widget-sizes.h"
#include "widgets/spw-utilities.h"
#include "widgets/spinbutton-events.h"
#include "widgets/layer-selector.h"

#ifdef WITH_INKBOARD
#include "jabber_whiteboard/session-manager.h"
#endif



enum {
    ACTIVATE,
    DEACTIVATE,
    MODIFIED,
    EVENT_CONTEXT_CHANGED,
    LAST_SIGNAL
};


//---------------------------------------------------------------------
/* SPDesktopWidget */

static void sp_desktop_widget_class_init (SPDesktopWidgetClass *klass);
static void sp_desktop_widget_init (SPDesktopWidget *widget);
static void sp_desktop_widget_destroy (GtkObject *object);

static void sp_desktop_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void sp_desktop_widget_realize (GtkWidget *widget);

static gint sp_desktop_widget_event (GtkWidget *widget, GdkEvent *event, SPDesktopWidget *dtw);



static void sp_desktop_widget_adjustment_value_changed (GtkAdjustment *adj, SPDesktopWidget *dtw);
static void sp_desktop_widget_namedview_modified (SPNamedView *nv, guint flags, SPDesktopWidget *dtw);

static gdouble sp_dtw_zoom_value_to_display (gdouble value);
static gdouble sp_dtw_zoom_display_to_value (gdouble value);
static gint sp_dtw_zoom_input (GtkSpinButton *spin, gdouble *new_val, gpointer data);
static bool sp_dtw_zoom_output (GtkSpinButton *spin, gpointer data);
static void sp_dtw_zoom_value_changed (GtkSpinButton *spin, gpointer data);
static void sp_dtw_zoom_populate_popup (GtkEntry *entry, GtkMenu *menu, gpointer data);
static void sp_dtw_zoom_menu_handler (SPDesktop *dt, gdouble factor);
static void sp_dtw_zoom_50 (GtkMenuItem *item, gpointer data);
static void sp_dtw_zoom_100 (GtkMenuItem *item, gpointer data);
static void sp_dtw_zoom_200 (GtkMenuItem *item, gpointer data);
static void sp_dtw_zoom_page (GtkMenuItem *item, gpointer data);
static void sp_dtw_zoom_drawing (GtkMenuItem *item, gpointer data);
static void sp_dtw_zoom_selection (GtkMenuItem *item, gpointer data);

SPViewWidgetClass *dtw_parent_class;

void 
SPDesktopWidget::setMessage (Inkscape::MessageType type, const gchar *message)
{
    GtkLabel *sb=GTK_LABEL(this->select_status);
    gtk_label_set_markup (sb, message ? message : "");
}

void 
SPDesktopWidget::window_get_pointer (int *x, int *y)
{
    gdk_window_get_pointer (GTK_WIDGET (canvas)->window, x, y, NULL);
}

/**
 * Registers SPDesktopWidget class and returns its type number.
 */
GtkType
sp_desktop_widget_get_type (void)
{
    static GtkType type = 0;
    if (!type) {
        static const GtkTypeInfo info = {
            "SPDesktopWidget",
            sizeof (SPDesktopWidget),
            sizeof (SPDesktopWidgetClass),
            (GtkClassInitFunc) sp_desktop_widget_class_init,
            (GtkObjectInitFunc) sp_desktop_widget_init,
            NULL, NULL, NULL
        };
        type = gtk_type_unique (SP_TYPE_VIEW_WIDGET, &info);
    }
    return type;
}

/**
 * SPDesktopWidget vtable initialization
 */
static void
sp_desktop_widget_class_init (SPDesktopWidgetClass *klass)
{
    dtw_parent_class = (SPViewWidgetClass*)gtk_type_class (SP_TYPE_VIEW_WIDGET);

    GtkObjectClass *object_class = (GtkObjectClass *) klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

    object_class->destroy = sp_desktop_widget_destroy;

    widget_class->size_allocate = sp_desktop_widget_size_allocate;
    widget_class->realize = sp_desktop_widget_realize;
}

/**
 * Callback for SPDesktopWidget object initialization.
 */
static void
sp_desktop_widget_init (SPDesktopWidget *dtw)
{
    GtkWidget *widget;
    GtkWidget *tbl;
    GtkWidget *w;

    GtkWidget *hbox;
    GtkWidget *eventbox;
    GtkStyle *style;

    widget = GTK_WIDGET (dtw);

    dtw->desktop = NULL;

    dtw->tt = gtk_tooltips_new ();

    /* Main table */
    dtw->vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (dtw), dtw->vbox);

    dtw->statusbar = gtk_hbox_new (FALSE, 0);
    //gtk_widget_set_usize (dtw->statusbar, -1, BOTTOM_BAR_HEIGHT);
    gtk_box_pack_end (GTK_BOX (dtw->vbox), dtw->statusbar, FALSE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_end (GTK_BOX (dtw->vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show (hbox);

    dtw->aux_toolbox = sp_aux_toolbox_new ();
    gtk_box_pack_end (GTK_BOX (dtw->vbox), dtw->aux_toolbox, FALSE, TRUE, 0);

    dtw->commands_toolbox = sp_commands_toolbox_new ();
    gtk_box_pack_end (GTK_BOX (dtw->vbox), dtw->commands_toolbox, FALSE, TRUE, 0);

    dtw->tool_toolbox = sp_tool_toolbox_new ();
    gtk_box_pack_start (GTK_BOX (hbox), dtw->tool_toolbox, FALSE, TRUE, 0);

    tbl = gtk_table_new (4, 3, FALSE);
    gtk_box_pack_start (GTK_BOX (hbox), tbl, TRUE, TRUE, 1);

    /* Horizontal ruler */
    eventbox = gtk_event_box_new ();
    dtw->hruler = sp_hruler_new ();
    dtw->hruler_box = eventbox;
    sp_ruler_set_metric (GTK_RULER (dtw->hruler), SP_PT);
    gtk_tooltips_set_tip (dtw->tt, dtw->hruler_box, gettext(sp_unit_get_plural (&sp_unit_get_by_id(SP_UNIT_PT))), NULL);
    gtk_container_add (GTK_CONTAINER (eventbox), dtw->hruler);
    gtk_table_attach (GTK_TABLE (tbl), eventbox, 1, 2, 0, 1, (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(GTK_FILL), widget->style->xthickness, 0);
    g_signal_connect (G_OBJECT (eventbox), "button_press_event", G_CALLBACK (sp_dt_hruler_event), dtw);
    g_signal_connect (G_OBJECT (eventbox), "button_release_event", G_CALLBACK (sp_dt_hruler_event), dtw);
    g_signal_connect (G_OBJECT (eventbox), "motion_notify_event", G_CALLBACK (sp_dt_hruler_event), dtw);

    /* Vertical ruler */
    eventbox = gtk_event_box_new ();
    dtw->vruler = sp_vruler_new ();
    dtw->vruler_box = eventbox;
    sp_ruler_set_metric (GTK_RULER (dtw->vruler), SP_PT);
    gtk_tooltips_set_tip (dtw->tt, dtw->vruler_box, gettext(sp_unit_get_plural (&sp_unit_get_by_id(SP_UNIT_PT))), NULL);
    gtk_container_add (GTK_CONTAINER (eventbox), GTK_WIDGET (dtw->vruler));
    gtk_table_attach (GTK_TABLE (tbl), eventbox, 0, 1, 1, 2, (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(GTK_FILL), 0, widget->style->ythickness);
    g_signal_connect (G_OBJECT (eventbox), "button_press_event", G_CALLBACK (sp_dt_vruler_event), dtw);
    g_signal_connect (G_OBJECT (eventbox), "button_release_event", G_CALLBACK (sp_dt_vruler_event), dtw);
    g_signal_connect (G_OBJECT (eventbox), "motion_notify_event", G_CALLBACK (sp_dt_vruler_event), dtw);

    /* Horizontal scrollbar */
    dtw->hadj = (GtkAdjustment *) gtk_adjustment_new (0.0, -4000.0, 4000.0, 10.0, 100.0, 4.0);
    dtw->hscrollbar = gtk_hscrollbar_new (GTK_ADJUSTMENT (dtw->hadj));
    gtk_table_attach (GTK_TABLE (tbl), dtw->hscrollbar, 1, 2, 2, 3, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), (GtkAttachOptions)(GTK_FILL), 0, 0);
    /* Vertical scrollbar and the sticky zoom button */
    dtw->vscrollbar_box = gtk_vbox_new (FALSE, 0);
    dtw->sticky_zoom = sp_button_new_from_data ( GTK_ICON_SIZE_MENU,
                                                 SP_BUTTON_TYPE_TOGGLE,
                                                 NULL,
                                                 "sticky_zoom",
                                                 _("Zoom drawing if window size changes"),
                                                 dtw->tt);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dtw->sticky_zoom), prefs_get_int_attribute ("options.stickyzoom", "value", 0));
    gtk_box_pack_start (GTK_BOX (dtw->vscrollbar_box), dtw->sticky_zoom, FALSE, FALSE, 0);
    dtw->vadj = (GtkAdjustment *) gtk_adjustment_new (0.0, -4000.0, 4000.0, 10.0, 100.0, 4.0);
    dtw->vscrollbar = gtk_vscrollbar_new (GTK_ADJUSTMENT (dtw->vadj));
    gtk_box_pack_start (GTK_BOX (dtw->vscrollbar_box), dtw->vscrollbar, TRUE, TRUE, 0);
    gtk_table_attach (GTK_TABLE (tbl), dtw->vscrollbar_box, 2, 3, 0, 2, (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

    /* Canvas */
    w = gtk_frame_new (NULL);
    gtk_table_attach (GTK_TABLE (tbl), w, 1, 2, 1, 2, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);
    dtw->canvas = SP_CANVAS (sp_canvas_new_aa ());
    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (dtw->canvas), GTK_CAN_FOCUS);
    style = gtk_style_copy (GTK_WIDGET (dtw->canvas)->style);
    style->bg[GTK_STATE_NORMAL] = style->white;
    gtk_widget_set_style (GTK_WIDGET (dtw->canvas), style);
    gtk_widget_set_extension_events(GTK_WIDGET (dtw->canvas) , GDK_EXTENSION_EVENTS_ALL);
    g_signal_connect (G_OBJECT (dtw->canvas), "event", G_CALLBACK (sp_desktop_widget_event), dtw);
    gtk_container_add (GTK_CONTAINER (w), GTK_WIDGET (dtw->canvas));

    // zoom status spinbutton
    dtw->zoom_status = gtk_spin_button_new_with_range (log(SP_DESKTOP_ZOOM_MIN)/log(2), log(SP_DESKTOP_ZOOM_MAX)/log(2), 0.1);
    gtk_tooltips_set_tip (dtw->tt, dtw->zoom_status, _("Zoom"), NULL);
    gtk_widget_set_size_request (dtw->zoom_status, STATUS_ZOOM_WIDTH, -1);
    gtk_entry_set_width_chars (GTK_ENTRY (dtw->zoom_status), 6);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (dtw->zoom_status), FALSE);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (dtw->zoom_status), GTK_UPDATE_ALWAYS);
    g_signal_connect (G_OBJECT (dtw->zoom_status), "input", G_CALLBACK (sp_dtw_zoom_input), dtw);
    g_signal_connect (G_OBJECT (dtw->zoom_status), "output", G_CALLBACK (sp_dtw_zoom_output), dtw);
    gtk_object_set_data (GTK_OBJECT (dtw->zoom_status), "dtw", dtw->canvas);
    gtk_object_set_data (GTK_OBJECT (dtw), "altz", dtw->zoom_status);
    gtk_signal_connect (GTK_OBJECT (dtw->zoom_status), "focus-in-event", GTK_SIGNAL_FUNC (spinbutton_focus_in), dtw->zoom_status);
    gtk_signal_connect (GTK_OBJECT (dtw->zoom_status), "key-press-event", GTK_SIGNAL_FUNC (spinbutton_keypress), dtw->zoom_status);
    dtw->zoom_update = g_signal_connect (G_OBJECT (dtw->zoom_status), "value_changed", G_CALLBACK (sp_dtw_zoom_value_changed), dtw);
    dtw->zoom_update = g_signal_connect (G_OBJECT (dtw->zoom_status), "populate_popup", G_CALLBACK (sp_dtw_zoom_populate_popup), dtw);
    sp_set_font_size (dtw->zoom_status, STATUS_ZOOM_FONT_SIZE);
    gtk_box_pack_start (GTK_BOX (dtw->statusbar), dtw->zoom_status, FALSE, FALSE, 0);

    /* connecting canvas, scrollbars, rulers, statusbar */
    g_signal_connect (G_OBJECT (dtw->hadj), "value-changed", G_CALLBACK (sp_desktop_widget_adjustment_value_changed), dtw);
    g_signal_connect (G_OBJECT (dtw->vadj), "value-changed", G_CALLBACK (sp_desktop_widget_adjustment_value_changed), dtw);

    // cursor coordinates
    dtw->coord_status = gtk_label_new ("");
    eventbox = gtk_event_box_new ();
    gtk_container_add (GTK_CONTAINER (eventbox), dtw->coord_status);
    gtk_tooltips_set_tip (dtw->tt, eventbox, _("Cursor coordinates"), NULL);
    gtk_widget_set_size_request (dtw->coord_status, STATUS_COORD_WIDTH, -1);
    sp_set_font_size (dtw->coord_status, STATUS_COORD_FONT_SIZE);
    gtk_box_pack_start (GTK_BOX (dtw->statusbar), eventbox, FALSE, FALSE, 1);

    dtw->layer_selector = new Inkscape::Widgets::LayerSelector(NULL);
    dtw->layer_selector->reference();
    //dtw->layer_selector->set_size_request(-1, SP_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(dtw->statusbar), GTK_WIDGET(dtw->layer_selector->gobj()), FALSE, FALSE, 1);

    dtw->select_status = gtk_label_new (NULL);//gtk_statusbar_new ();
    gtk_misc_set_alignment (GTK_MISC (dtw->select_status), 0.0, 0.5);
    gtk_widget_set_size_request (dtw->select_status, 1, -1);
    //sp_set_font_size (dtw->select_status, STATUS_BAR_FONT_SIZE);  // setting absolute size is bad, in some themes it ends up being larger!
    // display the initial welcome message in the statusbar
    gtk_label_set_markup (GTK_LABEL (dtw->select_status), _("<b>Welcome to Inkscape!</b> Use shape or freehand tools to create objects; use selector (arrow) to move or transform them."));
    // space label 2 pixels from left edge
    gtk_box_pack_start (GTK_BOX (dtw->statusbar), gtk_hbox_new(FALSE, 0), FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (dtw->statusbar), dtw->select_status, TRUE, TRUE, 0);

    gtk_widget_show_all (dtw->vbox);
}

/**
 * Called before SPDesktopWidget destruction.
 */
static void
sp_desktop_widget_destroy (GtkObject *object)
{
    SPDesktopWidget *dtw = SP_DESKTOP_WIDGET (object);

    dtw->layer_selector->unreference();

    if (dtw->desktop) {
        inkscape_remove_desktop (dtw->desktop); // clears selection too
        Inkscape::GC::release (dtw->desktop);
        dtw->desktop = NULL;
    }

    if (GTK_OBJECT_CLASS (dtw_parent_class)->destroy) {
        (* GTK_OBJECT_CLASS (dtw_parent_class)->destroy) (object);
    }
}

static void
sp_desktop_widget_set_title (SPDesktopWidget *dtw)
{
    sp_desktop_widget_set_title (dtw, SP_DOCUMENT_NAME (dtw->desktop->doc()));
}
/**
 * Set the title in the desktop-window (if desktop has an own window).
 * 
 * The title has form file name: desktop number - Inkscape.
 * The desktop number is only shown if it's 2 or higher,
 */
void
sp_desktop_widget_set_title (SPDesktopWidget *dtw, gchar const* uri)
{
    GtkWindow *window = GTK_WINDOW (gtk_object_get_data (GTK_OBJECT(dtw), "window"));
    if (window) {
        gchar const *fname = ( TRUE
                               ? uri
                               : g_basename(uri) );
        GString *name = g_string_new ("");
        if (dtw->desktop->number > 1) {
            g_string_printf (name, _("%s: %d - Inkscape"), fname, dtw->desktop->number);
        } else {
            g_string_printf (name, _("%s - Inkscape"), fname);
        }
        gtk_window_set_title (window, name->str);
        g_string_free (name, TRUE);
    }
}

/**
 * Callback to allocate space for desktop widget.
 */
static void
sp_desktop_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    SPDesktopWidget *dtw = SP_DESKTOP_WIDGET (widget);

    if ((allocation->x == widget->allocation.x) &&
        (allocation->y == widget->allocation.y) &&
        (allocation->width == widget->allocation.width) &&
        (allocation->height == widget->allocation.height)) {
        if (GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate)
            GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate (widget, allocation);
        return;
    }

    if (GTK_WIDGET_REALIZED (widget)) {
        NRRect area;
        double zoom;
        dtw->desktop->get_display_area (&area);
        zoom = dtw->desktop->current_zoom();

        if (GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate)
            GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate (widget, allocation);

        if (SP_BUTTON_IS_DOWN (dtw->sticky_zoom)) {
            NRRect newarea;
            double zpsp;
            /* Calculate zoom per pixel */
            zpsp = zoom / hypot (area.x1 - area.x0, area.y1 - area.y0);
            /* Find new visible area */
            dtw->desktop->get_display_area (&newarea);
            /* Calculate adjusted zoom */
            zoom = zpsp * hypot (newarea.x1 - newarea.x0, newarea.y1 - newarea.y0);
            dtw->desktop->zoom_absolute (0.5F * (area.x1 + area.x0), 0.5F * (area.y1 + area.y0), zoom);
        } else {
            dtw->desktop->zoom_absolute (0.5F * (area.x1 + area.x0), 0.5F * (area.y1 + area.y0), zoom);
        }

    } else {
        if (GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate)
            GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate (widget, allocation);
//            this->size_allocate (widget, allocation);
    }
}

/**
 * Callback to realize desktop widget.
 */
static void
sp_desktop_widget_realize (GtkWidget *widget)
{

    SPDesktopWidget *dtw = SP_DESKTOP_WIDGET (widget);

    if (GTK_WIDGET_CLASS (dtw_parent_class)->realize)
        (* GTK_WIDGET_CLASS (dtw_parent_class)->realize) (widget);

    NRRect d;
    d.x0 = 0.0;
    d.y0 = 0.0;
    d.x1 = sp_document_width (dtw->desktop->doc());
    d.y1 = sp_document_height (dtw->desktop->doc());

    if ((fabs (d.x1 - d.x0) < 1.0) || (fabs (d.y1 - d.y0) < 1.0)) return;

    dtw->desktop->set_display_area (d.x0, d.y0, d.x1, d.y1, 10);

    /* Listen on namedview modification */
    g_signal_connect (G_OBJECT (dtw->desktop->namedview), "modified", G_CALLBACK (sp_desktop_widget_namedview_modified), dtw);
    sp_desktop_widget_namedview_modified (dtw->desktop->namedview, SP_OBJECT_MODIFIED_FLAG, dtw);

    sp_desktop_widget_set_title (dtw);
}

/**
 * Callback to handle desktop widget event.
 */
static gint
sp_desktop_widget_event (GtkWidget *widget, GdkEvent *event, SPDesktopWidget *dtw)
{
    if (event->type == GDK_BUTTON_PRESS) {
        // defocus any spinbuttons
        gtk_widget_grab_focus (GTK_WIDGET(dtw->canvas));
    }

    if ((event->type == GDK_BUTTON_PRESS) && (event->button.button == 3)) {
        if (event->button.state & GDK_SHIFT_MASK) {
            sp_canvas_arena_set_sticky (SP_CANVAS_ARENA (dtw->desktop->drawing), TRUE);
        } else {
            sp_canvas_arena_set_sticky (SP_CANVAS_ARENA (dtw->desktop->drawing), FALSE);
        }
    }

    if (GTK_WIDGET_CLASS (dtw_parent_class)->event) {
        return (* GTK_WIDGET_CLASS (dtw_parent_class)->event) (widget, event);
    } else {
        // The keypress events need to be passed to desktop handler explicitly,
        // because otherwise the event contexts only receive keypresses when the mouse cursor
        // is over the canvas. This redirection is only done for keypresses and only if there's no
        // current item on the canvas, because item events and all mouse events are caught
        // and passed on by the canvas acetate (I think). --bb
        if (event->type == GDK_KEY_PRESS && !dtw->canvas->current_item) {
            return sp_desktop_root_handler (NULL, event, dtw->desktop);
        }
    }

    return FALSE;
}

void
sp_dtw_desktop_activate (SPDesktop *desktop, SPDesktopWidget *dtw)
{
    /* update active desktop indicator */
}

void
sp_dtw_desktop_deactivate (SPDesktop *desktop, SPDesktopWidget *dtw)
{
    /* update inactive desktop indicator */
}

/**
 *  Shuts down the desktop object for the view being closed.  It checks
 *  to see if the document has been edited, and if so prompts the user
 *  to save, discard, or cancel.  Returns TRUE if the shutdown operation
 *  is cancelled or if the save is cancelled or fails, FALSE otherwise.
 */
bool 
SPDesktopWidget::shutdown()
{
    g_assert(desktop != NULL);
    if (inkscape_is_sole_desktop_for_document(*desktop)) {
        SPDocument *doc = desktop->doc();
        if (sp_document_repr_root(doc)->attribute("sodipodi:modified") != NULL) {
            GtkWidget *dialog;

            dialog = gtk_message_dialog_new(
                GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(this))),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_NONE,
                "Document modified");

            gchar *markup;
            /** \todo
             * FIXME !!! obviously this will have problems if the document 
             * name contains markup characters
             */
            markup = g_strdup_printf(
                _("<span weight=\"bold\" size=\"larger\">Save changes to document \"%s\" before closing?</span>\n\n"
                  "If you close without saving, your changes will be discarded."),
                SP_DOCUMENT_NAME(doc));

            /** \todo
             * FIXME !!! Gtk 2.3+ gives us gtk_message_dialog_set_markup() 
             * (and actually even 
             * gtk_message_dialog_new_with_markup(..., format, ...)!) -- 
             * until then, we will have to be a little bit evil here and 
             * poke at GtkMessageDialog::label, which is private... 
             */

            gtk_label_set_markup(GTK_LABEL(GTK_MESSAGE_DIALOG(dialog)->label), markup);
            g_free(markup);

            GtkWidget *close_button;
            close_button = gtk_button_new_with_mnemonic(_("Close _without saving"));
            gtk_widget_show(close_button);
            gtk_dialog_add_action_widget(GTK_DIALOG(dialog), close_button, GTK_RESPONSE_NO);

            gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
            gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_SAVE, GTK_RESPONSE_YES);
            gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);

            gint response;
            response = gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);

            switch (response) {
            case GTK_RESPONSE_YES:
                sp_document_ref(doc);
                if (sp_file_save_document(doc)) {
                    sp_document_unref(doc);
                } else { // save dialog cancelled or save failed
                    sp_document_unref(doc);
                    return TRUE;
                }
                break;
            case GTK_RESPONSE_NO:
                break;
            default: // cancel pressed, or dialog was closed
                return TRUE;
                break;
            }
        }
        /* Code to check data loss */
        bool allow_data_loss = FALSE;
        while (sp_document_repr_root(doc)->attribute("inkscape:dataloss") != NULL && allow_data_loss == FALSE) {
            GtkWidget *dialog;

            dialog = gtk_message_dialog_new(
                GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(this))),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_NONE,
                "Document modified");

            gchar *markup;
            /** \todo 
             * FIXME !!! obviously this will have problems if the document 
             * name contains markup characters
             */
            markup = g_strdup_printf(
                _("<span weight=\"bold\" size=\"larger\">The file \"%s\" was saved with a format (%s) that may cause data loss!</span>\n\n"
                  "Do you want to save this file in another format?"),
                SP_DOCUMENT_NAME(doc),
                Inkscape::Extension::db.get(sp_document_repr_root(doc)->attribute("inkscape:output_extension"))->get_name());

            /** \todo
             * FIXME !!! Gtk 2.3+ gives us gtk_message_dialog_set_markup() 
             * (and actually even 
             * gtk_message_dialog_new_with_markup(..., format, ...)!) -- 
             * until then, we will have to be a little bit evil here and 
             * poke at GtkMessageDialog::label, which is private... 
             */

            gtk_label_set_markup(GTK_LABEL(GTK_MESSAGE_DIALOG(dialog)->label), markup);
            g_free(markup);

            GtkWidget *close_button;
            close_button = gtk_button_new_with_mnemonic(_("Close _without saving"));
            gtk_widget_show(close_button);
            gtk_dialog_add_action_widget(GTK_DIALOG(dialog), close_button, GTK_RESPONSE_NO);

            gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
            gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_SAVE, GTK_RESPONSE_YES);
            gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);

            gint response;
            response = gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);

            switch (response) {
            case GTK_RESPONSE_YES:
                sp_document_ref(doc);
                if (sp_file_save_dialog(doc)) {
                    sp_document_unref(doc);
                } else { // save dialog cancelled or save failed
                    sp_document_unref(doc);
                    return TRUE;
                }
                break;
            case GTK_RESPONSE_NO:
                allow_data_loss = TRUE;
                break;
            default: // cancel pressed, or dialog was closed
                return TRUE;
                break;
            }
        }
    }

    return FALSE;
}

/**
 * \pre dtw->desktop->main != 0
 */
void 
sp_desktop_widget_queue_draw (SPDesktopWidget* dtw)
{
    gtk_widget_queue_draw (GTK_WIDGET (SP_CANVAS_ITEM (dtw->desktop->main)->canvas));
}

void 
sp_desktop_widget_set_coordinate_status (SPDesktopWidget *dtw, gchar *cstr)
{
    gtk_label_set_text (GTK_LABEL (dtw->coord_status), cstr);
}

void
sp_desktop_widget_fullscreen(SPDesktopWidget *dtw)
{
#ifdef HAVE_GTK_WINDOW_FULLSCREEN
    GtkWindow *topw = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(dtw->canvas)));
    if (GTK_IS_WINDOW(topw)) {
        if (dtw->desktop->is_fullscreen) {
            dtw->desktop->is_fullscreen = FALSE;
            gtk_window_unfullscreen(topw);
            sp_desktop_widget_layout (dtw);
        } else {
            dtw->desktop->is_fullscreen = TRUE;
            gtk_window_fullscreen(topw);
            sp_desktop_widget_layout (dtw);
        }
    }
#endif /* HAVE_GTK_WINDOW_FULLSCREEN */
}

/**
 * Hide whatever the user does not want to see in the window
 */
void
sp_desktop_widget_layout (SPDesktopWidget *dtw)
{
    bool fullscreen = dtw->desktop->is_fullscreen;

    if (prefs_get_int_attribute (fullscreen ? "fullscreen.menu" : "window.menu", "state", 1) == 0) {
        gtk_widget_hide_all (dtw->menubar);
    } else {
        gtk_widget_show_all (dtw->menubar);
    }

    if (prefs_get_int_attribute (fullscreen ? "fullscreen.commands" : "window.commands", "state", 1) == 0) {
        gtk_widget_hide_all (dtw->commands_toolbox);
    } else {
        gtk_widget_show_all (dtw->commands_toolbox);
    }

    if (prefs_get_int_attribute (fullscreen ? "fullscreen.toppanel" : "window.toppanel", "state", 1) == 0) {
        gtk_widget_hide_all (dtw->aux_toolbox);
    } else {
        // we cannot just show_all because that will show all tools' panels;
        // this is a function from toolbox.cpp that shows only the current tool's panel
        show_aux_toolbox (dtw->aux_toolbox);
    }

    if (prefs_get_int_attribute (fullscreen ? "fullscreen.toolbox" : "window.toolbox", "state", 1) == 0) {
        gtk_widget_hide_all (dtw->tool_toolbox);
    } else {
        gtk_widget_show_all (dtw->tool_toolbox);
    }

    if (prefs_get_int_attribute (fullscreen ? "fullscreen.statusbar" : "window.statusbar", "state", 1) == 0) {
        gtk_widget_hide_all (dtw->statusbar);
    } else {
        gtk_widget_show_all (dtw->statusbar);
    }

    if (prefs_get_int_attribute (fullscreen ? "fullscreen.scrollbars" : "window.scrollbars", "state", 1) == 0) {
        gtk_widget_hide_all (dtw->hscrollbar);
        gtk_widget_hide_all (dtw->vscrollbar_box);
    } else {
        gtk_widget_show_all (dtw->hscrollbar);
        gtk_widget_show_all (dtw->vscrollbar_box);
    }

    if (prefs_get_int_attribute (fullscreen ? "fullscreen.rulers" : "window.rulers", "state", 1) == 0) {
        gtk_widget_hide_all (dtw->hruler);
        gtk_widget_hide_all (dtw->vruler);
    } else {
        gtk_widget_show_all (dtw->hruler);
        gtk_widget_show_all (dtw->vruler);
    }
}

SPViewWidget *
sp_desktop_widget_new (SPNamedView *namedview)
{
    SPDesktopWidget *dtw = (SPDesktopWidget*)gtk_type_new (SP_TYPE_DESKTOP_WIDGET);

    dtw->dt2r = 1.0 / namedview->doc_units->unittobase;
    dtw->ruler_origin = namedview->gridorigin;

    dtw->desktop = new SPDesktop();
    dtw->desktop = Inkscape::GC::anchor (dtw->desktop);
    dtw->desktop->owner = dtw;
    dtw->desktop->init (namedview, dtw->canvas);

    /* Once desktop is set, we can update rulers */
    sp_desktop_widget_update_rulers (dtw);

    sp_view_widget_set_view (SP_VIEW_WIDGET (dtw), dtw->desktop);


    /* Listen on namedview modification */
    g_signal_connect (G_OBJECT (namedview), "modified", G_CALLBACK (sp_desktop_widget_namedview_modified), dtw);

    dtw->layer_selector->setDesktop(dtw->desktop);

    dtw->menubar = sp_ui_main_menubar (dtw->desktop);
    gtk_widget_show_all (dtw->menubar);
    gtk_box_pack_start (GTK_BOX (gtk_bin_get_child (GTK_BIN (dtw))), dtw->menubar, FALSE, FALSE, 0);

    sp_desktop_widget_layout (dtw);

    sp_tool_toolbox_set_desktop (dtw->tool_toolbox, dtw->desktop);
    sp_aux_toolbox_set_desktop (dtw->aux_toolbox, dtw->desktop);
    sp_commands_toolbox_set_desktop (dtw->commands_toolbox, dtw->desktop);

    return SP_VIEW_WIDGET (dtw);
}

void
SPDesktopWidget::viewSetPosition (double x, double y)
{
    using NR::X;
    using NR::Y;

    NR::Point const origin = ( NR::Point(x, y) - ruler_origin );

    /// \todo fixme:
    GTK_RULER(hruler)->position = origin[X];
    gtk_ruler_draw_pos (GTK_RULER (hruler));
    GTK_RULER(vruler)->position = origin[Y];
    gtk_ruler_draw_pos (GTK_RULER (vruler));
}

void
sp_desktop_widget_update_rulers (SPDesktopWidget *dtw)
{
    NRRect viewbox;
    sp_canvas_get_viewbox (dtw->canvas, &viewbox);
    double scale = dtw->desktop->current_zoom();
    double s = viewbox.x0 / scale - dtw->ruler_origin[NR::X];
    double e = viewbox.x1 / scale - dtw->ruler_origin[NR::X];
    gtk_ruler_set_range (GTK_RULER (dtw->hruler), s,  e, GTK_RULER (dtw->hruler)->position, (e - s));
    s = viewbox.y0 / -scale - dtw->ruler_origin[NR::Y];
    e = viewbox.y1 / -scale - dtw->ruler_origin[NR::Y];
    gtk_ruler_set_range (GTK_RULER (dtw->vruler), s, e, GTK_RULER (dtw->vruler)->position, (e - s));
}


static void
sp_desktop_widget_namedview_modified (SPNamedView *nv, guint flags, SPDesktopWidget *dtw)
{
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        dtw->dt2r = 1.0 / nv->doc_units->unittobase;

        dtw->ruler_origin = nv->gridorigin;

        sp_ruler_set_metric (GTK_RULER (dtw->vruler), dtw->desktop->get_default_metric());
        sp_ruler_set_metric (GTK_RULER (dtw->hruler), dtw->desktop->get_default_metric());

        gtk_tooltips_set_tip (dtw->tt, dtw->hruler_box, gettext(sp_unit_get_plural (dtw->desktop->get_default_unit())), NULL);
        gtk_tooltips_set_tip (dtw->tt, dtw->vruler_box, gettext(sp_unit_get_plural (dtw->desktop->get_default_unit())), NULL);

        sp_desktop_widget_update_rulers (dtw);
    }
}

static void
sp_desktop_widget_adjustment_value_changed (GtkAdjustment *adj, SPDesktopWidget *dtw)
{
    if (dtw->update)
        return;

    dtw->update = 1;

    sp_canvas_scroll_to (dtw->canvas, dtw->hadj->value, dtw->vadj->value, FALSE);
    sp_desktop_widget_update_rulers (dtw);

    dtw->update = 0;
}

/* we make the desktop window with focus active, signal is connected in interface.c */

gint
sp_desktop_widget_set_focus (GtkWidget *widget, GdkEvent *event, SPDesktopWidget *dtw)
{
    inkscape_activate_desktop (dtw->desktop);

    /* give focus to canvas widget */
    gtk_widget_grab_focus (GTK_WIDGET (dtw->canvas));

    return FALSE;
}

static gdouble
sp_dtw_zoom_value_to_display (gdouble value)
{
    return floor (pow (2, value) * 100.0 + 0.5);
}

static gdouble
sp_dtw_zoom_display_to_value (gdouble value)
{
    return  log (value / 100.0) / log (2);
}

static gint
sp_dtw_zoom_input (GtkSpinButton *spin, gdouble *new_val, gpointer data)
{
    gdouble new_scrolled = gtk_spin_button_get_value (spin);
    const gchar *b = gtk_entry_get_text (GTK_ENTRY (spin));
    gdouble new_typed = atof (b);

    if (sp_dtw_zoom_value_to_display (new_scrolled) == new_typed) { // the new value is set by scrolling
        *new_val = new_scrolled;
    } else { // the new value is typed in
        *new_val = sp_dtw_zoom_display_to_value (new_typed);
    }

    return TRUE;
}

static bool
sp_dtw_zoom_output (GtkSpinButton *spin, gpointer data)
{
    gchar b[64];
    g_snprintf (b, 64, "%4.0f%%", sp_dtw_zoom_value_to_display (gtk_spin_button_get_value (spin)));
    gtk_entry_set_text (GTK_ENTRY (spin), b);
    return TRUE;
}

static void
sp_dtw_zoom_value_changed (GtkSpinButton *spin, gpointer data)
{
    NRRect d;
    double zoom_factor = pow (2, gtk_spin_button_get_value (spin));

    SPDesktopWidget *dtw = SP_DESKTOP_WIDGET (data);
    SPDesktop *desktop = dtw->desktop;

    desktop->get_display_area (&d);
    g_signal_handler_block (spin, dtw->zoom_update);
    desktop->zoom_absolute ((d.x0 + d.x1) / 2, (d.y0 + d.y1) / 2, zoom_factor);
    g_signal_handler_unblock (spin, dtw->zoom_update);

    spinbutton_defocus (GTK_OBJECT (spin));
}

static void
sp_dtw_zoom_populate_popup (GtkEntry *entry, GtkMenu *menu, gpointer data)
{
    GList *children, *iter;
    GtkWidget *item;
    SPDesktop *dt = SP_DESKTOP_WIDGET (data)->desktop;

    children = gtk_container_get_children (GTK_CONTAINER (menu));
    for ( iter = children ; iter ; iter = g_list_next (iter)) {
        gtk_container_remove (GTK_CONTAINER (menu), GTK_WIDGET (iter->data));
    }
    g_list_free (children);

    item = gtk_menu_item_new_with_label ("200%");
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_200), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    item = gtk_menu_item_new_with_label ("100%");
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_100), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    item = gtk_menu_item_new_with_label ("50%");
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_50), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_separator_menu_item_new ();
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_menu_item_new_with_label (_("Page"));
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_page), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    item = gtk_menu_item_new_with_label (_("Drawing"));
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_drawing), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    item = gtk_menu_item_new_with_label (_("Selection"));
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_selection), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void
sp_dtw_zoom_menu_handler (SPDesktop *dt, gdouble factor)
{
    NRRect d;

    dt->get_display_area (&d);
    dt->zoom_absolute (( d.x0 + d.x1 ) / 2, ( d.y0 + d.y1 ) / 2, factor);
}

static void
sp_dtw_zoom_50 (GtkMenuItem *item, gpointer data)
{
    sp_dtw_zoom_menu_handler (static_cast<SPDesktop*>(data), 0.5);
}

static void
sp_dtw_zoom_100 (GtkMenuItem *item, gpointer data)
{
    sp_dtw_zoom_menu_handler (static_cast<SPDesktop*>(data), 1.0);
}

static void
sp_dtw_zoom_200 (GtkMenuItem *item, gpointer data)
{
    sp_dtw_zoom_menu_handler (static_cast<SPDesktop*>(data), 2.0);
}

static void
sp_dtw_zoom_page (GtkMenuItem *item, gpointer data)
{
    static_cast<SPDesktop*>(data)->zoom_page();
}

static void
sp_dtw_zoom_drawing (GtkMenuItem *item, gpointer data)
{
    static_cast<SPDesktop*>(data)->zoom_drawing();
}

static void
sp_dtw_zoom_selection (GtkMenuItem *item, gpointer data)
{
    static_cast<SPDesktop*>(data)->zoom_selection();
}



void
sp_desktop_widget_update_zoom (SPDesktopWidget *dtw)
{
    g_signal_handlers_block_by_func (G_OBJECT (dtw->zoom_status), (gpointer)G_CALLBACK (sp_dtw_zoom_value_changed), dtw);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (dtw->zoom_status), log(dtw->desktop->current_zoom()) / log(2));
    g_signal_handlers_unblock_by_func (G_OBJECT (dtw->zoom_status), (gpointer)G_CALLBACK (sp_dtw_zoom_value_changed), dtw);
}

void
sp_desktop_widget_toggle_rulers (SPDesktopWidget *dtw, bool is_fullscreen)
{
    if (GTK_WIDGET_VISIBLE (dtw->hruler)) {
        gtk_widget_hide_all (dtw->hruler);
        gtk_widget_hide_all (dtw->vruler);
        prefs_set_int_attribute (is_fullscreen ? "fullscreen.rulers" : "window.rulers", "state", 0);
    } else {
        gtk_widget_show_all (dtw->hruler);
        gtk_widget_show_all (dtw->vruler);
        prefs_set_int_attribute (is_fullscreen ? "fullscreen.rulers" : "window.rulers", "state", 1);
    }
}

void
sp_desktop_widget_toggle_scrollbars (SPDesktopWidget *dtw, bool is_fullscreen)
{
    if (GTK_WIDGET_VISIBLE (dtw->hscrollbar)) {
        gtk_widget_hide_all (dtw->hscrollbar);
        gtk_widget_hide_all (dtw->vscrollbar_box);
        prefs_set_int_attribute (is_fullscreen ? "fullscreen.scrollbars" : "window.scrollbars", "state", 0);
    } else {
        gtk_widget_show_all (dtw->hscrollbar);
        gtk_widget_show_all (dtw->vscrollbar_box);
        prefs_set_int_attribute (is_fullscreen ? "fullscreen.scrollbars" : "window.scrollbars", "state", 1);
    }
}

/* Unused
void
sp_spw_toggle_menubar (SPDesktopWidget *dtw, bool is_fullscreen)
{
    if (GTK_WIDGET_VISIBLE (dtw->menubar)) {
        gtk_widget_hide_all (dtw->menubar);
        prefs_set_int_attribute (is_fullscreen ? "fullscreen.menu" : "window.menu", "state", 0);
    } else {
        gtk_widget_show_all (dtw->menubar);
        prefs_set_int_attribute (is_fullscreen ? "fullscreen.menu" : "window.menu", "state", 1);
    }
}
*/

static void
set_adjustment (GtkAdjustment *adj, double l, double u, double ps, double si, double pi)
{
    if ((l != adj->lower) ||
        (u != adj->upper) ||
        (ps != adj->page_size) ||
        (si != adj->step_increment) ||
        (pi != adj->page_increment)) {
        adj->lower = l;
        adj->upper = u;
        adj->page_size = ps;
        adj->step_increment = si;
        adj->page_increment = pi;
        gtk_adjustment_changed (adj);
    }
}

void
sp_desktop_widget_update_scrollbars (SPDesktopWidget *dtw, SPDocument *doc, double scale)
{
    if (!dtw) return;
    if (dtw->update) return;
    dtw->update = 1;

    /* The desktop region we always show unconditionally */
    NRRect darea;
    sp_item_bbox_desktop (SP_ITEM (SP_DOCUMENT_ROOT (doc)), &darea);
    darea.x0 = MIN (darea.x0, -sp_document_width (doc));
    darea.y0 = MIN (darea.y0, -sp_document_height (doc));
    darea.x1 = MAX (darea.x1, 2 * sp_document_width (doc));
    darea.y1 = MAX (darea.y1, 2 * sp_document_height (doc));

    /* Canvas region we always show unconditionally */
    NRRect carea;
    carea.x0 = darea.x0 * scale - 64;
    carea.y0 = darea.y1 * -scale - 64;
    carea.x1 = darea.x1 * scale + 64;
    carea.y1 = darea.y0 * -scale + 64;

    NRRect viewbox;
    sp_canvas_get_viewbox (dtw->canvas, &viewbox);

    /* Viewbox is always included into scrollable region */
    nr_rect_d_union (&carea, &carea, &viewbox);

    set_adjustment (dtw->hadj, carea.x0, carea.x1,
                    (viewbox.x1 - viewbox.x0),
                    0.1 * (viewbox.x1 - viewbox.x0),
                    (viewbox.x1 - viewbox.x0));
    gtk_adjustment_set_value (dtw->hadj, viewbox.x0);

    set_adjustment (dtw->vadj, carea.y0, carea.y1,
                    (viewbox.y1 - viewbox.y0),
                    0.1 * (viewbox.y1 - viewbox.y0),
                    (viewbox.y1 - viewbox.y0));
    gtk_adjustment_set_value (dtw->vadj, viewbox.y0);

    dtw->update = 0;
}


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
