// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief A dialog for CSS selectors
 */
/* Authors:
 *   Kamalpreet Kaur Grewal
 *   Tavmjong Bah
 *   Jabiertxof
 *
 * Copyright (C) Kamalpreet Kaur Grewal 2016 <grewalkamal005@gmail.com>
 * Copyright (C) Tavmjong Bah 2017 <tavmjong@free.fr>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "selectorsdialog.h"

#include <map>
#include <regex>
#include <utility>

#include <glibmm/i18n.h>
#include <glibmm/regex.h>

#include "attribute-rel-svg.h"
#include "document-undo.h"
#include "inkscape.h"
#include "selection.h"
#include "style.h"

#include "ui/dialog-run.h"
#include "ui/icon-loader.h"
#include "ui/icon-names.h"
#include "ui/widget/iconrenderer.h"

#include "util/trim.h"

#include "xml/attribute-record.h"
#include "xml/node-observer.h"
#include "xml/sp-css-attr.h"


// G_MESSAGES_DEBUG=DEBUG_SELECTORSDIALOG  gdb ./inkscape
// #define DEBUG_SELECTORSDIALOG
// #define G_LOG_DOMAIN "SELECTORSDIALOG"

using Inkscape::DocumentUndo;

namespace Inkscape {
namespace UI {
namespace Dialog {

// Keeps a watch on style element
class SelectorsDialog::NodeObserver : public Inkscape::XML::NodeObserver
{
public:
    NodeObserver(SelectorsDialog *selectorsdialog)
        : _selectorsdialog(selectorsdialog)
    {
        g_debug("SelectorsDialog::NodeObserver: Constructor");
    };

    void notifyContentChanged(Inkscape::XML::Node &node,
                              Inkscape::Util::ptr_shared old_content,
                              Inkscape::Util::ptr_shared new_content) override;

    SelectorsDialog *_selectorsdialog;
};

void SelectorsDialog::NodeObserver::notifyContentChanged(Inkscape::XML::Node &,
                                                         Inkscape::Util::ptr_shared,
                                                         Inkscape::Util::ptr_shared)
{
    g_debug("SelectorsDialog::NodeObserver::notifyContentChanged");
    _selectorsdialog->_scrollock = true;
    _selectorsdialog->_updating = false;
    _selectorsdialog->_readStyleElement();
    _selectorsdialog->_selectRow();
}

// Keeps a watch for new/removed/changed nodes
// (Must update objects that selectors match.)
class SelectorsDialog::NodeWatcher : public Inkscape::XML::NodeObserver
{
public:
    NodeWatcher(SelectorsDialog *selectorsdialog)
        : _selectorsdialog(selectorsdialog)
    {
        g_debug("SelectorsDialog::NodeWatcher: Constructor");
    };

    void notifyChildAdded(Inkscape::XML::Node &,
                          Inkscape::XML::Node &child,
                          Inkscape::XML::Node *) override
    {
        _selectorsdialog->_nodeAdded(child);
    }

    void notifyChildRemoved(Inkscape::XML::Node &,
                            Inkscape::XML::Node &child,
                            Inkscape::XML::Node *) override
    {
        _selectorsdialog->_nodeRemoved(child);
    }

    void notifyAttributeChanged(Inkscape::XML::Node &node,
                                GQuark qname,
                                Util::ptr_shared,
                                Util::ptr_shared) override
    {
        static GQuark const CODE_id = g_quark_from_static_string("id");
        static GQuark const CODE_class = g_quark_from_static_string("class");

        if (qname == CODE_id || qname == CODE_class) {
            _selectorsdialog->_nodeChanged(node);
        }
    }

    SelectorsDialog *_selectorsdialog;
};

void SelectorsDialog::_nodeAdded(Inkscape::XML::Node &node)
{
    _readStyleElement();
    _selectRow();
}

void SelectorsDialog::_nodeRemoved(Inkscape::XML::Node &repr)
{
    if (_textNode == &repr) {
        _textNode = nullptr;
    }

    _readStyleElement();
    _selectRow();
}

void SelectorsDialog::_nodeChanged(Inkscape::XML::Node &object)
{
    g_debug("SelectorsDialog::NodeChanged");

    _scrollock = true;

    _readStyleElement();
    _selectRow();
}

SelectorsDialog::TreeStore::TreeStore() = default;

/**
 * Allow dragging only selectors.
 */
bool SelectorsDialog::TreeStore::row_draggable_vfunc(const Gtk::TreeModel::Path &path) const
{
    g_debug("SelectorsDialog::TreeStore::row_draggable_vfunc");

    auto unconstThis = const_cast<SelectorsDialog::TreeStore *>(this);
    const_iterator iter = unconstThis->get_iter(path);
    if (iter) {
        Gtk::TreeModel::Row row = *iter;
        bool is_draggable = row[_selectorsdialog->_mColumns._colType] == SELECTOR;
        return is_draggable;
    }
    return Gtk::TreeStore::row_draggable_vfunc(path);
}

/**
 * Allow dropping only in between other selectors.
 */
bool SelectorsDialog::TreeStore::row_drop_possible_vfunc(const Gtk::TreeModel::Path &dest,
                                                         const Gtk::SelectionData &selection_data) const
{
    g_debug("SelectorsDialog::TreeStore::row_drop_possible_vfunc");

    Gtk::TreeModel::Path dest_parent = dest;
    dest_parent.up();
    return dest_parent.empty();
}


// This is only here to handle updating style element after a drag and drop.
void SelectorsDialog::TreeStore::on_row_deleted(const TreeModel::Path &path)
{
    if (_selectorsdialog->_updating)
        return; // Don't write if we deleted row (other than from DND)

    g_debug("on_row_deleted");
    _selectorsdialog->_writeStyleElement();
    _selectorsdialog->_readStyleElement();
}


