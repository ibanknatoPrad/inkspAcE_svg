// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Node aux toolbar
 */
/* Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Frank Felfe <innerspace@iname.com>
 *   John Cliff <simarilius@yahoo.com>
 *   David Turner <novalis@gnu.org>
 *   Josh Andler <scislac@scislac.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2003 MenTaLguY
 * Copyright (C) 1999-2011 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "node-toolbar.h"

#include <glibmm/i18n.h>

#include <gtkmm/adjustment.h>
#include <gtkmm/image.h>
#include <gtkmm/menutoolbutton.h>
#include <gtkmm/separatortoolitem.h>

#include "desktop.h"
#include "document-undo.h"
#include "inkscape.h"
#include "selection-chemistry.h"

#include "object/sp-namedview.h"

#include "page-manager.h"

#include "ui/icon-names.h"
#include "ui/simple-pref-pusher.h"
#include "ui/tool/control-point-selection.h"
#include "ui/tool/multi-path-manipulator.h"
#include "ui/tools/node-tool.h"
#include "ui/widget/canvas.h"
#include "ui/widget/combo-tool-item.h"
#include "ui/widget/spinbutton.h"
#include "ui/widget/spin-button-tool-item.h"
#include "ui/widget/unit-tracker.h"

#include "widgets/widget-sizes.h"

using Inkscape::UI::Widget::UnitTracker;
using Inkscape::Util::Unit;
using Inkscape::Util::Quantity;
using Inkscape::DocumentUndo;
using Inkscape::Util::unit_table;
using Inkscape::UI::Tools::NodeTool;

/** Temporary hack: Returns the node tool in the active desktop.
 * Will go away during tool refactoring. */
static NodeTool *get_node_tool()
{
    NodeTool *tool = nullptr;
    if (SP_ACTIVE_DESKTOP ) {
        Inkscape::UI::Tools::ToolBase *ec = SP_ACTIVE_DESKTOP->event_context;
        if (INK_IS_NODE_TOOL(ec)) {
            tool = static_cast<NodeTool*>(ec);
        }
    }
    return tool;
}

namespace Inkscape {
namespace UI {
namespace Toolbar {

NodeToolbar::NodeToolbar(SPDesktop *desktop)
    : Toolbar(desktop),
    _tracker(new UnitTracker(Inkscape::Util::UNIT_TYPE_LINEAR)),
    _freeze(false)
{
    auto prefs = Inkscape::Preferences::get();

    Unit doc_units = *desktop->getNamedView()->display_units;
    _tracker->setActiveUnit(&doc_units);

    {
        auto insert_node_item = Gtk::manage(new Gtk::MenuToolButton());
        insert_node_item->set_icon_name(INKSCAPE_ICON("node-add"));
        insert_node_item->set_label(_("Insert node"));
        insert_node_item->set_tooltip_text(_("Insert new nodes into selected segments"));
        insert_node_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_add));

        auto insert_node_menu = Gtk::manage(new Gtk::Menu());

