#ifndef __SP_SVG_VIEW_H__
#define __SP_SVG_VIEW_H__

/** \file
 * SPSVGView, SPSVGViewWidget: Generic SVG view and widget
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *
 * Copyright (C) 2001-2002 Lauris Kaplinski
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "ui/view/view.h"

class SPCanvasGroup;
class SPCanvasItem;


/**
 * Generic SVG view.
 */
class SPSVGView : public SPView {
    public:
	unsigned int _dkey;

	SPCanvasGroup *_parent;
	SPCanvasItem *_drawing;

	/// Horizontal and vertical scale
	gdouble _hscale, _vscale;
	/// Whether to rescale automatically
	bool _rescale, _keepaspect;
	gdouble _width, _height;


    SPSVGView (SPCanvasGroup* parent);
    ~SPSVGView();
        
    /// Rescales SPSVGView to given proportions.
    void setScale (gdouble hscale, gdouble vscale);
    
    /// Rescales SPSVGView and keeps aspect ratio.
    void setRescale (bool rescale, bool keepaspect, gdouble width, gdouble height);

    void doRescale (bool event);

    virtual void mouseover();
    virtual void mouseout();
    virtual bool shutdown() { return true; }

    private:
    
    virtual bool onShutdown() { return true; }
    virtual void onPositionSet (double, double) {}
    virtual void onResized (double, double) {}
    virtual void onRedrawRequested() {}
    virtual void onDocumentSet (SPDocument*);
    virtual void onStatusMessage (Inkscape::MessageType type, gchar const *message) {}
    virtual void onDocumentURISet (gchar const* uri) {}
    virtual void onDocumentResized (double, double);
};

#endif

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