Glib::RefPtr<SelectorsDialog::TreeStore> SelectorsDialog::TreeStore::create(SelectorsDialog *selectorsdialog)
{
    g_debug("SelectorsDialog::TreeStore::create");

    SelectorsDialog::TreeStore *store = new SelectorsDialog::TreeStore();
    store->_selectorsdialog = selectorsdialog;
    store->set_column_types(store->_selectorsdialog->_mColumns);
    return Glib::RefPtr<SelectorsDialog::TreeStore>(store);
}

/**
 * Constructor
 * A treeview and a set of two buttons are added to the dialog. _addSelector
 * adds selectors to treeview. _delSelector deletes the selector from the dialog.
 * Any addition/deletion of the selectors updates XML style element accordingly.
 */
SelectorsDialog::SelectorsDialog()
    : DialogBase("/dialogs/selectors", "Selectors")
{
    g_debug("SelectorsDialog::SelectorsDialog");

    m_nodewatcher = std::make_unique<NodeWatcher>(this);
    m_styletextwatcher = std::make_unique<NodeObserver>(this);

    // Tree
    Inkscape::UI::Widget::IconRenderer * addRenderer = manage(
                new Inkscape::UI::Widget::IconRenderer() );
    addRenderer->add_icon("edit-delete");
    addRenderer->add_icon("list-add");
    addRenderer->add_icon("empty-icon");
    _store = TreeStore::create(this);
    _treeView.set_model(_store);

    // ALWAYS be a single selection widget
    _treeView.get_selection()->set_mode(Gtk::SELECTION_SINGLE);

    _treeView.set_headers_visible(false);
    _treeView.enable_model_drag_source();
    _treeView.enable_model_drag_dest( Gdk::ACTION_MOVE );
    int addCol = _treeView.append_column("", *addRenderer) - 1;
    Gtk::TreeViewColumn *col = _treeView.get_column(addCol);
    if ( col ) {
        col->add_attribute(addRenderer->property_icon(), _mColumns._colType);
    }

    Gtk::CellRendererText *label = Gtk::manage(new Gtk::CellRendererText());
    addCol = _treeView.append_column("CSS Selector", *label) - 1;
    col = _treeView.get_column(addCol);
    if (col) {
        col->add_attribute(label->property_text(), _mColumns._colSelector);
        col->add_attribute(label->property_weight(), _mColumns._colSelected);
    }
    _treeView.set_expander_column(*(_treeView.get_column(1)));


    // Signal handlers
    _treeView.signal_button_release_event().connect( // Needs to be release, not press.
        sigc::mem_fun(*this, &SelectorsDialog::_handleButtonEvent), false);

    _treeView.signal_button_release_event().connect_notify(
        sigc::mem_fun(*this, &SelectorsDialog::_buttonEventsSelectObjs), false);

    _treeView.signal_row_expanded().connect(sigc::mem_fun(*this, &SelectorsDialog::_rowExpand));

    _treeView.signal_row_collapsed().connect(sigc::mem_fun(*this, &SelectorsDialog::_rowCollapse));

    _showWidgets();

    show_all();
}


void SelectorsDialog::_vscroll()
{
    if (!_scrollock) {
        _scrollpos = _vadj->get_value();
    } else {
        _vadj->set_value(_scrollpos);
        _scrollock = false;
    }
}

void SelectorsDialog::_showWidgets()
{
    // Pack widgets
    g_debug("SelectorsDialog::_showWidgets");

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool dir = prefs->getBool("/dialogs/selectors/vertical", true);
    _paned.set_orientation(dir ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL);
    _selectors_box.set_orientation(Gtk::ORIENTATION_VERTICAL);
    _selectors_box.set_name("SelectorsDialog");
    _scrolled_window_selectors.add(_treeView);
    _scrolled_window_selectors.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    _scrolled_window_selectors.set_overlay_scrolling(false);
    _vadj = _scrolled_window_selectors.get_vadjustment();
    _vadj->signal_value_changed().connect(sigc::mem_fun(*this, &SelectorsDialog::_vscroll));
    _selectors_box.pack_start(_scrolled_window_selectors, Gtk::PACK_EXPAND_WIDGET);
    /* Gtk::Label *dirtogglerlabel = Gtk::manage(new Gtk::Label(_("Paned vertical")));
    dirtogglerlabel->get_style_context()->add_class("inksmall");
    _direction.property_active() = dir;
    _direction.property_active().signal_changed().connect(sigc::mem_fun(*this, &SelectorsDialog::_toggleDirection));
    _direction.get_style_context()->add_class("inkswitch"); */
    _styleButton(_create, "list-add", "Add a new CSS Selector");
    _create.signal_clicked().connect(sigc::mem_fun(*this, &SelectorsDialog::_addSelector));
    _styleButton(_del, "list-remove", "Remove a CSS Selector");
    _button_box.pack_start(_create, Gtk::PACK_SHRINK);
    _button_box.pack_start(_del, Gtk::PACK_SHRINK);
    Gtk::RadioButton::Group group;
    Gtk::RadioButton *_horizontal = Gtk::manage(new Gtk::RadioButton());
    Gtk::RadioButton *_vertical = Gtk::manage(new Gtk::RadioButton());
    _horizontal->set_image_from_icon_name(INKSCAPE_ICON("horizontal"));
    _vertical->set_image_from_icon_name(INKSCAPE_ICON("vertical"));
    _horizontal->set_group(group);
    _vertical->set_group(group);
    _vertical->set_active(dir);
    _vertical->signal_toggled().connect(
        sigc::bind(sigc::mem_fun(*this, &SelectorsDialog::_toggleDirection), _vertical));
    _horizontal->property_draw_indicator() = false;
    _vertical->property_draw_indicator() = false;
    _button_box.pack_end(*_horizontal, false, false, 0);
    _button_box.pack_end(*_vertical, false, false, 0);
    _del.signal_clicked().connect(sigc::mem_fun(*this, &SelectorsDialog::_delSelector));
    _del.hide();
    _style_dialog = Gtk::make_managed<StyleDialog>();
    _style_dialog->set_name("StyleDialog");
    _paned.pack1(*_style_dialog, Gtk::SHRINK);
    _paned.pack2(_selectors_box, true, true);
    _paned.set_wide_handle(true);
    Gtk::Box *contents = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    contents->pack_start(_paned, Gtk::PACK_EXPAND_WIDGET);
    contents->pack_start(_button_box, false, false, 0);
    contents->set_valign(Gtk::ALIGN_FILL);
    contents->child_property_fill(_paned);
    pack_start(*contents, Gtk::PACK_EXPAND_WIDGET);
    show_all();
    _updating = true;
    _paned.property_position() = 200;
    _updating = false;
    set_size_request(320, -1);
    set_name("SelectorsAndStyleDialog");
}