        {
            // TODO: Consider moving back to icons in menu?
            //auto insert_min_x_icon = Gtk::manage(new Gtk::Image());
            //insert_min_x_icon->set_from_icon_name(INKSCAPE_ICON("node_insert_min_x"), Gtk::ICON_SIZE_MENU);
            //auto insert_min_x_item = Gtk::manage(new Gtk::MenuItem(*insert_min_x_icon));
            auto insert_min_x_item = Gtk::manage(new Gtk::MenuItem(_("Insert node at min X")));
            insert_min_x_item->set_tooltip_text(_("Insert new nodes at min X into selected segments"));
            insert_min_x_item->signal_activate().connect(sigc::mem_fun(*this, &NodeToolbar::edit_add_min_x));
            insert_node_menu->append(*insert_min_x_item);
        }
        {
            //auto insert_max_x_icon = Gtk::manage(new Gtk::Image());
            //insert_max_x_icon->set_from_icon_name(INKSCAPE_ICON("node_insert_max_x"), Gtk::ICON_SIZE_MENU);
            //auto insert_max_x_item = Gtk::manage(new Gtk::MenuItem(*insert_max_x_icon));
            auto insert_max_x_item = Gtk::manage(new Gtk::MenuItem(_("Insert node at max X")));
            insert_max_x_item->set_tooltip_text(_("Insert new nodes at max X into selected segments"));
            insert_max_x_item->signal_activate().connect(sigc::mem_fun(*this, &NodeToolbar::edit_add_max_x));
            insert_node_menu->append(*insert_max_x_item);
        }
        {
            //auto insert_min_y_icon = Gtk::manage(new Gtk::Image());
            //insert_min_y_icon->set_from_icon_name(INKSCAPE_ICON("node_insert_min_y"), Gtk::ICON_SIZE_MENU);
            //auto insert_min_y_item = Gtk::manage(new Gtk::MenuItem(*insert_min_y_icon));
            auto insert_min_y_item = Gtk::manage(new Gtk::MenuItem(_("Insert node at min Y")));
            insert_min_y_item->set_tooltip_text(_("Insert new nodes at min Y into selected segments"));
            insert_min_y_item->signal_activate().connect(sigc::mem_fun(*this, &NodeToolbar::edit_add_min_y));
            insert_node_menu->append(*insert_min_y_item);
        }
        {
            //auto insert_max_y_icon = Gtk::manage(new Gtk::Image());
            //insert_max_y_icon->set_from_icon_name(INKSCAPE_ICON("node_insert_max_y"), Gtk::ICON_SIZE_MENU);
            //auto insert_max_y_item = Gtk::manage(new Gtk::MenuItem(*insert_max_y_icon));
            auto insert_max_y_item = Gtk::manage(new Gtk::MenuItem(_("Insert node at max Y")));
            insert_max_y_item->set_tooltip_text(_("Insert new nodes at max Y into selected segments"));
            insert_max_y_item->signal_activate().connect(sigc::mem_fun(*this, &NodeToolbar::edit_add_max_y));
            insert_node_menu->append(*insert_max_y_item);
        }

        insert_node_menu->show_all();
        insert_node_item->set_menu(*insert_node_menu);
        add(*insert_node_item);
    }

