// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Desktop main menu bar code.
 */
/*
 * Authors:
 *   Tavmjong Bah       <tavmjong@free.fr>
 *   Alex Valavanis     <valavanisalex@gmail.com>
 *   Patrick Storz      <eduard.braun2@gmx.de>
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *   Kris De Gussem     <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2018 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 * Read the file 'COPYING' for more information.
 *
 */

#include <gtkmm.h>
#include <glibmm/i18n.h>

#include <iostream>

#include "inkscape.h"
#include "inkscape-application.h" // Open recent

#include "message-context.h"

#include "helper/action.h"
#include "helper/action-context.h"

#include "io/resource.h"    // UI File location

#include "object/sp-namedview.h"

#include <gtkmm/application.h>

#include "ui/icon-loader.h"
#include "inkscape-window.h"
#include "ui/shortcuts.h"
#include "ui/view/view.h"
#include "ui/uxmanager.h"   // To Do: Convert to actions

#ifdef GDK_WINDOWING_QUARTZ
#include <gtkosxapplication.h>
#endif

// =================== Main Menu ================
void
build_menu()
{
    std::string filename = Inkscape::IO::Resource::get_filename(Inkscape::IO::Resource::UIS, "menus.ui");
    auto refBuilder = Gtk::Builder::create();

    try
    {
        refBuilder->add_from_file(filename);
    }
    catch (const Glib::Error& err)
    {
        std::cerr << "build_menu: failed to load View menu from: "
                    << filename <<": "
                    << err.what() << std::endl;
    }
    
    const auto object = refBuilder->get_object("menus");
    const auto gmenu = Glib::RefPtr<Gio::Menu>::cast_dynamic(object);
    
    if (!gmenu) {
        std::cerr << "build_menu: failed to build View menu!" << std::endl;
    } else {

        InkscapeApplication::instance()->gtk_app()->set_menubar(gmenu); 

        { // Filters and Extensions
            static auto app = InkscapeApplication::instance();
            
            for (auto [ filter_id, submenu_name ] : app->get_action_effect_data().give_all_data()) 
            {
                auto [ filter_submenu,filter_name ] = submenu_name;
                auto sub_object = refBuilder->get_object(filter_submenu);
                auto sub_gmenu = Glib::RefPtr<Gio::Menu>::cast_dynamic(sub_object);
                sub_gmenu->append( filter_name, "app."+filter_id );        
            }

        }

        { // Recent file
            auto recent_manager = Gtk::RecentManager::get_default();
            auto recent_files = recent_manager->get_items(); // all recent files not necessarily inkscape only

            int max_files = Inkscape::Preferences::get()->getInt("/options/maxrecentdocuments/value");

            auto sub_object = refBuilder->get_object("recent-files");
            auto sub_gmenu = Glib::RefPtr<Gio::Menu>::cast_dynamic(sub_object);

            for (auto const &recent_file : recent_files) {
                // check if given was generated by inkscape
                bool valid_file = recent_file->has_application(g_get_prgname()) or
                                recent_file->has_application("org.inkscape.Inkscape") or
                                recent_file->has_application("inkscape") or
                                recent_file->has_application("inkscape.exe");

                valid_file = valid_file and recent_file->exists();

                if (not valid_file) {
                    continue;
                }

                if (max_files-- <= 0) {
                    break;
                }
                
                std::string action_name = "app.file-open-window('"+recent_file->get_uri_display()+"')";
                sub_gmenu->append(recent_file->get_short_name(),action_name);
            }
        }

    }
}

void
reload_menu()
{   
    build_menu();
}

void
build_menubar()
{
    build_menu();
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