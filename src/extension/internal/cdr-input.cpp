// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  This file came from libwpg as a source, their utility wpg2svg
 *  specifically.  It has been modified to work as an Inkscape extension.
 *  The Inkscape extension code is covered by this copyright, but the
 *  rest is covered by the one below.
 *
 * Authors:
 *   Fridrich Strba (fridrich.strba@bluewin.ch)
 *
 * Copyright (C) 2012 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include <cstdio>

#include "cdr-input.h"

#ifdef WITH_LIBCDR

#include <string>
#include <cstring>

#include <libcdr/libcdr.h>

#include <librevenge-stream/librevenge-stream.h>

using librevenge::RVNGString;
using librevenge::RVNGFileStream;
using librevenge::RVNGStringVector;

#include <gtkmm/grid.h>
#include <gtkmm/spinbutton.h>

#include "extension/system.h"
#include "extension/input.h"

#include "document.h"
#include "inkscape.h"

#include "ui/dialog-events.h"
#include "ui/dialog-run.h"
#include <glibmm/i18n.h>

#include "ui/view/svg-view-widget.h"

#include "object/sp-root.h"

#include "util/units.h"

namespace Inkscape {
namespace Extension {
namespace Internal {


class CdrImportDialog : public Gtk::Dialog {
public:
     CdrImportDialog(const std::vector<RVNGString> &vec);
     ~CdrImportDialog() override;

     bool showDialog();
     unsigned getSelectedPage();
     void getImportSettings(Inkscape::XML::Node *prefs);

private:
     void _setPreviewPage();

     // Signal handlers
     void _onPageNumberChanged();
     void _onSpinButtonPress(GdkEventButton* button_event);
     void _onSpinButtonRelease(GdkEventButton* button_event);

     class Gtk::Box * vbox1;
     class Inkscape::UI::View::SVGViewWidget * _previewArea;
     class Gtk::Button * cancelbutton;
     class Gtk::Button * okbutton;

     class Gtk::Box  * _page_selector_box;
     class Gtk::Label * _labelSelect;
     class Gtk::Label * _labelTotalPages;
     class Gtk::SpinButton * _pageNumberSpin;

     const std::vector<RVNGString> &_vec;  // Document to be imported
     unsigned _current_page;               // Current selected page
     bool _spinning;                       // whether SpinButton is pressed (i.e. we're "spinning")
};

CdrImportDialog::CdrImportDialog(const std::vector<RVNGString> &vec)
    : _previewArea(nullptr)
    , _vec(vec)
    , _current_page(1)
    , _spinning(false)
{
     int num_pages = _vec.size();
     if ( num_pages <= 1 )
          return;

     // Dialog settings
     this->set_title(_("Page Selector"));
     this->set_modal(true);
     sp_transientize(GTK_WIDGET(this->gobj()));  //Make transient
     this->property_window_position().set_value(Gtk::WIN_POS_NONE);
     this->set_resizable(true);
     this->property_destroy_with_parent().set_value(false);

     // Preview area
     vbox1 = Gtk::manage(new class Gtk::Box(Gtk::ORIENTATION_VERTICAL));
     this->get_content_area()->pack_start(*vbox1);

     // CONTROLS
     _page_selector_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));

     // "Select page:" label
     _labelSelect = Gtk::manage(new class Gtk::Label(_("Select page:")));
     _labelTotalPages = Gtk::manage(new class Gtk::Label());
     _labelSelect->set_line_wrap(false);
     _labelSelect->set_use_markup(false);
     _labelSelect->set_selectable(false);
     _page_selector_box->pack_start(*_labelSelect, Gtk::PACK_SHRINK);

     // Adjustment + spinner
     auto pageNumberSpin_adj = Gtk::Adjustment::create(1, 1, _vec.size(), 1, 10, 0);
     _pageNumberSpin = Gtk::manage(new Gtk::SpinButton(pageNumberSpin_adj, 1, 0));
     _pageNumberSpin->set_can_focus();
     _pageNumberSpin->set_update_policy(Gtk::UPDATE_ALWAYS);
     _pageNumberSpin->set_numeric(true);
     _pageNumberSpin->set_wrap(false);
     _page_selector_box->pack_start(*_pageNumberSpin, Gtk::PACK_SHRINK);

     _labelTotalPages->set_line_wrap(false);
     _labelTotalPages->set_use_markup(false);
     _labelTotalPages->set_selectable(false);
     gchar *label_text = g_strdup_printf(_("out of %i"), num_pages);
     _labelTotalPages->set_label(label_text);
     g_free(label_text);
     _page_selector_box->pack_start(*_labelTotalPages, Gtk::PACK_SHRINK);

