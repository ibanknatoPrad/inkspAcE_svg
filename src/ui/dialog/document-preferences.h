/** \file
 * \brief  Document Preferences dialog
 *
 * Authors:
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *
 * Copyright (C) 2004, 2005 Authors
 *
 * Released under GNU GPL.  Read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_DOCUMENT_PREFERENCES_H
#define INKSCAPE_UI_DIALOG_DOCUMENT_PREFERENCES_H

#include <list>
#include <gtkmm/notebook.h>
#include <glibmm/i18n.h>

#include "ui/widget/licensor.h"
#include "ui/widget/notebook-page.h"
#include "ui/widget/page-sizer.h"
#include "ui/widget/registered-widget.h"
#include "ui/widget/registry.h"
#include "dialog.h"

using namespace Inkscape::UI::Widget;

namespace Inkscape {
    namespace UI {
        namespace Widget {
            class EntityEntry;
        }
        namespace Dialog {

typedef std::list<EntityEntry*> RDElist;

class DocumentPreferences : public Inkscape::UI::Dialog::Dialog {
public:
    static DocumentPreferences *create();
    static void destroy();
    static bool isUpdating();
    static void setUpdating (bool);
    static void addKey (const Glib::ustring&, Gtk::Object*);

protected:
    void  build_page();
    void  build_grid();
    void  build_guides();
    void  build_metadata();
    void  init();
    void  update();

    Gtk::Tooltips _tt;
    Gtk::Notebook  _notebook;

    NotebookPage   _page_page, _page_grid, _page_guides;
    NotebookPage   _page_metadata1, _page_metadata2;

    RegisteredCheckButton _rcb_canb, _rcb_bord, _rcb_shad;
    RegisteredColorPicker _rcp_bg, _rcp_bord;
    RegisteredUnitMenu    _rum_deflt;
    PageSizer             _page_sizer;
    //---------------------------------------------------------------
    RegisteredCheckButton _rcbgrid, _rcbsnbb, _rcbsnnod;
    RegisteredUnitMenu    _rumg, _rums;
    RegisteredScalarUnit  _rsu_ox, _rsu_oy, _rsu_sx, _rsu_sy, _rsu_sn;
    RegisteredColorPicker _rcp_gcol, _rcp_gmcol;
    RegisteredSuffixedInteger _rsi;
    //---------------------------------------------------------------
    RegisteredCheckButton _rcb_sgui, _rcb_snpgui, _rcb_snbgui;
    RegisteredUnitMenu    _rum_gusn;
    RegisteredScalarUnit  _rsu_gusn;
    RegisteredColorPicker _rcp_gui, _rcp_hgui;
    //---------------------------------------------------------------
    RDElist _rdflist;
    Licensor _licensor;

    gchar * _prefs_path;
    Registry _wr;

private:
    DocumentPreferences();
    virtual ~DocumentPreferences();
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_DIALOG_DOCUMENT_PREFERENCES_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