    {
        auto delete_item = Gtk::manage(new Gtk::ToolButton(_("Delete node")));
        delete_item->set_tooltip_text(_("Delete selected nodes"));
        delete_item->set_icon_name(INKSCAPE_ICON("node-delete"));
        delete_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_delete));
        add(*delete_item);
    }

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    {
        auto join_item = Gtk::manage(new Gtk::ToolButton(_("Join nodes")));
        join_item->set_tooltip_text(_("Join selected nodes"));
        join_item->set_icon_name(INKSCAPE_ICON("node-join"));
        join_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_join));
        add(*join_item);
    }

    {
        auto break_item = Gtk::manage(new Gtk::ToolButton(_("Break nodes")));
        break_item->set_tooltip_text(_("Break path at selected nodes"));
        break_item->set_icon_name(INKSCAPE_ICON("node-break"));
        break_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_break));
        add(*break_item);
    }

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    {
        auto join_segment_item = Gtk::manage(new Gtk::ToolButton(_("Join with segment")));
        join_segment_item->set_tooltip_text(_("Join selected endnodes with a new segment"));
        join_segment_item->set_icon_name(INKSCAPE_ICON("node-join-segment"));
        join_segment_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_join_segment));
        add(*join_segment_item);
    }

    {
        auto delete_segment_item = Gtk::manage(new Gtk::ToolButton(_("Delete segment")));
        delete_segment_item->set_tooltip_text(_("Delete segment between two non-endpoint nodes"));
        delete_segment_item->set_icon_name(INKSCAPE_ICON("node-delete-segment"));
        delete_segment_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_delete_segment));
        add(*delete_segment_item);
    }

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    {
        auto cusp_item = Gtk::manage(new Gtk::ToolButton(_("Node Cusp")));
        cusp_item->set_tooltip_text(_("Make selected nodes corner"));
        cusp_item->set_icon_name(INKSCAPE_ICON("node-type-cusp"));
        cusp_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_cusp));
        add(*cusp_item);
    }

    {
        auto smooth_item = Gtk::manage(new Gtk::ToolButton(_("Node Smooth")));
        smooth_item->set_tooltip_text(_("Make selected nodes smooth"));
        smooth_item->set_icon_name(INKSCAPE_ICON("node-type-smooth"));
        smooth_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_smooth));
        add(*smooth_item);
    }

    {
        auto symmetric_item = Gtk::manage(new Gtk::ToolButton(_("Node Symmetric")));
        symmetric_item->set_tooltip_text(_("Make selected nodes symmetric"));
        symmetric_item->set_icon_name(INKSCAPE_ICON("node-type-symmetric"));
        symmetric_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_symmetrical));
        add(*symmetric_item);
    }

    {
        auto auto_item = Gtk::manage(new Gtk::ToolButton(_("Node Auto")));
        auto_item->set_tooltip_text(_("Make selected nodes auto-smooth"));
        auto_item->set_icon_name(INKSCAPE_ICON("node-type-auto-smooth"));
        auto_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_auto));
        add(*auto_item);
    }

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    {
        auto line_item = Gtk::manage(new Gtk::ToolButton(_("Node Line")));
        line_item->set_tooltip_text(_("Straighten lines"));
        line_item->set_icon_name(INKSCAPE_ICON("node-segment-line"));
        line_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_toline));
        add(*line_item);
    }

    {
        auto curve_item = Gtk::manage(new Gtk::ToolButton(_("Node Curve")));
        curve_item->set_tooltip_text(_("Add curve handles"));
        curve_item->set_icon_name(INKSCAPE_ICON("node-segment-curve"));
        curve_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_tocurve));
        add(*curve_item);
    }

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    {
        auto lpe_corners_item = Gtk::manage(new Gtk::ToolButton(_("_Add corners")));
        lpe_corners_item->set_tooltip_text(_("Add corners live path effect"));
        lpe_corners_item->set_icon_name(INKSCAPE_ICON("corners"));
        // Must use C API until GTK4.
        gtk_actionable_set_action_name(GTK_ACTIONABLE(lpe_corners_item->gobj()), "app.object-add-corners-lpe");
        add(*lpe_corners_item);
    }

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    {
        auto object_to_path_item = Gtk::manage(new Gtk::ToolButton(_("_Object to Path")));
        object_to_path_item->set_tooltip_text(_("Convert selected object to path"));
        object_to_path_item->set_icon_name(INKSCAPE_ICON("object-to-path"));
        // Must use C API until GTK4.
        gtk_actionable_set_action_name(GTK_ACTIONABLE(object_to_path_item->gobj()), "app.object-to-path");
        add(*object_to_path_item);
    }

    {
        auto stroke_to_path_item = Gtk::manage(new Gtk::ToolButton(_("_Stroke to Path")));
        stroke_to_path_item->set_tooltip_text(_("Convert selected object's stroke to paths"));
        stroke_to_path_item->set_icon_name(INKSCAPE_ICON("stroke-to-path"));
        // Must use C API until GTK4.
        gtk_actionable_set_action_name(GTK_ACTIONABLE(stroke_to_path_item->gobj()), "app.object-stroke-to-path");
        add(*stroke_to_path_item);
    }

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    /* X coord of selected node(s) */
    {
        std::vector<double> values = {1, 2, 3, 5, 10, 20, 50, 100, 200, 500};
        auto nodes_x_val = prefs->getDouble("/tools/nodes/Xcoord", 0);
        _nodes_x_adj = Gtk::Adjustment::create(nodes_x_val, -1e6, 1e6, SPIN_STEP, SPIN_PAGE_STEP);
        _nodes_x_item = Gtk::manage(new UI::Widget::SpinButtonToolItem("node-x", _("X:"), _nodes_x_adj));
        _nodes_x_item->set_tooltip_text(_("X coordinate of selected node(s)"));
        _nodes_x_item->set_custom_numeric_menu_data(values);
        _tracker->addAdjustment(_nodes_x_adj->gobj());
        _nodes_x_item->get_spin_button()->addUnitTracker(_tracker.get());
        _nodes_x_item->set_focus_widget(desktop->canvas);
        _nodes_x_adj->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &NodeToolbar::value_changed), Geom::X));
        _nodes_x_item->set_sensitive(false);
        add(*_nodes_x_item);
    }

    /* Y coord of selected node(s) */
    {
        std::vector<double> values = {1, 2, 3, 5, 10, 20, 50, 100, 200, 500};
        auto nodes_y_val = prefs->getDouble("/tools/nodes/Ycoord", 0);
        _nodes_y_adj = Gtk::Adjustment::create(nodes_y_val, -1e6, 1e6, SPIN_STEP, SPIN_PAGE_STEP);
        _nodes_y_item = Gtk::manage(new UI::Widget::SpinButtonToolItem("node-y", _("Y:"), _nodes_y_adj));
        _nodes_y_item->set_tooltip_text(_("Y coordinate of selected node(s)"));
        _nodes_y_item->set_custom_numeric_menu_data(values);
        _tracker->addAdjustment(_nodes_y_adj->gobj());
        _nodes_y_item->get_spin_button()->addUnitTracker(_tracker.get());
        _nodes_y_item->set_focus_widget(desktop->canvas);
        _nodes_y_adj->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &NodeToolbar::value_changed), Geom::Y));
        _nodes_y_item->set_sensitive(false);
        add(*_nodes_y_item);
    }

    // add the units menu
    {
        auto unit_menu = _tracker->create_tool_item(_("Units"), (""));
        add(*unit_menu);
    }

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    {
        _object_edit_clip_path_item = add_toggle_button(_("Edit clipping paths"),
                                                        _("Show clipping path(s) of selected object(s)"));
        _object_edit_clip_path_item->set_icon_name(INKSCAPE_ICON("path-clip-edit"));
        _pusher_edit_clipping_paths.reset(new SimplePrefPusher(_object_edit_clip_path_item, "/tools/nodes/edit_clipping_paths"));
        _object_edit_clip_path_item->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &NodeToolbar::on_pref_toggled),
                                                                         _object_edit_clip_path_item,
                                                                         "/tools/nodes/edit_clipping_paths"));
    }

    {
        _object_edit_mask_path_item = add_toggle_button(_("Edit masks"),
                                                        _("Show mask(s) of selected object(s)"));
        _object_edit_mask_path_item->set_icon_name(INKSCAPE_ICON("path-mask-edit"));
        _pusher_edit_masks.reset(new SimplePrefPusher(_object_edit_mask_path_item, "/tools/nodes/edit_masks"));
        _object_edit_mask_path_item->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &NodeToolbar::on_pref_toggled),
                                                                         _object_edit_mask_path_item,
                                                                         "/tools/nodes/edit_masks"));
    }

    {
        _nodes_lpeedit_item = Gtk::manage(new Gtk::ToolButton(N_("Next path effect parameter")));
        _nodes_lpeedit_item->set_tooltip_text(N_("Show next editable path effect parameter"));
        _nodes_lpeedit_item->set_icon_name(INKSCAPE_ICON("path-effect-parameter-next"));
        // Must use C API until GTK4.
        gtk_actionable_set_action_name(GTK_ACTIONABLE(_nodes_lpeedit_item->gobj()), "win.path-effect-parameter-next");
        add(*_nodes_lpeedit_item);
    }

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    {
        _show_transform_handles_item = add_toggle_button(_("Show Transform Handles"),
                                                         _("Show transformation handles for selected nodes"));
        _show_transform_handles_item->set_icon_name(INKSCAPE_ICON("node-transform"));
        _pusher_show_transform_handles.reset(new UI::SimplePrefPusher(_show_transform_handles_item, "/tools/nodes/show_transform_handles"));
        _show_transform_handles_item->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &NodeToolbar::on_pref_toggled),
                                                                          _show_transform_handles_item,
                                                                          "/tools/nodes/show_transform_handles"));
    }

    {
        _show_handles_item = add_toggle_button(_("Show Handles"),
                                               _("Show Bezier handles of selected nodes"));
        _show_handles_item->set_icon_name(INKSCAPE_ICON("show-node-handles"));
        _pusher_show_handles.reset(new UI::SimplePrefPusher(_show_handles_item, "/tools/nodes/show_handles"));
        _show_handles_item->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &NodeToolbar::on_pref_toggled),
                                                                _show_handles_item,
                                                                "/tools/nodes/show_handles"));
    }

    {
        _show_helper_path_item = add_toggle_button(_("Show Outline"),
                                                   _("Show path outline (without path effects)"));
        _show_helper_path_item->set_icon_name(INKSCAPE_ICON("show-path-outline"));
        _pusher_show_outline.reset(new UI::SimplePrefPusher(_show_helper_path_item, "/tools/nodes/show_outline"));
        _show_helper_path_item->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &NodeToolbar::on_pref_toggled),
                                                                _show_helper_path_item,
                                                                "/tools/nodes/show_outline"));
    }

    sel_changed(desktop->getSelection());
    desktop->connectEventContextChanged(sigc::mem_fun(*this, &NodeToolbar::watch_ec));

    show_all();
}