void SelectorsDialog::_toggleDirection(Gtk::RadioButton *vertical)
{
    g_debug("SelectorsDialog::_toggleDirection");
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool dir = vertical->get_active();
    prefs->setBool("/dialogs/selectors/vertical", dir);
    _paned.set_orientation(dir ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL);
    _paned.check_resize();
    int widthpos = _paned.property_max_position() - _paned.property_min_position();
    prefs->setInt("/dialogs/selectors/panedpos", widthpos / 2);
    _paned.property_position() = widthpos / 2;
}

/**
 * @return Inkscape::XML::Node* pointing to a style element's text node.
 * Returns the style element's text node. If there is no style element, one is created.
 * Ditto for text node.
 */
Inkscape::XML::Node *SelectorsDialog::_getStyleTextNode(bool create_if_missing)
{
    g_debug("SelectorsDialog::_getStyleTextNode");

    auto textNode = Inkscape::get_first_style_text_node(m_root, create_if_missing);

    if (_textNode != textNode) {
        if (_textNode) {
            _textNode->removeObserver(*m_styletextwatcher);
        }

        _textNode = textNode;

        if (_textNode) {
            _textNode->addObserver(*m_styletextwatcher);
        }
    }

    return textNode;
}

/**
 * Fill the Gtk::TreeStore from the svg:style element.
 */
void SelectorsDialog::_readStyleElement()
{
    g_debug("SelectorsDialog::_readStyleElement(): updating %s", (_updating ? "true" : "false"));

    if (_updating) return; // Don't read if we wrote style element.
    _updating = true;
    _scrollock = true;
    Inkscape::XML::Node * textNode = _getStyleTextNode();

    // Get content from style text node.
    std::string content = (textNode && textNode->content()) ? textNode->content() : "";

    // Remove end-of-lines (check it works on Windoze).
    content.erase(std::remove(content.begin(), content.end(), '\n'), content.end());

    // Remove comments (/* xxx */)
#if 0
        while(content.find("/*") != std::string::npos) {
            size_t start = content.find("/*");
            content.erase(start, (content.find("*\/", start) - start) +2);
        }
#endif

    // First split into selector/value chunks.
    // An attempt to use Glib::Regex failed. A C++11 version worked but
    // reportedly has problems on Windows. Using split_simple() is simpler
    // and probably faster.
    //
    // Glib::RefPtr<Glib::Regex> regex1 =
    //   Glib::Regex::create("([^\\{]+)\\{([^\\{]+)\\}");
    //
    // Glib::MatchInfo minfo;
    // regex1->match(content, minfo);

    // Split on curly brackets. Even tokens are selectors, odd are values.
    std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[}{]", content);

    // If text node is empty, return (avoids problem with negative below).
    if (tokens.size() == 0) {
        _store->clear();
        _updating = false;
        return;
    }
    _treeView.show_all();
    std::vector<std::pair<Glib::ustring, bool>> expanderstatus;
    for (unsigned i = 0; i < tokens.size() - 1; i += 2) {
        Glib::ustring selector = tokens[i];
        Util::trim(selector, ","); // Remove leading/trailing spaces and commas
        std::vector<Glib::ustring> selectordata = Glib::Regex::split_simple(";", selector);
        if (!selectordata.empty()) {
            selector = selectordata.back();
        }
        selector = _style_dialog->fixCSSSelectors(selector);
        for (auto &row : _store->children()) {
            Glib::ustring selectorold = row[_mColumns._colSelector];
            if (selectorold == selector) {
                expanderstatus.emplace_back(selector, row[_mColumns._colExpand]);
            }
        }
    }
    _store->clear();
    bool rewrite = false;


    for (unsigned i = 0; i < tokens.size()-1; i += 2) {
        Glib::ustring selector = tokens[i];
        Util::trim(selector, ","); // Remove leading/trailing spaces and commas
        std::vector<Glib::ustring> selectordata = Glib::Regex::split_simple(";", selector);
        for (auto selectoritem : selectordata) {
            if (selectordata[selectordata.size() - 1] == selectoritem) {
                selector = selectoritem;
            } else {
                Gtk::TreeModel::Row row = *(_store->append());
                row[_mColumns._colSelector] = selectoritem + ";";
                row[_mColumns._colExpand] = false;
                row[_mColumns._colType] = OTHER;
                row[_mColumns._colObj] = nullptr;
                row[_mColumns._colProperties] = "";
                row[_mColumns._colVisible] = true;
                row[_mColumns._colSelected] = 400;
            }
        }
        Glib::ustring selector_old = selector;
        selector = _style_dialog->fixCSSSelectors(selector);
        if (selector_old != selector) {
            rewrite = true;
        }

        if (selector.empty() || selector == "* > .inkscapehacktmp") {
            continue;
        }
        std::vector<Glib::ustring> tokensplus = Glib::Regex::split_simple("[,]+", selector);
        coltype colType = SELECTOR;

        Glib::ustring properties;
        // Check to make sure we do have a value to match selector.
        if ((i+1) < tokens.size()) {
            properties = tokens[i+1];
        } else {
            std::cerr << "SelectorsDialog::_readStyleElement(): Missing values "
                         "for last selector!"
                      << std::endl;
        }
        Util::trim(properties);
        bool colExpand = false;
        for (auto rowstatus : expanderstatus) {
            if (selector == rowstatus.first) {
                colExpand = rowstatus.second;
            }
        }
        std::vector<Glib::ustring> properties_data = Glib::Regex::split_simple(";", properties);
        Gtk::TreeModel::Row row = *(_store->append());
        row[_mColumns._colSelector] = selector;
        row[_mColumns._colExpand] = colExpand;
        row[_mColumns._colType] = colType;
        row[_mColumns._colObj] = nullptr;
        row[_mColumns._colProperties] = properties;
        row[_mColumns._colVisible] = true;
        row[_mColumns._colSelected] = 400;
        // Add as children, objects that match selector.
        for (auto &obj : _getObjVec(selector)) {
            auto *id = obj->getId();
            if (!id)
                continue;
            Gtk::TreeModel::Row childrow = *(_store->append(row->children()));
            childrow[_mColumns._colSelector] = "#" + Glib::ustring(id);
            childrow[_mColumns._colExpand] = false;
            childrow[_mColumns._colType] = colType == OBJECT;
            childrow[_mColumns._colObj] = obj;
            childrow[_mColumns._colProperties] = ""; // Unused
            childrow[_mColumns._colVisible] = true;  // Unused
            childrow[_mColumns._colSelected] = 400;
        }
    }


    _updating = false;
    if (rewrite) {
        _writeStyleElement();
    }
    _scrollock = false;
    _vadj->set_value(std::min(_scrollpos, _vadj->get_upper()));
}