     vbox1->pack_end(*_page_selector_box, Gtk::PACK_SHRINK);

     // Buttons
     cancelbutton = Gtk::manage(new Gtk::Button(_("_Cancel"), true));
     okbutton     = Gtk::manage(new Gtk::Button(_("_OK"),     true));
     this->add_action_widget(*cancelbutton, Gtk::RESPONSE_CANCEL);
     this->add_action_widget(*okbutton, Gtk::RESPONSE_OK);

     // Show all widgets in dialog
     this->show_all();

     // Connect signals
     _pageNumberSpin->signal_value_changed().connect(sigc::mem_fun(*this, &CdrImportDialog::_onPageNumberChanged));
     _pageNumberSpin->signal_button_press_event().connect_notify(sigc::mem_fun(*this, &CdrImportDialog::_onSpinButtonPress));
     _pageNumberSpin->signal_button_release_event().connect_notify(sigc::mem_fun(*this, &CdrImportDialog::_onSpinButtonRelease));

     _setPreviewPage();
}

CdrImportDialog::~CdrImportDialog() = default;

bool CdrImportDialog::showDialog()
{
     gint b = UI::dialog_run(*this);
     if (b == Gtk::RESPONSE_OK || b == Gtk::RESPONSE_ACCEPT) {
          return TRUE;
     } else {
          return FALSE;
     }
}

unsigned CdrImportDialog::getSelectedPage()
{
     return _current_page;
}

void CdrImportDialog::_onPageNumberChanged()
{
     unsigned page = static_cast<unsigned>(_pageNumberSpin->get_value_as_int());
     _current_page = CLAMP(page, 1U, _vec.size());
     _setPreviewPage();
}

void CdrImportDialog::_onSpinButtonPress(GdkEventButton* /*button_event*/)
{
     _spinning = true;
}

void CdrImportDialog::_onSpinButtonRelease(GdkEventButton* /*button_event*/)
{
     _spinning = false;
     _setPreviewPage();
}

/**
 * \brief Renders the given page's thumbnail
 */
void CdrImportDialog::_setPreviewPage()
{
    if (_spinning) {
        return;
    }

    SPDocument *doc = SPDocument::createNewDocFromMem(_vec[_current_page-1].cstr(), strlen(_vec[_current_page-1].cstr()), false);
    if(!doc) {
        g_warning("CDR import: Could not create preview for page %d", _current_page);
        gchar const *no_preview_template = R"A(
          <svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'>
            <path d='M 82,10 18,74 m 0,-64 64,64' style='fill:none;stroke:#ff0000;stroke-width:2px;'/>
            <rect x='18' y='10' width='64' height='64' style='fill:none;stroke:#000000;stroke-width:1.5px;'/>
            <text x='50' y='92' style='font-size:10px;text-anchor:middle;font-family:sans-serif;'>%s</text>
          </svg>
        )A";
        gchar * no_preview = g_strdup_printf(no_preview_template, _("No preview"));
        doc = SPDocument::createNewDocFromMem(no_preview, strlen(no_preview), false);
        g_free(no_preview);
    }

    if (!doc) {
        std::cerr << "CdrImportDialog::_setPreviewPage: No document!" << std::endl;
        return;
    }

    if (_previewArea) {
        _previewArea->setDocument(doc);
    } else {
        _previewArea = Gtk::manage(new Inkscape::UI::View::SVGViewWidget(doc));
        vbox1->pack_start(*_previewArea, Gtk::PACK_EXPAND_WIDGET, 0);
    }

    _previewArea->setResize(400, 400);
    _previewArea->show_all();
}

