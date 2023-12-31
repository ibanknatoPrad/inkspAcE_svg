// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main UI stuff.
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2012 Kris De Gussem
 * Copyright (C) 2010 authors
 * Copyright (C) 1999-2005 authors
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm/i18n.h>

#include "desktop.h"
#include "document.h"
#include "enums.h"
#include "file.h"
#include "inkscape.h"
#include "inkscape-window.h"
#include "preferences.h"
#include "shortcuts.h"

#include "io/sys.h"

#include "object/sp-namedview.h"
#include "object/sp-root.h"

#include "ui/dialog-events.h"
#include "ui/dialog/inkscape-preferences.h"
#include "ui/dialog/layer-properties.h"
#include "ui/interface.h"

#include "ui/view/svg-view-widget.h"

#include "widgets/desktop-widget.h"

static void sp_ui_import_one_file(char const *filename);
static void sp_ui_import_one_file_with_check(gpointer filename, gpointer unused);

void
sp_ui_new_view()
{
    SPDocument *document;

    document = SP_ACTIVE_DOCUMENT;
    if (!document) return;

    auto *app = InkscapeApplication::instance();

    app->window_open(document);
}

void
sp_ui_close_view(GtkWidget */*widget*/)
{
    auto *app = InkscapeApplication::instance();

    auto window = app->get_active_window();
    assert(window);
    app->destroy_window(window, true); // Keep inkscape alive!
}


Glib::ustring getLayoutPrefPath( Inkscape::UI::View::View *view )
{
    Glib::ustring prefPath;

    if (reinterpret_cast<SPDesktop*>(view)->is_focusMode()) {
        prefPath = "/focus/";
    } else if (reinterpret_cast<SPDesktop*>(view)->is_fullscreen()) {
        prefPath = "/fullscreen/";
    } else {
        prefPath = "/window/";
    }

    return prefPath;
}


void
sp_ui_import_files(gchar *buffer)
{
    gchar** l = g_uri_list_extract_uris(buffer);
    for (unsigned int i=0; i < g_strv_length(l); i++) {
        gchar *f = g_filename_from_uri (l[i], nullptr, nullptr);
        sp_ui_import_one_file_with_check(f, nullptr);
        g_free(f);
    }
    g_strfreev(l);
}

static void
sp_ui_import_one_file_with_check(gpointer filename, gpointer /*unused*/)
{
    if (filename) {
        if (strlen((char const *)filename) > 2)
            sp_ui_import_one_file((char const *)filename);
    }
}

static void
sp_ui_import_one_file(char const *filename)
{
    SPDocument *doc = SP_ACTIVE_DOCUMENT;
    if (!doc) return;

    if (filename == nullptr) return;

    // Pass off to common implementation
    // TODO might need to get the proper type of Inkscape::Extension::Extension
    file_import( doc, filename, nullptr );
}

void
sp_ui_error_dialog(gchar const *message)
{
    GtkWidget *dlg;
    gchar *safeMsg = Inkscape::IO::sanitizeString(message);

    dlg = gtk_message_dialog_new(nullptr, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
                                 GTK_BUTTONS_CLOSE, "%s", safeMsg);
    sp_transientize(dlg);
    gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dlg),safeMsg);
    gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    g_free(safeMsg);
}

bool
sp_ui_overwrite_file(gchar const *filename)
{
    bool return_value = FALSE;

    if (Inkscape::IO::file_test(filename, G_FILE_TEST_EXISTS)) {
        Gtk::Window *window = SP_ACTIVE_DESKTOP->getToplevel();
        gchar* baseName = g_path_get_basename( filename );
        gchar* dirName = g_path_get_dirname( filename );
        GtkWidget* dialog = gtk_message_dialog_new_with_markup( window->gobj(),
                                                                (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                                GTK_MESSAGE_QUESTION,
                                                                GTK_BUTTONS_NONE,
                                                                _( "<span weight=\"bold\" size=\"larger\">A file named \"%s\" already exists. Do you want to replace it?</span>\n\n"
                                                                   "The file already exists in \"%s\". Replacing it will overwrite its contents." ),
                                                                baseName,
                                                                dirName
            );
        gtk_dialog_add_buttons( GTK_DIALOG(dialog),
                                _("_Cancel"), GTK_RESPONSE_NO,
                                _("Replace"), GTK_RESPONSE_YES,
                                nullptr );
        gtk_dialog_set_default_response( GTK_DIALOG(dialog), GTK_RESPONSE_YES );

        if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_YES ) {
            return_value = TRUE;
        } else {
            return_value = FALSE;
        }
        gtk_widget_destroy(dialog);
        g_free( baseName );
        g_free( dirName );
    } else {
        return_value = TRUE;
    }

    return return_value;
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