void SelectorsDialog::_rowExpand(const Gtk::TreeModel::iterator &iter, const Gtk::TreeModel::Path &path)
{
    g_debug("SelectorsDialog::_row_expand()");
    Gtk::TreeModel::Row row = *iter;
    row[_mColumns._colExpand] = true;
}

void SelectorsDialog::_rowCollapse(const Gtk::TreeModel::iterator &iter, const Gtk::TreeModel::Path &path)
{
    g_debug("SelectorsDialog::_row_collapse()");
    Gtk::TreeModel::Row row = *iter;
    row[_mColumns._colExpand] = false;
}
/**
 * Update the content of the style element as selectors (or objects) are added/removed.
 */
void SelectorsDialog::_writeStyleElement()
{

    if (_updating) {
        return;
    }

    g_debug("SelectorsDialog::_writeStyleElement");

    _scrollock = true;
    _updating = true;
    Glib::ustring styleContent = "";
    for (auto& row: _store->children()) {
        Glib::ustring selector = row[_mColumns._colSelector];
#if 0
                Util::trim(selector, ",");
                row[_mColumns._colSelector] =  selector;
#endif
        if (row[_mColumns._colType] == OTHER) {
            styleContent = selector + styleContent;
        } else {
            styleContent = styleContent + selector + " { " + row[_mColumns._colProperties] + " }\n";
        }
    }
    // We could test if styleContent is empty and then delete the style node here but there is no
    // harm in keeping it around ...
    Inkscape::XML::Node *textNode = _getStyleTextNode(true);
    bool empty = false;
    if (styleContent.empty()) {
        empty = true;
        styleContent = "* > .inkscapehacktmp{}";
    }
    textNode->setContent(styleContent.c_str());
    if (empty) {
        styleContent = "";
        textNode->setContent(styleContent.c_str());
    }
    textNode->setContent(styleContent.c_str());
    DocumentUndo::done(SP_ACTIVE_DOCUMENT, _("Edited style element."), INKSCAPE_ICON("dialog-selectors"));

    _updating = false;
    _scrollock = false;
    _vadj->set_value(std::min(_scrollpos, _vadj->get_upper()));
    g_debug("SelectorsDialog::_writeStyleElement(): | %s |", styleContent.c_str());
}

Glib::ustring SelectorsDialog::_getSelectorClasses(Glib::ustring selector)
{
    g_debug("SelectorsDialog::_getSelectorClasses");

    std::pair<Glib::ustring, Glib::ustring> result;
    std::vector<Glib::ustring> tokensplus = Glib::Regex::split_simple("[ ]+", selector);
    selector = tokensplus[tokensplus.size() - 1];
    // Erase any comma/space
    Util::trim(selector, ",");
    Glib::ustring toparse = Glib::ustring(selector);
    selector = Glib::ustring("");
    auto i = toparse.find(".");
    if (i == std::string::npos) {
        return "";
    }
    if (toparse[0] != '.' && toparse[0] != '#') {
        i = std::min(toparse.find("#"), toparse.find("."));
        Glib::ustring tag = toparse.substr(0, i);
        if (!SPAttributeRelSVG::isSVGElement(tag)) {
            return selector;
        }
        if (i != std::string::npos) {
            toparse.erase(0, i);
        }
    }
    i = toparse.find("#");
    if (i != std::string::npos) {
        toparse.erase(i, 1);
    }
    auto j = toparse.find("#");
    if (j != std::string::npos) {
        return selector;
    }
    if (i != std::string::npos) {
        toparse.insert(i, "#");
        if (i) {
            Glib::ustring post = toparse.substr(0, i);
            Glib::ustring pre = toparse.substr(i, toparse.size() - i);
            toparse = pre + post;
        }
        auto k = toparse.find(".");
        if (k != std::string::npos) {
            toparse = toparse.substr(k, toparse.size() - k);
        }
    }
    return toparse;
}

/**
 * @param row
 * Add selected objects on the desktop to the selector corresponding to 'row'.
 */