GtkWidget *
NodeToolbar::create(SPDesktop *desktop)
{
    auto holder = new NodeToolbar(desktop);
    return GTK_WIDGET(holder->gobj());
} // NodeToolbar::prep()

void
NodeToolbar::value_changed(Geom::Dim2 d)
{
    auto adj = (d == Geom::X) ? _nodes_x_adj : _nodes_y_adj;

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if (!_tracker) {
        return;
    }

    Unit const *unit = _tracker->getActiveUnit();

    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        prefs->setDouble(Glib::ustring("/tools/nodes/") + (d == Geom::X ? "x" : "y"),
            Quantity::convert(adj->get_value(), unit, "px"));
    }

    // quit if run by the attr_changed listener
    if (_freeze || _tracker->isUpdating()) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    NodeTool *nt = get_node_tool();
    if (nt && !nt->_selected_nodes->empty()) {
        double val = Quantity::convert(adj->get_value(), unit, "px");
        double oldval = nt->_selected_nodes->pointwiseBounds()->midpoint()[d];

        // Adjust the coordinate to the current page, if needed
        auto &pm = _desktop->getDocument()->getPageManager();
        if (prefs->getBool("/options/origincorrection/page", true)) {
            auto page = pm.getSelectedPageRect();
            oldval -= page.corner(0)[d];
        }

        Geom::Point delta(0,0);
        delta[d] = val - oldval;
        nt->_multipath->move(delta);
    }

    _freeze = false;
}