SPDocument *CdrInput::open(Inkscape::Extension::Input * /*mod*/, const gchar * uri)
{
     #ifdef _WIN32
          // RVNGFileStream uses fopen() internally which unfortunately only uses ANSI encoding on Windows
          // therefore attempt to convert uri to the system codepage
          // even if this is not possible the alternate short (8.3) file name will be used if available
          gchar * converted_uri = g_win32_locale_filename_from_utf8(uri);
          RVNGFileStream input(converted_uri);
          g_free(converted_uri);
     #else
          RVNGFileStream input(uri);
     #endif

     if (!libcdr::CDRDocument::isSupported(&input)) {
          return nullptr;
     }

     RVNGStringVector output;
     librevenge::RVNGSVGDrawingGenerator generator(output, "svg");

     if (!libcdr::CDRDocument::parse(&input, &generator)) {
          return nullptr;
     }

     if (output.empty()) {
          return nullptr;
     }

     std::vector<RVNGString> tmpSVGOutput;
     for (unsigned i=0; i<output.size(); ++i) {
          RVNGString tmpString("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
          tmpString.append(output[i]);
          tmpSVGOutput.push_back(tmpString);
     }

     unsigned page_num = 1;

     // If only one page is present, import that one without bothering user
     if (tmpSVGOutput.size() > 1) {
          CdrImportDialog *dlg = nullptr;
          if (INKSCAPE.use_gui()) {
               dlg = new CdrImportDialog(tmpSVGOutput);
               if (!dlg->showDialog()) {
                    delete dlg;
                    throw Input::open_cancelled();
               }
          }

          // Get needed page
          if (dlg) {
               page_num = dlg->getSelectedPage();
               if (page_num < 1)
                    page_num = 1;
               if (page_num > tmpSVGOutput.size())
                    page_num = tmpSVGOutput.size();
          }
     }

     SPDocument * doc = SPDocument::createNewDocFromMem(tmpSVGOutput[page_num-1].cstr(), strlen(tmpSVGOutput[page_num-1].cstr()), TRUE);

     if (doc && !doc->getRoot()->viewBox_set) {
          // Scales the document to account for 72dpi scaling in librevenge(<=0.0.4)
          doc->setWidth(Inkscape::Util::Quantity(doc->getWidth().quantity, "pt"), false);
          doc->setHeight(Inkscape::Util::Quantity(doc->getHeight().quantity, "pt"), false);
          doc->setViewBox(Geom::Rect::from_xywh(0, 0, doc->getWidth().value("pt"), doc->getHeight().value("pt")));
     }
     return doc;
}

#include "clear-n_.h"

void CdrInput::init()
{
    // clang-format off
    /* CDR */
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("Corel DRAW Input") "</name>\n"
            "<id>org.inkscape.input.cdr</id>\n"
            "<input>\n"
                "<extension>.cdr</extension>\n"
                "<mimetype>image/x-xcdr</mimetype>\n"
                "<filetypename>" N_("Corel DRAW 7-X4 files (*.cdr)") "</filetypename>\n"
                "<filetypetooltip>" N_("Open files saved in Corel DRAW 7-X4") "</filetypetooltip>\n"
            "</input>\n"
        "</inkscape-extension>", new CdrInput());

    /* CDT */
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("Corel DRAW templates input") "</name>\n"
            "<id>org.inkscape.input.cdt</id>\n"
            "<input>\n"
                "<extension>.cdt</extension>\n"
                "<mimetype>application/x-xcdt</mimetype>\n"
                "<filetypename>" N_("Corel DRAW 7-13 template files (*.cdt)") "</filetypename>\n"
                "<filetypetooltip>" N_("Open files saved in Corel DRAW 7-13") "</filetypetooltip>\n"
            "</input>\n"
        "</inkscape-extension>", new CdrInput());

    /* CCX */
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("Corel DRAW Compressed Exchange files input") "</name>\n"
            "<id>org.inkscape.input.ccx</id>\n"
            "<input>\n"
                "<extension>.ccx</extension>\n"
                "<mimetype>application/x-xccx</mimetype>\n"
                "<filetypename>" N_("Corel DRAW Compressed Exchange files (*.ccx)") "</filetypename>\n"
                "<filetypetooltip>" N_("Open compressed exchange files saved in Corel DRAW") "</filetypetooltip>\n"
            "</input>\n"
        "</inkscape-extension>", new CdrInput());

    /* CMX */
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("Corel DRAW Presentation Exchange files input") "</name>\n"
            "<id>org.inkscape.input.cmx</id>\n"
            "<input>\n"
                "<extension>.cmx</extension>\n"
                "<mimetype>application/x-xcmx</mimetype>\n"
                "<filetypename>" N_("Corel DRAW Presentation Exchange files (*.cmx)") "</filetypename>\n"
                "<filetypetooltip>" N_("Open presentation exchange files saved in Corel DRAW") "</filetypetooltip>\n"
            "</input>\n"
        "</inkscape-extension>", new CdrInput());
    // clang-format on

     return;

} // init

} } }  /* namespace Inkscape, Extension, Implementation */
#endif /* WITH_LIBCDR */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