void SelectorsDialog::_addToSelector(Gtk::TreeModel::Row row)
{
    g_debug("SelectorsDialog::_addToSelector: Entrance");
    if (*row) {
        // Store list of selected elements on desktop (not to be confused with selector).
        _updating = true;
        if (row[_mColumns._colType] == OTHER) {
            return;
        }
        Inkscape::Selection *selection = getDesktop()->getSelection();
        std::vector<SPObject *> toAddObjVec(selection->objects().begin(), selection->objects().end());
        Glib::ustring multiselector = row[_mColumns._colSelector];
        row[_mColumns._colExpand] = true;
        std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[,]+", multiselector);
        for (auto &obj : toAddObjVec) {
            auto *id = obj->getId();
            if (!id)
                continue;
            for (auto tok : tokens) {
                Glib::ustring clases = _getSelectorClasses(tok);
                if (!clases.empty()) {
                    _insertClass(obj, clases);
                    std::vector<SPObject *> currentobjs = _getObjVec(multiselector);
                    bool removeclass = true;
                    for (auto currentobj : currentobjs) {
                        if (g_strcmp0(currentobj->getId(), id) == 0) {
                            removeclass = false;
                        }
                    }
                    if (removeclass) {
                        _removeClass(obj, clases);
                    }
                }
            }
            std::vector<SPObject *> currentobjs = _getObjVec(multiselector);
            bool insertid = true;
            for (auto currentobj : currentobjs) {
                if (g_strcmp0(currentobj->getId(), id) == 0) {
                    insertid = false;
                }
            }
            if (insertid) {
                multiselector = multiselector + ",#" + id;
            }
            Gtk::TreeModel::Row childrow = *(_store->prepend(row->children()));
            childrow[_mColumns._colSelector] = "#" + Glib::ustring(id);
            childrow[_mColumns._colExpand] = false;
            childrow[_mColumns._colType] = OBJECT;
            childrow[_mColumns._colObj] = obj;
            childrow[_mColumns._colProperties] = ""; // Unused
            childrow[_mColumns._colVisible] = true;  // Unused
            childrow[_mColumns._colSelected] = 400;
        }
        row[_mColumns._colSelector] = multiselector;
        _updating = false;

        // Add entry to style element
        for (auto &obj : toAddObjVec) {
            Glib::ustring css_str = "";
            SPCSSAttr *css = sp_repr_css_attr_new();
            SPCSSAttr *css_selector = sp_repr_css_attr_new();
            sp_repr_css_attr_add_from_string(css, obj->getRepr()->attribute("style"));
            Glib::ustring selprops = row[_mColumns._colProperties];
            sp_repr_css_attr_add_from_string(css_selector, selprops.c_str());
            for (const auto & iter : css_selector->attributeList()) {
                gchar const *key = g_quark_to_string(iter.key);
                css->removeAttribute(key);
            }
            sp_repr_css_write_string(css, css_str);
            sp_repr_css_attr_unref(css);
            sp_repr_css_attr_unref(css_selector);
            obj->getRepr()->setAttribute("style", css_str);
            obj->style->readFromObject(obj);
            obj->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
        }
        _writeStyleElement();
    }
}

/**
 * @param row
 * Remove the object corresponding to 'row' from the parent selector.
 */
void SelectorsDialog::_removeFromSelector(Gtk::TreeModel::Row row)
{
    g_debug("SelectorsDialog::_removeFromSelector: Entrance");
    if (*row) {
        _scrollock = true;
        _updating = true;
        SPObject *obj = nullptr;
        Glib::ustring objectLabel = row[_mColumns._colSelector];
        Gtk::TreeModel::iterator iter = row->parent();
        if (iter) {
            Gtk::TreeModel::Row parent = *iter;
            Glib::ustring multiselector = parent[_mColumns._colSelector];
            Util::trim(multiselector, ",");
            obj = _getObjVec(objectLabel)[0];
            std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[,]+", multiselector);
            Glib::ustring selector = "";
            for (auto tok : tokens) {
                if (tok.empty()) {
                    continue;
                }
                // TODO: handle when other selectors has the removed class applied to maybe not remove
                Glib::ustring clases = _getSelectorClasses(tok);
                if (!clases.empty()) {
                    _removeClass(obj, tok, true);
                }
                auto i = tok.find(row[_mColumns._colSelector]);
                if (i == std::string::npos) {
                    selector = selector.empty() ? tok : selector + "," + tok;
                }
            }
            Util::trim(selector);
            if (selector.empty()) {
                _store->erase(parent);

            } else {
                _store->erase(row);
                parent[_mColumns._colSelector] = selector;
                parent[_mColumns._colExpand] = true;
                parent[_mColumns._colObj] = nullptr;
            }
        }
        _updating = false;

        // Add entry to style element
        _writeStyleElement();
        obj->style->readFromObject(obj);
        obj->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
        _scrollock = false;
        _vadj->set_value(std::min(_scrollpos, _vadj->get_upper()));
    }
}


/**
 * @param sel
 * @return This function returns a comma separated list of ids for objects in input vector.
 * It is used in creating an 'id' selector. It relies on objects having 'id's.
 */
Glib::ustring SelectorsDialog::_getIdList(std::vector<SPObject *> sel)
{
    g_debug("SelectorsDialog::_getIdList");

    Glib::ustring str;
    for (auto& obj: sel) {
        char const *id = obj->getId();
        if (id) {
            if (!str.empty()) {
                str.append(", ");
            }
            str.append("#").append(id);
        }
    }
    return str;
}

/**
 * @param selector: a valid CSS selector string.
 * @return objVec: a vector of pointers to SPObject's the selector matches.
 * Return a vector of all objects that selector matches.
 */
std::vector<SPObject *> SelectorsDialog::_getObjVec(Glib::ustring selector)
{

    g_debug("SelectorsDialog::_getObjVec: | %s |", selector.c_str());

    g_assert(selector.find(";") == Glib::ustring::npos);

    return getDesktop()->getDocument()->getObjectsBySelector(selector);
}


/**
 * @param objs: list of objects to insert class
 * @param class: class to insert
 * Insert a class name into objects' 'class' attribute.
 */