void
NodeToolbar::sel_changed(Inkscape::Selection *selection)
{
    SPItem *item = selection->singleItem();
    if (item && is<SPLPEItem>(item)) {
       if (cast_unsafe<SPLPEItem>(item)->hasPathEffect()) {
           _nodes_lpeedit_item->set_sensitive(true);
       } else {
           _nodes_lpeedit_item->set_sensitive(false);
       }
    } else {
       _nodes_lpeedit_item->set_sensitive(false);
    }
}

void
NodeToolbar::watch_ec(SPDesktop* desktop, Inkscape::UI::Tools::ToolBase* ec)
{
    if (INK_IS_NODE_TOOL(ec)) {
        // watch selection
        c_selection_changed = desktop->getSelection()->connectChanged(sigc::mem_fun(*this, &NodeToolbar::sel_changed));
        c_selection_modified = desktop->getSelection()->connectModified(sigc::mem_fun(*this, &NodeToolbar::sel_modified));
        c_subselection_changed = desktop->connect_control_point_selected([=](void* sender, Inkscape::UI::ControlPointSelection* selection) {
            coord_changed(selection);
        });

        sel_changed(desktop->getSelection());
    } else {
        if (c_selection_changed)
            c_selection_changed.disconnect();
        if (c_selection_modified)
            c_selection_modified.disconnect();
        if (c_subselection_changed)
            c_subselection_changed.disconnect();
    }
}

