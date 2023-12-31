// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief A dialog for the about screen
 *
 * Copyright (C) Martin Owens 2019 <doctormo@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "startup.h"

#include <fstream>
#include <glibmm/i18n.h>
#include <streambuf>
#include <string>
#include <limits>

#include "color-rgba.h"
#include "file.h"
#include "inkscape-application.h"
#include "inkscape-version-info.h"
#include "inkscape-version.h"
#include "inkscape.h"
#include "io/resource.h"
#include "object/sp-namedview.h"
#include "preferences.h"
#include "ui/dialog/filedialog.h"
#include "ui/shortcuts.h"
#include "ui/themes.h"
#include "ui/util.h"
#include "ui/widget/template-list.h"
#include "util/units.h"

using namespace Inkscape::IO;
using namespace Inkscape::UI::View;
using Inkscape::Util::unit_table;

namespace Inkscape {
namespace UI {
namespace Dialog {

class NameIdCols: public Gtk::TreeModel::ColumnRecord {
    public:
        // These types must match those for the model in the .glade file
        NameIdCols() {
            this->add(this->col_name);
            this->add(this->col_id);
        }
        Gtk::TreeModelColumn<Glib::ustring> col_name;
        Gtk::TreeModelColumn<Glib::ustring> col_id;
};

class RecentCols: public Gtk::TreeModel::ColumnRecord {
    public:
        // These types must match those for the model in the .glade file
        RecentCols() {
            this->add(this->col_name);
            this->add(this->col_id);
            this->add(this->col_dt);
            this->add(this->col_crash);
        }
        Gtk::TreeModelColumn<Glib::ustring> col_name;
        Gtk::TreeModelColumn<Glib::ustring> col_id;
        Gtk::TreeModelColumn<gint64> col_dt;
        Gtk::TreeModelColumn<bool> col_crash;
};

class CanvasCols: public Gtk::TreeModel::ColumnRecord {
    public:
        // These types must match those for the model in the .glade file
        CanvasCols() {
            this->add(this->id);
            this->add(this->name);
            this->add(this->icon_filename);
            this->add(this->pagecolor);
            this->add(this->checkered);
            this->add(this->bordercolor);
            this->add(this->shadow);
            this->add(this->deskcolor);
        }
        Gtk::TreeModelColumn<Glib::ustring> id;
        Gtk::TreeModelColumn<Glib::ustring> name;
        Gtk::TreeModelColumn<Glib::ustring> icon_filename;
        Gtk::TreeModelColumn<Glib::ustring> pagecolor;
        Gtk::TreeModelColumn<bool> checkered;
        Gtk::TreeModelColumn<Glib::ustring> bordercolor;
        Gtk::TreeModelColumn<bool> shadow;
        Gtk::TreeModelColumn<Glib::ustring> deskcolor;
};

class ThemeCols: public Gtk::TreeModel::ColumnRecord {
    public:
        // These types must match those for the model in the .glade file
        ThemeCols() {
            this->add(this->id);
            this->add(this->name);
            this->add(this->theme);
            this->add(this->icons);
            this->add(this->base);
            this->add(this->base_dark);
            this->add(this->success);
            this->add(this->warn);
            this->add(this->error);
            this->add(this->symbolic);
            this->add(this->smallicons);
            this->add(this->enabled);
        }
        Gtk::TreeModelColumn<Glib::ustring> id;
        Gtk::TreeModelColumn<Glib::ustring> name;
        Gtk::TreeModelColumn<Glib::ustring> theme;
        Gtk::TreeModelColumn<Glib::ustring> icons;
        Gtk::TreeModelColumn<Glib::ustring> base;
        Gtk::TreeModelColumn<Glib::ustring> base_dark;
        Gtk::TreeModelColumn<Glib::ustring> success;
        Gtk::TreeModelColumn<Glib::ustring> warn;
        Gtk::TreeModelColumn<Glib::ustring> error;
        Gtk::TreeModelColumn<bool> symbolic;
        Gtk::TreeModelColumn<bool> smallicons;
        Gtk::TreeModelColumn<bool> enabled;
};

/**
 * Color is store as a string in the form #RRGGBBAA, '0' means "unset"
 *
 * @param color - The string color from glade.
 */
unsigned int get_color_value(const Glib::ustring color)
{
    Gdk::RGBA gdk_color = Gdk::RGBA(color);
    ColorRGBA  sp_color(gdk_color.get_red(), gdk_color.get_green(),
                        gdk_color.get_blue(), gdk_color.get_alpha());
    return sp_color.getIntValue();
}

StartScreen::StartScreen()
    : Gtk::Dialog()
{
    set_can_focus(true);
    grab_focus();
    set_can_default(true);
    grab_default();
    set_urgency_hint(true);  // Draw user's attention to this window!
    set_modal(true);
    set_position(Gtk::WIN_POS_CENTER_ALWAYS);
    set_default_size(700, 360);

    Glib::ustring gladefile = Resource::get_filename(Resource::UIS, "inkscape-start.glade");

    try {
        builder = Gtk::Builder::create_from_file(gladefile);
    } catch (const Glib::Error &ex) {
        g_error("Glade file loading failed for boot screen");
        // cleanup?
    }

    // Get window from Glade file.
    builder->get_widget("start-screen-window", window);
    set_name("start-screen-window");
    set_title(Inkscape::inkscape_version());

    // Get references to various widgets used globally.
    builder->get_widget("tabs", tabs);
    builder->get_widget_derived("kinds", templates);
    builder->get_widget("banner", banners);
    builder->get_widget("themes", themes);
    builder->get_widget("recent_treeview", recent_treeview);

    // Populate with template extensions
    templates->init(Inkscape::Extension::TEMPLATE_NEW_WELCOME);

    // Get references to various widget used locally. (In order of appearance.)
    Gtk::ComboBox* canvas = nullptr;
    Gtk::ComboBox* keys = nullptr;
    Gtk::Button* save = nullptr;
    Gtk::Button* thanks = nullptr;
    Gtk::Button* close_btn = nullptr;
    Gtk::Button* new_btn = nullptr;
    Gtk::Button* show_toggle = nullptr;
    Gtk::Switch* dark_toggle = nullptr;
    builder->get_widget("canvas", canvas);
    builder->get_widget("keys", keys);
    builder->get_widget("save", save);
    builder->get_widget("thanks", thanks);
    builder->get_widget("show_toggle", show_toggle);
    builder->get_widget("dark_toggle", dark_toggle);
    builder->get_widget("load", load_btn);
    builder->get_widget("new", new_btn);
    builder->get_widget("close_window", close_btn);

    // Unparent to move to our dialog window.
    auto parent = banners->get_parent();
    parent->remove(*banners);
    parent->remove(*tabs);

    // Add signals and setup things.
    auto prefs = Inkscape::Preferences::get();

    tabs->signal_switch_page().connect(sigc::mem_fun(*this, &StartScreen::notebook_switch));

    // Setup the lists of items
    enlist_recent_files();
    enlist_keys();
    filter_themes();
    set_active_combo("themes", prefs->getString("/options/boot/theme"));
    set_active_combo("canvas", prefs->getString("/options/boot/canvas"));

    // initialise dark depending on prefs and background
    refresh_dark_switch();

    // Welcome! tab
    std::string welcome_text_file = Resource::get_filename_string(Resource::SCREENS, "start-welcome-text.svg", true);
    Gtk::Image *welcome_text;
    builder->get_widget("welcome_text", welcome_text);
    welcome_text->set(welcome_text_file);
    
    canvas->signal_changed().connect(sigc::mem_fun(*this, &StartScreen::canvas_changed));
    keys->signal_changed().connect(sigc::mem_fun(*this, &StartScreen::keyboard_changed));
    themes->signal_changed().connect(sigc::mem_fun(*this, &StartScreen::theme_changed));
    dark_toggle->property_active().signal_changed().connect(sigc::mem_fun(*this, &StartScreen::theme_changed));
    save->signal_clicked().connect(sigc::bind<Gtk::Button *>(sigc::mem_fun(*this, &StartScreen::notebook_next), save));

    // "Supported by You" tab
    thanks->signal_clicked().connect(sigc::bind<Gtk::Button *>(sigc::mem_fun(*this, &StartScreen::notebook_next), thanks));

    // "Time to Draw" tab
    recent_treeview->signal_row_activated().connect(sigc::hide(sigc::hide((sigc::mem_fun(*this, &StartScreen::load_document)))));
    recent_treeview->get_selection()->signal_changed().connect(sigc::mem_fun(*this, &StartScreen::on_recent_changed));
    templates->signal_switch_page().connect(sigc::mem_fun(*this, &StartScreen::on_kind_changed));
    load_btn->set_sensitive(true);

    show_toggle->signal_clicked().connect(sigc::mem_fun(*this, &StartScreen::show_toggle));
    load_btn->signal_clicked().connect(sigc::mem_fun(*this, &StartScreen::load_document));
    templates->connectItemSelected(sigc::mem_fun(*this, &StartScreen::new_document));
    new_btn->signal_clicked().connect(sigc::mem_fun(*this, &StartScreen::new_document));
    close_btn->signal_clicked().connect([=] { response(GTK_RESPONSE_CANCEL); });

    // Reparent to our dialog window
    set_titlebar(*banners);
    Gtk::Box* box = get_content_area();
    box->add(*tabs);

    // Show the first tab ONLY on the first run for this version
    std::string opt_shown = "/options/boot/shown/ver";
    opt_shown += Inkscape::version_string_without_revision;
    if (!prefs->getBool(opt_shown, false)) {
        theme_changed();
        tabs->set_current_page(0);
        prefs->setBool(opt_shown, true);
    } else {
        tabs->set_current_page(2);
        notebook_switch(nullptr, 2);
    }

    set_modal(true);
    set_position(Gtk::WIN_POS_CENTER_ALWAYS);
    property_resizable() = false;
    set_default_size(700, 360);
    show();
}

StartScreen::~StartScreen()
{
    // These are "owned" by builder... don't delete them!
    banners->get_parent()->remove(*banners);
    tabs->get_parent()->remove(*tabs);
}

/**
 * Return the active row of the named combo box.
 *
 * @param widget_name - The name of the widget in the glade file
 * @return Gtk Row object ready for use.
 * @throws Three errors depending on where it failed.
 */
Gtk::TreeModel::Row
StartScreen::active_combo(std::string widget_name)
{
    Gtk::ComboBox *combo;
    builder->get_widget(widget_name, combo);
    if (!combo) throw 1;
    Gtk::TreeModel::iterator iter = combo->get_active();
    if (!iter) throw 2;
    Gtk::TreeModel::Row row = *iter;
    if (!row) throw 3;
    return row;
}

/**
 * Set the active item in the combo based on the unique_id (column set in glade)
 *
 * @param widget_name - The name of the widget in the glade file
 * @param unique_id - The column id to activate, sets to first item if blank.
 */
void
StartScreen::set_active_combo(std::string widget_name, std::string unique_id)
{
    Gtk::ComboBox *combo;
    builder->get_widget(widget_name, combo);
    if (combo) {
        if (unique_id.empty()) {
            combo->set_active(0); // Select the first
        } else if(!combo->set_active_id(unique_id)) {
            combo->set_active(-1); // Select nothing
        }
    }
}

/**
 * When a notbook is switched, reveal the right banner image (gtk signal).
 */
void
StartScreen::notebook_switch(Gtk::Widget *tab, guint page_num)
{
    int page = 0;
    for (auto banner : banners->get_children()) {
        if (auto revealer = dynamic_cast<Gtk::Revealer *>(banner)) {
            revealer->set_reveal_child(page == page_num);
            page++;
        }
    }
}

void
StartScreen::enlist_recent_files()
{
    RecentCols cols;
    if (!recent_treeview) return;
    // We're not sure why we have to ask C for the TreeStore object
    auto store = Glib::wrap(GTK_LIST_STORE(gtk_tree_view_get_model(recent_treeview->gobj())));
    store->clear();
    // Now sort the result by visited time
    store->set_sort_column(cols.col_dt, Gtk::SORT_DESCENDING);

    // Open [other]
    Gtk::TreeModel::Row first_row = *(store->append());
    first_row[cols.col_name] = _("Browse for other files...");
    first_row[cols.col_id] = "";
    first_row[cols.col_dt] = std::numeric_limits<gint64>::max();
    recent_treeview->get_selection()->select(store->get_path(first_row));

    Glib::RefPtr<Gtk::RecentManager> manager = Gtk::RecentManager::get_default();
    for (auto item : manager->get_items()) {
        if (item->has_application(g_get_prgname())
            || item->has_application("org.inkscape.Inkscape")
            || item->has_application("inkscape")
            || item->has_application("inkscape.exe")
           ) {
            // This uri is a GVFS uri, so parse it with that or it will fail.
            auto file = Gio::File::create_for_uri(item->get_uri());
            std::string path = file->get_path();
            if (!path.empty() && Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR)
                && item->get_mime_type() == "image/svg+xml") {
                Gtk::TreeModel::Row row = *(store->append());
                row[cols.col_name] = item->get_display_name();
                row[cols.col_id] = item->get_uri();
                row[cols.col_dt] = item->get_modified();
                row[cols.col_crash] = item->has_group("Crash");
            }
        }
    }

}

/**
 * Called when a new recent document is selected.
 */
void
StartScreen::on_recent_changed()
{
    // TODO: In the future this is where previews and other information can be loaded.
}

/**
 * Called when the left side tabs are changed.
 */
void
StartScreen::on_kind_changed(Gtk::Widget *tab, guint page_num)
{
    if (page_num == 0) {
        load_btn->show();
    } else {
        load_btn->hide();
    }
}

/**
 * Called when new button clicked or template is double clicked, or escape pressed.
 */
void
StartScreen::new_document()
{
    // Generate a new document from the selected template.
    _document = templates->new_document();
    if (_document) {
    // Quit welcome screen if options not 'canceled'
        response(GTK_RESPONSE_APPLY);
    }
}

/**
 * Called when load button clicked.
 */
void
StartScreen::load_document()
{
    RecentCols cols;
    auto prefs = Inkscape::Preferences::get();
    auto app = InkscapeApplication::instance();

    if (!recent_treeview)
        return;

    auto iter = recent_treeview->get_selection()->get_selected();
    if (iter) {
        Gtk::TreeModel::Row row = *iter;
        if (row) {
            Glib::ustring _file = row[cols.col_id];
            Glib::RefPtr<Gio::File> file;

            if (!_file.empty()) {
                file = Gio::File::create_for_uri(_file);
            } else {
                Glib::ustring open_path = prefs->getString("/dialogs/open/path");
                if (open_path.empty()) {
                    open_path = g_get_home_dir();
                    open_path.append(G_DIR_SEPARATOR_S);
                }

                // Browse for file instead
                auto browser = Inkscape::UI::Dialog::FileOpenDialog::create(
                    *this, open_path, Inkscape::UI::Dialog::SVG_TYPES, _("Open a different file"));

                if (browser->show()) {
                    prefs->setString("/dialogs/open/path", browser->getCurrentDirectory());
                    file = Gio::File::create_for_path(browser->getFilename());
                    delete browser;
                } else {
                    delete browser;
                    return; // Cancel
                }
            }

            // Now we have filename, open document.
            bool canceled = false;
            _document = app->document_open(file, &canceled);

            if (!canceled && _document) {
                // We're done, hand back to app.
                response(GTK_RESPONSE_OK);
            }
        }
    }
}

/**
 * When a button needs to go to the next notebook page.
 */
void
StartScreen::notebook_next(Gtk::Widget *button)
{
    int page = tabs->get_current_page();
    if (page == 2) {
        response(GTK_RESPONSE_CANCEL); // Only occurs from keypress.
    } else {
        tabs->set_current_page(page + 1);
    }
}

/**
 * When a key is pressed in the main window.
 */
bool
StartScreen::on_key_press_event(GdkEventKey* event)
{
#ifdef GDK_WINDOWING_QUARTZ
    // On macOS only, if user press Cmd+Q => exit
    if (event->keyval == 'q' && event->state == (GDK_MOD2_MASK | GDK_META_MASK)) {
        close();
        return false;
    }
#endif
    switch (event->keyval) {
        case GDK_KEY_Escape:
            // Prevent loading any selected items
            response(GTK_RESPONSE_CANCEL);
            return true;
        case GDK_KEY_Return:
            notebook_next(nullptr);
            return true;
    }

    return false;
}

void
StartScreen::on_response(int response_id)
{
    if (response_id == GTK_RESPONSE_DELETE_EVENT) {
        // Don't open a window for force closing.
        return;
    }
    if (response_id == GTK_RESPONSE_CANCEL) {
        templates->reset_selection();
    }
    if (response_id != GTK_RESPONSE_OK && !_document) {
        // Last ditch attempt to generate a new document while exiting.
        _document = templates->new_document();
    }
}

void
StartScreen::show_toggle()
{
    Gtk::ToggleButton *button;
    builder->get_widget("show_toggle", button);
    if (button) {
        auto prefs = Inkscape::Preferences::get();
        prefs->setBool("/options/boot/enabled", button->get_active());
    } else {
        g_warning("Can't find toggle button widget.");
    }
}

/**
 * Refresh theme in-place so user can see a semi-preview. This theme selection
 * is not meant to be perfect, but hint to the user that they can set the
 * theme if they want.
 *
 * @param theme_name - The name of the theme to load.
 */
void
StartScreen::refresh_theme(Glib::ustring theme_name)
{
    auto const screen = Gdk::Screen::get_default();
    if (INKSCAPE.themecontext->getContrastThemeProvider()) {
        Gtk::StyleContext::remove_provider_for_screen(screen, INKSCAPE.themecontext->getContrastThemeProvider());
    }
    auto settings = Gtk::Settings::get_default();

    auto prefs = Inkscape::Preferences::get();

    settings->property_gtk_theme_name() = theme_name;
    settings->property_gtk_application_prefer_dark_theme() = prefs->getBool("/theme/preferDarkTheme", true);
    settings->property_gtk_icon_theme_name() = prefs->getString("/theme/iconTheme", prefs->getString("/theme/defaultIconTheme", ""));

    if (prefs->getBool("/theme/symbolicIcons", false)) {
        get_style_context()->add_class("symbolic");
        get_style_context()->remove_class("regular");
    } else {
        get_style_context()->add_class("regular");
        get_style_context()->remove_class("symbolic");
    }

    if (INKSCAPE.themecontext->getColorizeProvider()) {
        Gtk::StyleContext::remove_provider_for_screen(screen, INKSCAPE.themecontext->getColorizeProvider());
    }
    if (!prefs->getBool("/theme/symbolicDefaultHighColors", false)) {
        Gtk::CssProvider::create();
        Glib::ustring css_str = INKSCAPE.themecontext->get_symbolic_colors();
        try {
            INKSCAPE.themecontext->getColorizeProvider()->load_from_data(css_str);
        } catch (const Gtk::CssProviderError &ex) {
            g_critical("CSSProviderError::load_from_data(): failed to load '%s'\n(%s)", css_str.c_str(), ex.what().c_str());
        }
        Gtk::StyleContext::add_provider_for_screen(screen, INKSCAPE.themecontext->getColorizeProvider(),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    // set dark switch and disable if there is no prefer option for dark
    refresh_dark_switch();

    INKSCAPE.themecontext->getChangeThemeSignal().emit();
}

/**
 * Set the theme, icon pack and other theme options from a set defined
 * in the glade file. The combo box has a number of columns with the needed
 * data describing how to set up the theme.
 */
void
StartScreen::theme_changed()
{
    auto prefs = Inkscape::Preferences::get();

    ThemeCols cols;
    try {
        auto row = active_combo("themes");
        Glib::ustring theme_id = row[cols.id];
        if (theme_id == "custom") return;
        prefs->setString("/options/boot/theme", row[cols.id]);

        // Update theme from combo.
        Glib::ustring icons = row[cols.icons];
        prefs->setBool("/toolbox/tools/small", row[cols.smallicons]);
        prefs->setString("/theme/gtkTheme", row[cols.theme]);
        prefs->setString("/theme/iconTheme", icons);
        prefs->setBool("/theme/symbolicIcons", row[cols.symbolic]);

        Gtk::Switch* dark_toggle = nullptr;
        builder->get_widget("dark_toggle", dark_toggle);
        bool is_dark = dark_toggle->get_active();
        prefs->setBool("/theme/preferDarkTheme", is_dark);
        prefs->setBool("/theme/darkTheme", is_dark);
        // Symbolic icon colours
        if (get_color_value(row[cols.base]) == 0) {
            prefs->setBool("/theme/symbolicDefaultBaseColors", true);
            prefs->setBool("/theme/symbolicDefaultHighColors", true);
        } else {
            Glib::ustring prefix = "/theme/" + icons;
            prefs->setBool("/theme/symbolicDefaultBaseColors", false);
            prefs->setBool("/theme/symbolicDefaultHighColors", false);
            if (is_dark) {
                prefs->setUInt(prefix + "/symbolicBaseColor", get_color_value(row[cols.base_dark]));
            } else {
                prefs->setUInt(prefix + "/symbolicBaseColor", get_color_value(row[cols.base]));
            }
            prefs->setUInt(prefix + "/symbolicSuccessColor", get_color_value(row[cols.success]));
            prefs->setUInt(prefix + "/symbolicWarningColor", get_color_value(row[cols.warn]));
            prefs->setUInt(prefix + "/symbolicErrorColor", get_color_value(row[cols.error]));
        }

        refresh_theme(prefs->getString("/theme/gtkTheme", prefs->getString("/theme/defaultGtkTheme", "")));
    } catch(int e) {
        g_warning("Couldn't find theme value.");
    }
}

/**
 * Called when the canvas dropdown changes.
 */
void
StartScreen::canvas_changed()
{
    CanvasCols cols;
    try {
        auto row = active_combo("canvas");

        auto prefs = Inkscape::Preferences::get();
        prefs->setString("/options/boot/canvas", row[cols.id]);

        Gdk::RGBA gdk_color = Gdk::RGBA(row[cols.pagecolor]);
        SPColor sp_color(gdk_color.get_red(), gdk_color.get_green(), gdk_color.get_blue());
        prefs->setString("/template/base/pagecolor", sp_color.toString());
        prefs->setDouble("/template/base/pageopacity", gdk_color.get_alpha());

        Gdk::RGBA gdk_border = Gdk::RGBA(row[cols.bordercolor]);
        SPColor sp_border(gdk_border.get_red(), gdk_border.get_green(), gdk_border.get_blue());
        prefs->setString("/template/base/bordercolor", sp_border.toString());
        prefs->setDouble("/template/base/borderopacity", gdk_border.get_alpha());

        prefs->setBool("/template/base/pagecheckerboard", row[cols.checkered]);
        prefs->setInt("/template/base/pageshadow", row[cols.shadow] ? 2 : 0);

        Gdk::RGBA gdk_desk = Gdk::RGBA(row[cols.deskcolor]);
        SPColor sp_desk(gdk_desk.get_red(), gdk_desk.get_green(), gdk_desk.get_blue());
        prefs->setString("/template/base/deskcolor", sp_desk.toString());
    } catch(int e) {
        g_warning("Couldn't find canvas value.");
    }
}

void
StartScreen::filter_themes()
{
    ThemeCols cols;
    // We need to disable themes which aren't available.
    auto store = Glib::wrap(GTK_LIST_STORE(gtk_combo_box_get_model(themes->gobj())));
    auto available = INKSCAPE.themecontext->get_available_themes();

    // Detect use of custom theme here, detect defaults used in many systems.
    auto settings = Gtk::Settings::get_default();
    Glib::ustring theme_name = settings->property_gtk_theme_name();
    Glib::ustring icons_name = settings->property_gtk_icon_theme_name();

    bool has_system_theme = false;
    if (theme_name != "Adwaita" || icons_name != "hicolor") {
        has_system_theme = true;
        /* Enable if/when we want custom to be the default.
        if (prefs->getString("/options/boot/theme").empty()) {
            prefs->setString("/options/boot/theme", "system")
            theme_changed();
        }*/
    }

    for(auto row : store->children()) {
        Glib::ustring theme = row[cols.theme];
        if (!row[cols.enabled]) {
            // Available themes; We only "enable" them, we don't disable them.
            row[cols.enabled] = available.find(theme) != available.end();
        } else if(row[cols.id] == "system" && !has_system_theme) {
            // Disable system theme option if not available.
            row[cols.enabled] = false;
        }
    }
}

void
StartScreen::enlist_keys()
{
    NameIdCols cols;
    Gtk::ComboBox *keys;
    builder->get_widget("keys", keys);
    if (!keys) return;

    auto store = Glib::wrap(GTK_LIST_STORE(gtk_combo_box_get_model(keys->gobj())));
    store->clear();

    for(auto item: Inkscape::Shortcuts::get_file_names()){
        Gtk::TreeModel::Row row = *(store->append());
        row[cols.col_name] = item.first;
        row[cols.col_id] = item.second;
    }

    auto prefs = Inkscape::Preferences::get();
    auto current = prefs->getString("/options/kbshortcuts/shortcutfile");
    if (current.empty()) {
        current = "inkscape.xml";
    }
    keys->set_active_id(current);
}

/**
 * Set the keys file based on the keys set in the enlist above
 */
void
StartScreen::keyboard_changed()
{
    NameIdCols cols;
    try {
        auto row = active_combo("keys");
        auto prefs = Inkscape::Preferences::get();
        Glib::ustring set_to = row[cols.col_id];
        prefs->setString("/options/kbshortcuts/shortcutfile", set_to);
        Inkscape::Shortcuts::getInstance().init();

        Gtk::InfoBar* keys_warning;
        builder->get_widget("keys_warning", keys_warning);
        if (set_to != "inkscape.xml" && set_to != "default.xml") {
            keys_warning->set_message_type(Gtk::MessageType::MESSAGE_WARNING);
            keys_warning->show();
        } else {
            keys_warning->hide();
        }
    } catch(int e) {
        g_warning("Couldn't find keys value.");
    }
}

/**
 * Set Dark Switch based on current selected theme.
 * We will disable switch if current theme doesn't have prefer dark theme option.
 */

void StartScreen::refresh_dark_switch()
{
    auto prefs = Inkscape::Preferences::get();

    Gtk::Container *window = dynamic_cast<Gtk::Container *>(get_toplevel());
    bool dark = INKSCAPE.themecontext->isCurrentThemeDark(window);
    prefs->setBool("/theme/preferDarkTheme", dark);
    prefs->setBool("/theme/darkTheme", dark);

    auto themes = INKSCAPE.themecontext->get_available_themes();
    Glib::ustring current_theme = prefs->getString("/theme/gtkTheme", prefs->getString("/theme/defaultGtkTheme", ""));

    Gtk::Switch *dark_toggle = nullptr;
    builder->get_widget("dark_toggle", dark_toggle);

    if (!themes[current_theme]) {
        dark_toggle->set_sensitive(false);
    } else {
        dark_toggle->set_sensitive(true);
    }
    dark_toggle->set_active(dark);
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