void SelectorsDialog::_insertClass(const std::vector<SPObject *> &objVec, const Glib::ustring &className)
{
    g_debug("SelectorsDialog::_insertClass");

    for (auto& obj: objVec) {
        _insertClass(obj, className);
    }
}

/**
 * @param objs: list of objects to insert class
 * @param class: class to insert
 * Insert a class name into objects' 'class' attribute.
 */
void SelectorsDialog::_insertClass(SPObject *obj, const Glib::ustring &className)
{
    g_debug("SelectorsDialog::_insertClass");

    Glib::ustring classAttr = Glib::ustring("");
    if (obj->getRepr()->attribute("class")) {
        classAttr = obj->getRepr()->attribute("class");
    }
    std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[.]+", className);
    std::sort(tokens.begin(), tokens.end());
    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
    std::vector<Glib::ustring> tokensplus = Glib::Regex::split_simple("[\\s]+", classAttr);
    for (auto tok : tokens) {
        bool exist = false;
        for (auto &tokenplus : tokensplus) {
            if (tokenplus == tok) {
                exist = true;
            }
        }
        if (!exist) {
            classAttr = classAttr.empty() ? tok : classAttr + " " + tok;
        }
    }
    obj->getRepr()->setAttribute("class", classAttr);
}

/**
 * @param objs: list of objects to insert class
 * @param class: class to insert
 * Insert a class name into objects' 'class' attribute.
 */
void SelectorsDialog::_removeClass(const std::vector<SPObject *> &objVec, const Glib::ustring &className, bool all)
{
    g_debug("SelectorsDialog::_removeClass");

    for (auto &obj : objVec) {
        _removeClass(obj, className, all);
    }
}

/**
 * @param objs: list of objects to insert class
 * @param class: class to insert
 * Insert a class name into objects' 'class' attribute.
 */
void SelectorsDialog::_removeClass(SPObject *obj, const Glib::ustring &className, bool all) // without "."
{
    g_debug("SelectorsDialog::_removeClass");

    if (obj->getRepr()->attribute("class")) {
        std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[.]+", className);
        Glib::ustring classAttr = obj->getRepr()->attribute("class");
        Glib::ustring classAttrRestore = classAttr;
        bool notfound = false;
        for (auto tok : tokens) {
            auto i = classAttr.find(tok);
            if (i != std::string::npos) {
                classAttr.erase(i, tok.length());
            } else {
                notfound = true;
            }
        }
        if (all && notfound) {
            classAttr = classAttrRestore;
        }
        Util::trim(classAttr, ",");
        if (classAttr.empty()) {
            obj->getRepr()->removeAttribute("class");
        } else {
            obj->getRepr()->setAttribute("class", classAttr);
        }
    }
}


/**
 * @param eventX
 * @param eventY
 * This function selects objects in the drawing corresponding to the selector
 * selected in the treeview.
 */
void SelectorsDialog::_selectObjects(int eventX, int eventY)
{
    g_debug("SelectorsDialog::_selectObjects: %d, %d", eventX, eventY);
    Gtk::TreeViewColumn *col = _treeView.get_column(1);
    Gtk::TreeModel::Path path;
    int x2 = 0;
    int y2 = 0;
    // To do: We should be able to do this via passing in row.
    if (_treeView.get_path_at_pos(eventX, eventY, path, col, x2, y2)) {
        if (_lastpath.size() && _lastpath == path) {
            return;
        }
        if (col == _treeView.get_column(1) && x2 > 25) {
            getDesktop()->getSelection()->clear();
            Gtk::TreeModel::iterator iter = _store->get_iter(path);
            if (iter) {
                Gtk::TreeModel::Row row = *iter;
                if (row[_mColumns._colObj]) {
                    getDesktop()->getSelection()->add(row[_mColumns._colObj]);
                }
                Gtk::TreeModel::Children children = row.children();
                if (children.empty() || children.size() == 1) {
                    _del.show();
                }
                for (auto child : row.children()) {
                    Gtk::TreeModel::Row child_row = *child;
                    if (child[_mColumns._colObj]) {
                        getDesktop()->getSelection()->add(child[_mColumns._colObj]);
                    }
                }
            }
            _lastpath = path;
        }
    }
}

/**
 * This function opens a dialog to add a selector. The dialog is prefilled
 * with an 'id' selector containing a list of the id's of selected objects
 * or with a 'class' selector if no objects are selected.
 */
