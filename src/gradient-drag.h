#ifndef __GRADIENT_DRAG_H__
#define __GRADIENT_DRAG_H__

/*
 * On-canvas gradient dragging 
 *
 * Authors:
 *   bulia byak <bulia@users.sf.net>
 *
 * Copyright (C) 2005 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <glib/gslist.h>
#include <sigc++/sigc++.h>
#include <vector>

#include <forward.h>
#include <knot-enums.h>

struct SPItem;
struct SPKnot;
namespace NR {
class Point;
}

/**
This class represents a single draggable point of a gradient. It remembers the item
which has the gradient, whether it's fill or stroke, and the point number (from the
GrPoint enum).
*/
struct GrDraggable {
	GrDraggable(SPItem *item, guint point_num, bool fill_or_stroke);
	~GrDraggable();

	SPItem *item;
	guint point_num;
	bool fill_or_stroke;

	bool mayMerge (GrDraggable *da2);

    inline int equals (GrDraggable *other) {
		return ((item == other->item) && (point_num == other->point_num) && (fill_or_stroke == other->fill_or_stroke));
    }
};

struct GrDrag;

/**
This class holds together a visible on-canvas knot and a list of draggables that need to
be moved when the knot moves. Normally there's one draggable in the list, but there may
be more when draggers are snapped together.
*/
struct GrDragger {
	GrDragger (GrDrag *parent, NR::Point p, GrDraggable *draggable);
	~GrDragger();

	GrDrag *parent;

	SPKnot *knot;

	// position of the knot, desktop coords
	NR::Point point;
	// position of the knot before it began to drag; updated when released
	NR::Point point_original;

	/** Connection to \a knot's "moved" signal, for blocking it (unused?). */
	guint   handler_id;

	GSList *draggables;

	void addDraggable(GrDraggable *draggable);

	void updateKnotShape();
	void updateTip();

	void moveThisToDraggable (SPItem *item, guint point_num, bool fill_or_stroke, bool write_repr);
	void moveOtherToDraggable (SPItem *item, guint point_num, bool fill_or_stroke, bool write_repr);
	void updateDependencies (bool write_repr);

	bool mayMerge (GrDragger *other);
	bool mayMerge (GrDraggable *da2);

	bool isA (guint point_num);
	bool isA (SPItem *item, guint point_num, bool fill_or_stroke);

	void fireDraggables (bool write_repr);
};

/**
This is the root class of the gradient dragging machinery. It holds lists of GrDraggers
and of lines (simple canvas items). It also remembers one of the draggers as selected.
*/
struct GrDrag {
	GrDrag(SPDesktop *desktop);
	~GrDrag();

	void addLine (NR::Point p1, NR::Point p2);

	void addDragger (GrDraggable *draggable);

	void addDraggersRadial (SPRadialGradient *rg, SPItem *item, bool fill_or_stroke);
	void addDraggersLinear (SPLinearGradient *lg, SPItem *item, bool fill_or_stroke);

	GrDragger *selected;
	void setSelected (GrDragger *dragger);

	GrDragger *getDraggerFor (SPItem *item, guint point_num, bool fill_or_stroke);

	bool local_change;

	SPDesktop *desktop;
	SPSelection *selection;
	sigc::connection sel_changed_connection;
	sigc::connection sel_modified_connection;

	// lists of edges of selection bboxes, to snap draggers to
	std::vector<double> hor_levels;
	std::vector<double> vert_levels;

	GSList *draggers;
	GSList *lines;

	void updateDraggers ();
	void updateLines ();
	void updateLevels ();
};

#endif