void
NodeToolbar::sel_modified(Inkscape::Selection *selection, guint /*flags*/)
{
    sel_changed(selection);
}

/* is called when the node selection is modified */
void
NodeToolbar::coord_changed(Inkscape::UI::ControlPointSelection* selected_nodes) // gpointer /*shape_editor*/)
{
    // quit if run by the attr_changed listener
    if (_freeze) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    if (!_tracker) {
        return;
    }
    Unit const *unit = _tracker->getActiveUnit();
    g_return_if_fail(unit != nullptr);

    if (!selected_nodes || selected_nodes->empty()) {
        // no path selected
        _nodes_x_item->set_sensitive(false);
        _nodes_y_item->set_sensitive(false);
    } else {
        _nodes_x_item->set_sensitive(true);
        _nodes_y_item->set_sensitive(true);
        Geom::Coord oldx = Quantity::convert(_nodes_x_adj->get_value(), unit, "px");
        Geom::Coord oldy = Quantity::convert(_nodes_y_adj->get_value(), unit, "px");
        Geom::Point mid = selected_nodes->pointwiseBounds()->midpoint();

        // Adjust shown coordinate according to the selected page
        auto prefs = Inkscape::Preferences::get();
        if (prefs->getBool("/options/origincorrection/page", true)) {
            auto &pm = _desktop->getDocument()->getPageManager();
            mid *= pm.getSelectedPageAffine().inverse();
        }

        if (oldx != mid[Geom::X]) {
            _nodes_x_adj->set_value(Quantity::convert(mid[Geom::X], "px", unit));
        }
        if (oldy != mid[Geom::Y]) {
            _nodes_y_adj->set_value(Quantity::convert(mid[Geom::Y], "px", unit));
        }
    }

    _freeze = false;
}

void
NodeToolbar::edit_add()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->insertNodes();
    }
}

void
NodeToolbar::edit_add_min_x()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->insertNodesAtExtrema(Inkscape::UI::PointManipulator::EXTR_MIN_X);
    }
}

void
NodeToolbar::edit_add_max_x()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->insertNodesAtExtrema(Inkscape::UI::PointManipulator::EXTR_MAX_X);
    }
}

void
NodeToolbar::edit_add_min_y()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->insertNodesAtExtrema(Inkscape::UI::PointManipulator::EXTR_MIN_Y);
    }
}

void
NodeToolbar::edit_add_max_y()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->insertNodesAtExtrema(Inkscape::UI::PointManipulator::EXTR_MAX_Y);
    }
}

void
NodeToolbar::edit_delete()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        nt->_multipath->deleteNodes(prefs->getBool("/tools/nodes/delete_preserves_shape", true));
    }
}

void
NodeToolbar::edit_join()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->joinNodes();
    }
}

void
NodeToolbar::edit_break()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->breakNodes();
    }
}

void
NodeToolbar::edit_delete_segment()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->deleteSegments();
    }
}

void
NodeToolbar::edit_join_segment()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->joinSegments();
    }
}

void
NodeToolbar::edit_cusp()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_CUSP);
    }
}

void
NodeToolbar::edit_smooth()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_SMOOTH);
    }
}

void
NodeToolbar::edit_symmetrical()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_SYMMETRIC);
    }
}

void
NodeToolbar::edit_auto()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_AUTO);
    }
}

void
NodeToolbar::edit_toline()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setSegmentType(Inkscape::UI::SEGMENT_STRAIGHT);
    }
}

void
NodeToolbar::edit_tocurve()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setSegmentType(Inkscape::UI::SEGMENT_CUBIC_BEZIER);
    }
}

void
NodeToolbar::on_pref_toggled(Gtk::ToggleToolButton *item,
                             const Glib::ustring&   path)
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setBool(path, item->get_active());
}

}
}
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
