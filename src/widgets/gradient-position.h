#ifndef __SP_GRADIENT_POSITION_H__
#define __SP_GRADIENT_POSITION_H__

/*
 * Gradient positioning widget
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2001-2002 Lauris Kaplinski
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <glib.h>



#define SP_TYPE_GRADIENT_POSITION (sp_gradient_position_get_type ())
#define SP_GRADIENT_POSITION(o) (GTK_CHECK_CAST ((o), SP_TYPE_GRADIENT_POSITION, SPGradientPosition))
#define SP_GRADIENT_POSITION_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), SP_TYPE_GRADIENT_POSITION, SPGradientPositionClass))
#define SP_IS_GRADIENT_POSITION(o) (GTK_CHECK_TYPE ((o), SP_TYPE_GRADIENT_POSITION))
#define SP_IS_GRADIENT_POSITION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), SP_TYPE_GRADIENT_POSITION))

struct SPGradientPosition;
struct SPGradientPositionClass;

#include <libnr/nr-gradient.h>
#include <gtk/gtkwidget.h>
#include "display/nr-gradient-gpl.h"
#include "sp-gradient.h"

enum {
	SP_GRADIENT_POSITION_MODE_LINEAR,
	SP_GRADIENT_POSITION_MODE_RADIAL
};

class SPGPLGData {
 public:
	NR::Point start, end;
};

class SPGPRGData {
 public:
	NR::Point center, f;
	double r;
};

struct SPGradientPosition {
	GtkWidget widget;
	guint need_update : 1;
	guint dragging; /* 0 for no dragging, or the index of the node
			 * being dragged. */
	guint position : 2;
	guint mode : 1;
	guint changed : 1;
	SPGradient *gradient;
	NRRectL vbox; /* BBox in widget coordinates */
	GdkGC *gc;
	GdkPixmap *px;

	/* Spread type from libnr */
	guint spread : 2;

	unsigned char *cv;
	union {
		NRLGradientRenderer lgr;
		NRRGradientRenderer rgr;
	} renderer;

	NRMatrix gs2d;

	SPGPLGData linear;
	SPGPRGData radial;

	/* Gradiented bbox in document coordinates */
	NRRect bbox;
	/* Window in document coordinates */
	NRRect wbox;
	/* Window <-> document transformation */
	NRMatrix w2d;
	NRMatrix d2w;
	NRMatrix w2gs;
	NRMatrix gs2w;
};

struct SPGradientPositionClass {
	GtkWidgetClass parent_class;

	void (* grabbed) (SPGradientPosition *pos);
	void (* dragged) (SPGradientPosition *pos);
	void (* released) (SPGradientPosition *pos);
	void (* changed) (SPGradientPosition *pos);

};

GtkType sp_gradient_position_get_type (void);

GtkWidget *sp_gradient_position_new (SPGradient *gradient);

void sp_gradient_position_set_gradient (SPGradientPosition *pos, SPGradient *gradient);

void sp_gradient_position_set_mode (SPGradientPosition *pos, guint mode);
void sp_gradient_position_set_bbox (SPGradientPosition *pos, gdouble x0, gdouble y0, gdouble x1, gdouble y1);

void sp_gradient_position_set_gs2d_matrix_f (SPGradientPosition *pos, NRMatrix *gs2d);
void sp_gradient_position_get_gs2d_matrix_f (SPGradientPosition *pos, NRMatrix *gs2d);

void sp_gradient_position_set_linear_position (SPGradientPosition *pos, float x1, float y1, float x2, float y2);
void sp_gradient_position_set_radial_position (SPGradientPosition *pos, float cx, float cy, float fx, float fy, float r);

void sp_gradient_position_set_spread (SPGradientPosition *pos, unsigned int spread);

void sp_gradient_position_get_linear_position_floatv (SPGradientPosition *gp, float *pos);
void sp_gradient_position_get_radial_position_floatv (SPGradientPosition *gp, float *pos);




#endif