void SelectorsDialog::_addSelector()
{
    g_debug("SelectorsDialog::_addSelector: Entrance");
    _scrollock = true;
    // Store list of selected elements on desktop (not to be confused with selector).
    Inkscape::Selection* selection = getDesktop()->getSelection();
    std::vector<SPObject *> objVec( selection->objects().begin(),
                                    selection->objects().end() );

    // ==== Create popup dialog ====
    Gtk::Dialog *textDialogPtr =  new Gtk::Dialog();
    textDialogPtr->property_modal() = true;
    textDialogPtr->property_title() = _("CSS selector");
    textDialogPtr->property_window_position() = Gtk::WIN_POS_CENTER_ON_PARENT;
    textDialogPtr->add_button(_("Cancel"), Gtk::RESPONSE_CANCEL);
    textDialogPtr->add_button(_("Add"),    Gtk::RESPONSE_OK);

    Gtk::Entry *textEditPtr = manage ( new Gtk::Entry() );
    textEditPtr->signal_activate().connect(
        sigc::bind<Gtk::Dialog *>(sigc::mem_fun(*this, &SelectorsDialog::_closeDialog), textDialogPtr));
    textDialogPtr->get_content_area()->pack_start(*textEditPtr, Gtk::PACK_SHRINK);

    Gtk::Label *textLabelPtr = manage(new Gtk::Label(_("Invalid CSS selector.")));
    textDialogPtr->get_content_area()->pack_start(*textLabelPtr, Gtk::PACK_SHRINK);

    /**
     * By default, the entrybox contains 'Class1' as text. However, if object(s)
     * is(are) selected and user clicks '+' at the bottom of dialog, the
     * entrybox will have the id(s) of the selected objects as text.
     */
    if (getDesktop()->getSelection()->isEmpty()) {
        textEditPtr->set_text(".Class1");
    } else {
        textEditPtr->set_text(_getIdList(objVec));
    }

    Gtk::Requisition sreq1, sreq2;
    textDialogPtr->get_preferred_size(sreq1, sreq2);
    int minWidth = 200;
    int minHeight = 100;
    minWidth  = (sreq2.width  > minWidth  ? sreq2.width  : minWidth );
    minHeight = (sreq2.height > minHeight ? sreq2.height : minHeight);
    textDialogPtr->set_size_request(minWidth, minHeight);
    textEditPtr->show();
    textLabelPtr->hide();
    textDialogPtr->show();


    // ==== Get response ====
    int result = -1;
    bool invalid = true;
    Glib::ustring selectorValue;
    Glib::ustring originalValue;
    while (invalid) {
        result = dialog_run(*textDialogPtr);
        if (result != Gtk::RESPONSE_OK) { // Cancel, close dialog, etc.
            delete textDialogPtr;
            return;
        }
        /**
         * @brief selectorName
         * This string stores selector name. The text from entrybox is saved as name
         * for selector. If the entrybox is empty, the text (thus selectorName) is
         * set to ".Class1"
         */
        originalValue = Glib::ustring(textEditPtr->get_text());
        selectorValue = _style_dialog->fixCSSSelectors(originalValue);
        _del.show();
        if (originalValue.find("@import ") == std::string::npos && selectorValue.empty()) {
            textLabelPtr->show();
        } else {
            invalid = false;
        }
    }
    delete textDialogPtr;
    // ==== Handle response ====
    // If class selector, add selector name to class attribute for each object
    Util::trim(selectorValue, ",");
    if (originalValue.find("@import ") != std::string::npos) {
        Gtk::TreeModel::Row row = *(_store->prepend());
        row[_mColumns._colSelector] = originalValue;
        row[_mColumns._colExpand] = false;
        row[_mColumns._colType] = OTHER;
        row[_mColumns._colObj] = nullptr;
        row[_mColumns._colProperties] = "";
        row[_mColumns._colVisible] = true;
        row[_mColumns._colSelected] = 400;
    } else {
        std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[,]+", selectorValue);
        for (auto &obj : objVec) {
            for (auto tok : tokens) {
                Glib::ustring clases = _getSelectorClasses(tok);
                if (clases.empty()) {
                    continue;
                }
                _insertClass(obj, clases);
                std::vector<SPObject *> currentobjs = _getObjVec(selectorValue);
                bool removeclass = true;
                for (auto currentobj : currentobjs) {
                    if (currentobj == obj) {
                        removeclass = false;
                    }
                }
                if (removeclass) {
                    _removeClass(obj, clases);
                }
            }
        }
        Gtk::TreeModel::Row row = *(_store->prepend());
        row[_mColumns._colExpand] = true;
        row[_mColumns._colType] = SELECTOR;
        row[_mColumns._colSelector] = selectorValue;
        row[_mColumns._colObj] = nullptr;
        row[_mColumns._colProperties] = "";
        row[_mColumns._colVisible] = true;
        row[_mColumns._colSelected] = 400;
        for (auto &obj : _getObjVec(selectorValue)) {
            auto *id = obj->getId();
            if (!id)
                continue;
            Gtk::TreeModel::Row childrow = *(_store->prepend(row->children()));
            childrow[_mColumns._colSelector] = "#" + Glib::ustring(id);
            childrow[_mColumns._colExpand] = false;
            childrow[_mColumns._colType] = OBJECT;
            childrow[_mColumns._colObj] = obj;
            childrow[_mColumns._colProperties] = ""; // Unused
            childrow[_mColumns._colVisible] = true;  // Unused
            childrow[_mColumns._colSelected] = 400;
        }
    }
    // Add entry to style element
    _writeStyleElement();
    _scrollock = false;
    _vadj->set_value(std::min(_scrollpos, _vadj->get_upper()));
}

void SelectorsDialog::_closeDialog(Gtk::Dialog *textDialogPtr) { textDialogPtr->response(Gtk::RESPONSE_OK); }

/**
 * This function deletes selector when '-' at the bottom is clicked.
 * Note: If deleting a class selector, class attributes are NOT changed.
 */
void SelectorsDialog::_delSelector()
{
    g_debug("SelectorsDialog::_delSelector");

    _scrollock = true;
    Glib::RefPtr<Gtk::TreeSelection> refTreeSelection = _treeView.get_selection();
    Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
    if (iter) {
        _vscroll();
        Gtk::TreeModel::Row row = *iter;
        if (row.children().size() > 2) {
            return;
        }
        _updating = true;
        _store->erase(iter);
        _updating = false;
        _writeStyleElement();
        _del.hide();
        _scrollock = false;
        _vadj->set_value(std::min(_scrollpos, _vadj->get_upper()));
    }
}

/**
 * @param event
 * @return
 * Handles the event when '+' button in front of a selector name is clicked or when a '-' button in
 * front of a child object is clicked. In the first case, the selected objects on the desktop (if
 * any) are added as children of the selector in the treeview. In the latter case, the object
 * corresponding to the row is removed from the selector.
 */
bool SelectorsDialog::_handleButtonEvent(GdkEventButton *event)
{
    g_debug("SelectorsDialog::_handleButtonEvent: Entrance");
    if (event->type == GDK_BUTTON_RELEASE && event->button == 1) {
        _scrollock = true;
        Gtk::TreeViewColumn *col = nullptr;
        Gtk::TreeModel::Path path;
        int x = static_cast<int>(event->x);
        int y = static_cast<int>(event->y);
        int x2 = 0;
        int y2 = 0;

        if (_treeView.get_path_at_pos(x, y, path, col, x2, y2)) {
            if (col == _treeView.get_column(0)) {
                _vscroll();
                Gtk::TreeModel::iterator iter = _store->get_iter(path);
                Gtk::TreeModel::Row row = *iter;
                if (!row.parent()) {
                    _addToSelector(row);
                } else {
                    _removeFromSelector(row);
                }
                _vadj->set_value(std::min(_scrollpos, _vadj->get_upper()));
            }
        }
    }
    return false;
}

