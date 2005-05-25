#ifndef __TILEDIALOG_H__
#define __TILEDIALOG_H__
/*
 * A simple dialog for creating grid type arrangements of selected objects
 *
 * Authors:
 *   Bob Jamison ( based off trace dialog)
 *   John Cliff
 *   Other dudes from The Inkscape Organization
 *
 * Copyright (C) 2004 Bob Jamison
 * Copyright (C) 2004 John Cliff
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */


#include <gtkmm.h>
#include <gtkmm/box.h>
#include <gtkmm/dialog.h>
#include <gtkmm/button.h>

#include "ui/dialog/dialog.h"

namespace Inkscape {
namespace UI {
namespace Dialog {


/**
 * A dialog that displays log messages
 */
class TileDialog : public Dialog {

public:

    /**
     * Constructor
     */
    TileDialog() ;


    /**
     * Factory method
     */
    static TileDialog *create() { return new TileDialog(); }

    /**
     * Destructor
     */
    virtual ~TileDialog() {};

    /**
     * Do the actual work
     */
    void Grid_Arrange();

    /**
     * Respond to selection change
     */
    void TileDialog::updateSelection();


    /**
     * Callback from OK or Cancel
     */
    void responseCallback(int response_id);

    /**
     * Callback from spinbuttons
     */
    void on_row_spinbutton_changed();
    void on_col_spinbutton_changed();
    void on_xpad_spinbutton_changed();
    void on_ypad_spinbutton_changed();
    void on_RowSize_checkbutton_changed();
    void on_ColSize_checkbutton_changed();
    void on_rowSize_spinbutton_changed();
    void on_colSize_spinbutton_changed();
    void Spacing_button_changed();
    void VertAlign_changed();
    void HorizAlign_changed();


private:
    TileDialog(TileDialog const &d); // no copy
    void operator=(TileDialog const &d); // no assign

    bool userHidden;
    bool updating;



    Gtk::Notebook   notebook;
    Gtk::Tooltips   tips;

    Gtk::VBox             TileBox;
    Gtk::Button           *TileOkButton;
    Gtk::Button           *TileCancelButton;

    // Number selected label
    Gtk::Label            SelectionContentsLabel;


    Gtk::HBox             AlignHBox;
    Gtk::HBox             SpinsHBox;
    Gtk::HBox             SizesHBox;

    // Number per Row
    Gtk::VBox             NoOfColsBox;
    Gtk::Label            NoOfColsLabel;
    Gtk::SpinButton       NoOfColsSpinner;
    bool AutoRowSize;
    Gtk::CheckButton      RowHeightButton;

    Gtk::VBox             XByYLabelVBox;
    Gtk::Label            padXByYLabel;
    Gtk::Label            XByYLabel;

    // Number per Column
    Gtk::VBox             NoOfRowsBox;
    Gtk::Label            NoOfRowsLabel;
    Gtk::SpinButton       NoOfRowsSpinner;
    bool AutoColSize;
    Gtk::CheckButton      ColumnWidthButton;

    // Vertical align
    Gtk::Label            VertAlignLabel;
    Gtk::HBox             VertAlignHBox;
    Gtk::VBox             VertAlignVBox;
    Gtk::RadioButtonGroup VertAlignGroup;
    Gtk::RadioButton      VertCentreRadioButton;
    Gtk::RadioButton      VertTopRadioButton;
    Gtk::RadioButton      VertBotRadioButton;
    double VertAlign;

    // Horizontal align
    Gtk::Label            HorizAlignLabel;
    Gtk::VBox             HorizAlignVBox;
    Gtk::HBox             HorizAlignHBox;
    Gtk::RadioButtonGroup HorizAlignGroup;
    Gtk::RadioButton      HorizCentreRadioButton;
    Gtk::RadioButton      HorizLeftRadioButton;
    Gtk::RadioButton      HorizRightRadioButton;
    double HorizAlign;

    // padding in x
    Gtk::VBox             XPadBox;
    Gtk::Label            XPadLabel;
    Gtk::SpinButton       XPadSpinner;

    // padding in y
    Gtk::VBox             YPadBox;
    Gtk::Label            YPadLabel;
    Gtk::SpinButton       YPadSpinner;

    // BBox or manual spacing
    Gtk::VBox             SpacingVBox;
    Gtk::RadioButtonGroup SpacingGroup;
    Gtk::RadioButton      SpaceByBBoxRadioButton;
    Gtk::RadioButton      SpaceManualRadioButton;
    bool ManualSpacing;



    // Row height
    Gtk::VBox             RowHeightVBox;
    Gtk::HBox             RowHeightBox;
    Gtk::Label            RowHeightLabel;
    Gtk::SpinButton       RowHeightSpinner;

    // Column width
    Gtk::VBox             ColumnWidthVBox;
    Gtk::HBox             ColumnWidthBox;
    Gtk::Label            ColumnWidthLabel;
    Gtk::SpinButton       ColumnWidthSpinner;

};


} //namespace Dialog
} //namespace UI
} //namespace Inkscape


#endif /* __TILEDIALOG_H__ */