// -------------------------------------------------------------------

class PropertyData
{
public:
    PropertyData() = default;;
    PropertyData(Glib::ustring name) : _name(std::move(name)) {};

    void _setSheetValue(Glib::ustring value) { _sheetValue = value; };
    void _setAttrValue(Glib::ustring value)  { _attrValue  = value; };
    Glib::ustring _getName()       { return _name;       };
    Glib::ustring _getSheetValue() { return _sheetValue; };
    Glib::ustring _getAttrValue()  { return _attrValue;  };

private:
    Glib::ustring _name;
    Glib::ustring _sheetValue;
    Glib::ustring _attrValue;
};

// -------------------------------------------------------------------

SelectorsDialog::~SelectorsDialog()
{
    removeObservers();
    _style_dialog->setDesktop(nullptr);
}

void SelectorsDialog::update()
{
    _style_dialog->update();
}

void SelectorsDialog::desktopReplaced()
{
    _style_dialog->setDesktop(getDesktop());
}

void SelectorsDialog::removeObservers()
{
    if (_textNode) {
        _textNode->removeObserver(*m_styletextwatcher);
        _textNode = nullptr;
    }
    if (m_root) {
        m_root->removeSubtreeObserver(*m_nodewatcher);
        m_root = nullptr;
    }
}

void SelectorsDialog::documentReplaced()
{
    removeObservers();
    if (auto document = getDocument()) {
        m_root = document->getReprRoot();
        m_root->addSubtreeObserver(*m_nodewatcher);
    }
    selectionChanged(getSelection());
}

void SelectorsDialog::selectionChanged(Selection *selection)
{
    _lastpath.clear();
    _readStyleElement();
    _selectRow();
}

/**
 * @param event
 * This function detects single or double click on a selector in any row. Clicking
 * on a selector selects the matching objects on the desktop. A double click will
 * in addition open the CSS dialog.
 */
void SelectorsDialog::_buttonEventsSelectObjs(GdkEventButton *event)
{
    g_debug("SelectorsDialog::_buttonEventsSelectObjs");
    if (event->type == GDK_BUTTON_RELEASE && event->button == 1) {
        _updating = true;
        _del.show();
        int x = static_cast<int>(event->x);
        int y = static_cast<int>(event->y);
        _selectObjects(x, y);
        _updating = false;
        _selectRow();
    }
}


/**
 * This function selects the row in treeview corresponding to an object selected
 * in the drawing. If more than one row matches, the first is chosen.
 */
void SelectorsDialog::_selectRow()
{
    _scrollock = true;
    g_debug("SelectorsDialog::_selectRow: updating: %s", (_updating ? "true" : "false"));
    _del.hide();
    std::vector<Gtk::TreeModel::Path> selectedrows = _treeView.get_selection()->get_selected_rows();
    if (selectedrows.size() == 1) {
        Gtk::TreeModel::Row row = *_store->get_iter(selectedrows[0]);
        if (!row->parent() && row->children().size() < 2) {
            _del.show();
        }
        if (row) {
            _style_dialog->setCurrentSelector(row[_mColumns._colSelector]);
        }
    } else if (selectedrows.size() == 0) {
        _del.show();
    }
    if (_updating || !getDesktop()) return; // Avoid updating if we have set row via dialog.

    Gtk::TreeModel::Children children = _store->children();
    Inkscape::Selection* selection = getDesktop()->getSelection();
    if (selection->isEmpty()) {
        _style_dialog->setCurrentSelector("");
    }
    for (auto row : children) {
        row[_mColumns._colSelected] = 400;
        Gtk::TreeModel::Children subchildren = row->children();
        for (auto subrow : subchildren) {
            subrow[_mColumns._colSelected] = 400;
        }
    }

    // Sort selection for matching.
    std::vector<SPObject *> selected_objs(
        selection->objects().begin(), selection->objects().end());
    std::sort(selected_objs.begin(), selected_objs.end());

    for (auto row : children) {
        // Recalculate the selector, in real time.
        auto row_children = _getObjVec(row[_mColumns._colSelector]);
        std::sort(row_children.begin(), row_children.end());

        // If all selected objects are in the css-selector, select it.
        if (row_children == selected_objs) {
            row[_mColumns._colSelected] = 700;
        }

        Gtk::TreeModel::Children subchildren = row->children();

        for (auto subrow : subchildren) {
            if (subrow[_mColumns._colObj] && selection->includes(subrow[_mColumns._colObj])) {
                subrow[_mColumns._colSelected] = 700;
            }
            if (row[_mColumns._colExpand]) {
                _treeView.expand_to_path(Gtk::TreePath(row));
            }
        }
        if (row[_mColumns._colExpand]) {
            _treeView.expand_to_path(Gtk::TreePath(row));
        }
    }
    _vadj->set_value(std::min(_scrollpos, _vadj->get_upper()));
}

/**
 * @param btn
 * @param iconName
 * @param tooltip
 * Set the style of '+' and '-' buttons at the bottom of dialog.
 */
void SelectorsDialog::_styleButton(Gtk::Button &btn, char const *iconName, char const *tooltip)
{
    g_debug("SelectorsDialog::_styleButton");

    GtkWidget *child = sp_get_icon_image(iconName, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_show(child);
    btn.add(*manage(Glib::wrap(child)));
    btn.set_relief(Gtk::RELIEF_NONE);
    btn.set_tooltip_text (tooltip);
}


} // namespace Dialog
} // namespace UI
} // namespace Inkscape

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
