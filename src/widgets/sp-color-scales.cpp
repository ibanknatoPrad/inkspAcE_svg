#include <config.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktable.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkcolorsel.h>
#include "../color.h"
#include "../helper/sp-intl.h"
#include "../dialogs/dialog-events.h"
#include "sp-color-preview.h"
#include "sp-color-scales.h"

#define CSC_CHANNEL_R (1 << 0)
#define CSC_CHANNEL_G (1 << 1)
#define CSC_CHANNEL_B (1 << 2)
#define CSC_CHANNEL_A (1 << 3)
#define CSC_CHANNEL_H (1 << 0)
#define CSC_CHANNEL_S (1 << 1)
#define CSC_CHANNEL_V (1 << 2)
#define CSC_CHANNEL_C (1 << 0)
#define CSC_CHANNEL_M (1 << 1)
#define CSC_CHANNEL_Y (1 << 2)
#define CSC_CHANNEL_K (1 << 3)
#define CSC_CHANNEL_CMYKA (1 << 4)

#define CSC_CHANNELS_ALL 0


G_BEGIN_DECLS

static void sp_color_scales_class_init (SPColorScalesClass *klass);
static void sp_color_scales_init (SPColorScales *cs);
static void sp_color_scales_destroy (GtkObject *object);

static void sp_color_scales_show_all (GtkWidget *widget);
static void sp_color_scales_hide_all (GtkWidget *widget);

static const gchar *sp_color_scales_hue_map (void);

G_END_DECLS

static SPColorSelectorClass *parent_class;

#define XPAD 4
#define YPAD 1

GType
sp_color_scales_get_type (void)
{
	static GType type = 0;
	if (!type) {
		static const GTypeInfo info = {
			sizeof (SPColorScalesClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) sp_color_scales_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (SPColorScales),
			0,	  /* n_preallocs */
			(GInstanceInitFunc) sp_color_scales_init,
			NULL
		};

		type = g_type_register_static (SP_TYPE_COLOR_SELECTOR,
									   "SPColorScales",
									   &info,
									   static_cast< GTypeFlags > (0) );
	}
	return type;
}

static void
sp_color_scales_class_init (SPColorScalesClass *klass)
{
	static const gchar* nameset[] = {N_("RGB"), N_("HSV"), N_("CMYK"), 0};
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	SPColorSelectorClass *selector_class;

	object_class = (GtkObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;
	selector_class = SP_COLOR_SELECTOR_CLASS (klass);

	parent_class = SP_COLOR_SELECTOR_CLASS (g_type_class_peek_parent (klass));

	selector_class->name = nameset;
	selector_class->submode_count = 3;

	object_class->destroy = sp_color_scales_destroy;

	widget_class->show_all = sp_color_scales_show_all;
	widget_class->hide_all = sp_color_scales_hide_all;
}

ColorScales::ColorScales( SPColorSelector* csel )
    : ColorSelector( csel ),
      _mode( SP_COLOR_SCALES_MODE_NONE ),
      _updating( FALSE ),
      _dragging( FALSE )
{
    gint i = 0;

    for (i = 0; i < 5; i++) {
        _l[i] = 0;
        _a[i] = 0;
        _s[i] = 0;
        _b[i] = 0;
    }
}

ColorScales::~ColorScales()
{
    gint i = 0;

    for (i = 0; i < 5; i++) {
        _l[i] = 0;
        _a[i] = 0;
        _s[i] = 0;
        _b[i] = 0;
    }
}

void sp_color_scales_init (SPColorScales *cs)
{
    SP_COLOR_SELECTOR(cs)->base = new ColorScales( SP_COLOR_SELECTOR(cs) );

    if ( SP_COLOR_SELECTOR(cs)->base )
    {
        SP_COLOR_SELECTOR(cs)->base->init();
    }
}

void ColorScales::init()
{
	GtkWidget *t;
	gint i;

	_updating = FALSE;
	_dragging = FALSE;

	t = gtk_table_new (5, 3, FALSE);
	gtk_widget_show (t);
	gtk_box_pack_start (GTK_BOX (_csel), t, TRUE, TRUE, 0);

	/* Create components */
	for (i = 0; i < static_cast< gint > (G_N_ELEMENTS(_a)) ; i++) {
		/* Label */
		_l[i] = gtk_label_new ("");
		gtk_misc_set_alignment (GTK_MISC (_l[i]), 1.0, 0.5);
		gtk_widget_show (_l[i]);
		gtk_table_attach (GTK_TABLE (t), _l[i], 0, 1, i, i + 1, GTK_FILL, GTK_FILL, XPAD, YPAD);
		/* Adjustment */
		_a[i] = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 1.0, 0.01, 0.1, 0.1);
		/* Slider */
		_s[i] = sp_color_slider_new (_a[i]);
		gtk_widget_show (_s[i]);
		gtk_table_attach (GTK_TABLE (t), _s[i], 1, 2, i, i + 1, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), (GtkAttachOptions)GTK_FILL, XPAD, YPAD);

		/* Spinbutton */
		_b[i] = gtk_spin_button_new (GTK_ADJUSTMENT (_a[i]), 0.01, 2);
		sp_dialog_defocus_on_enter (_b[i]);
		gtk_widget_show (_b[i]);
		gtk_table_attach (GTK_TABLE (t), _b[i], 2, 3, i, i + 1, (GtkAttachOptions)0, (GtkAttachOptions)0, XPAD, YPAD);

		/* Attach channel value to adjustment */
		gtk_object_set_data (GTK_OBJECT (_a[i]), "channel", GINT_TO_POINTER (i));
		/* Signals */
		gtk_signal_connect (GTK_OBJECT (_a[i]), "value_changed",
					GTK_SIGNAL_FUNC (_adjustmentAnyChanged), _csel);
		gtk_signal_connect (GTK_OBJECT (_s[i]), "grabbed",
					GTK_SIGNAL_FUNC (_sliderAnyGrabbed), _csel);
		gtk_signal_connect (GTK_OBJECT (_s[i]), "released",
					GTK_SIGNAL_FUNC (_sliderAnyReleased), _csel);
		gtk_signal_connect (GTK_OBJECT (_s[i]), "changed",
					GTK_SIGNAL_FUNC (_sliderAnyChanged), _csel);
	}

	/* Initial mode is none, so it works */
	setMode(SP_COLOR_SCALES_MODE_RGB);
}

static void
sp_color_scales_destroy (GtkObject *object)
{
	if (((GtkObjectClass *) (parent_class))->destroy)
		(* ((GtkObjectClass *) (parent_class))->destroy) (object);
}

static void
sp_color_scales_show_all (GtkWidget *widget)
{
	gtk_widget_show (widget);
}

static void
sp_color_scales_hide_all (GtkWidget *widget)
{
	gtk_widget_hide (widget);
}

GtkWidget *
sp_color_scales_new (void)
{
	SPColorScales *csel;

	csel = (SPColorScales*)gtk_type_new (SP_TYPE_COLOR_SCALES);

	return GTK_WIDGET (csel);
}

void ColorScales::_recalcColor( gboolean changing )
{
    if ( changing )
    {
        SPColor color;
        gfloat alpha = 1.0;
        gfloat c[5];

        switch (_mode) {
        case SP_COLOR_SCALES_MODE_RGB:
        case SP_COLOR_SCALES_MODE_HSV:
            _getRgbaFloatv(c);
            sp_color_set_rgb_float (&color, c[0], c[1], c[2]);
            alpha = c[3];
            break;
        case SP_COLOR_SCALES_MODE_CMYK:
            _getCmykaFloatv( c );
            sp_color_set_cmyk_float (&color, c[0], c[1], c[2], c[3]);
            alpha = c[4];
            break;
        default:
            g_warning ("file %s: line %d: Illegal color selector mode %d", __FILE__, __LINE__, _mode);
            break;
        }
        _updateInternals( color, alpha, _dragging );
    }
    else
    {
        _updateInternals( _color, _alpha, _dragging );
    }
}

/* Helpers for setting color value */

void ColorScales::_colorChanged( const SPColor& color, gfloat alpha )
{
	gfloat tmp[3];
	gfloat c[5] = {0.0, 0.0, 0.0, 0.0};

	switch (_mode) {
	case SP_COLOR_SCALES_MODE_RGB:
		sp_color_get_rgb_floatv (&color, c);
		c[3] = alpha;
		c[4] = 0.0;
		break;
	case SP_COLOR_SCALES_MODE_HSV:
		sp_color_get_rgb_floatv (&color, tmp);
		c[0] = _a[0]->value;
		sp_color_rgb_to_hsv_floatv (c, tmp[0], tmp[1], tmp[2]);
		c[3] = alpha;
		c[4] = 0.0;
		break;
	case SP_COLOR_SCALES_MODE_CMYK:
		sp_color_get_cmyk_floatv (&color, c);
		c[4] = alpha;
		break;
	default:
		g_warning ("file %s: line %d: Illegal color selector mode %d", __FILE__, __LINE__, _mode);
		break;
	}

	_updating = TRUE;
	gtk_adjustment_set_value (_a[0], c[0]);
	gtk_adjustment_set_value (_a[1], c[1]);
	gtk_adjustment_set_value (_a[2], c[2]);
	gtk_adjustment_set_value (_a[3], c[3]);
	gtk_adjustment_set_value (_a[4], c[4]);
	_updateSliders( CSC_CHANNELS_ALL );
	_updating = FALSE;
}

void ColorScales::_getRgbaFloatv( gfloat *rgba )
{
	g_return_if_fail (rgba != NULL);

	switch (_mode) {
	case SP_COLOR_SCALES_MODE_RGB:
		rgba[0] = _a[0]->value;
		rgba[1] = _a[1]->value;
		rgba[2] = _a[2]->value;
		rgba[3] = _a[3]->value;
		break;
	case SP_COLOR_SCALES_MODE_HSV:
		sp_color_hsv_to_rgb_floatv (rgba, _a[0]->value, _a[1]->value, _a[2]->value);
		rgba[3] = _a[3]->value;
		break;
	case SP_COLOR_SCALES_MODE_CMYK:
		sp_color_cmyk_to_rgb_floatv (rgba, _a[0]->value, _a[1]->value, _a[2]->value, _a[3]->value);
		rgba[3] = _a[4]->value;
		break;
	default:
		g_warning ("file %s: line %d: Illegal color selector mode", __FILE__, __LINE__);
		break;
	}
}

void ColorScales::_getCmykaFloatv( gfloat *cmyka )
{
	gfloat rgb[3];

	g_return_if_fail (cmyka != NULL);

	switch (_mode) {
	case SP_COLOR_SCALES_MODE_RGB:
		sp_color_rgb_to_cmyk_floatv (cmyka, _a[0]->value, _a[1]->value, _a[2]->value);
		cmyka[4] = _a[3]->value;
		break;
	case SP_COLOR_SCALES_MODE_HSV:
		sp_color_hsv_to_rgb_floatv (rgb, _a[0]->value, _a[1]->value, _a[2]->value);
		sp_color_rgb_to_cmyk_floatv (cmyka, rgb[0], rgb[1], rgb[2]);
		cmyka[4] = _a[3]->value;
		break;
	case SP_COLOR_SCALES_MODE_CMYK:
		cmyka[0] = _a[0]->value;
		cmyka[1] = _a[1]->value;
		cmyka[2] = _a[2]->value;
		cmyka[3] = _a[3]->value;
		cmyka[4] = _a[4]->value;
		break;
	default:
		g_warning ("file %s: line %d: Illegal color selector mode", __FILE__, __LINE__);
		break;
	}
}

guint32 ColorScales::_getRgba32()
{
	gfloat c[4];
	guint32 rgba;

	_getRgbaFloatv(c);

	rgba = SP_RGBA32_F_COMPOSE (c[0], c[1], c[2], c[3]);

	return rgba;
}

void ColorScales::setMode(SPColorScalesMode mode)
{
	gfloat rgba[4];
	gfloat c[4];

	if (_mode == mode) return;

	if ((_mode == SP_COLOR_SCALES_MODE_RGB) ||
		(_mode == SP_COLOR_SCALES_MODE_HSV) ||
		(_mode == SP_COLOR_SCALES_MODE_CMYK)) {
		_getRgbaFloatv(rgba);
	} else {
		rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1.0;
	}

	_mode = mode;

	switch (mode) {
	case SP_COLOR_SCALES_MODE_RGB:
		gtk_label_set_text (GTK_LABEL (_l[0]), _("Red:"));
		gtk_label_set_text (GTK_LABEL (_l[1]), _("Green:"));
		gtk_label_set_text (GTK_LABEL (_l[2]), _("Blue:"));
		gtk_label_set_text (GTK_LABEL (_l[3]), _("Alpha:"));
		sp_color_slider_set_map (SP_COLOR_SLIDER (_s[0]), NULL);
		gtk_widget_hide (_l[4]);
		gtk_widget_hide (_s[4]);
		gtk_widget_hide (_b[4]);
		_updating = TRUE;
		gtk_adjustment_set_value (_a[0], rgba[0]);
		gtk_adjustment_set_value (_a[1], rgba[1]);
		gtk_adjustment_set_value (_a[2], rgba[2]);
		gtk_adjustment_set_value (_a[3], rgba[3]);
		_updating = FALSE;
		_updateSliders( CSC_CHANNELS_ALL );
		break;
	case SP_COLOR_SCALES_MODE_HSV:
		gtk_label_set_text (GTK_LABEL (_l[0]), _("Hue:"));
		gtk_label_set_text (GTK_LABEL (_l[1]), _("Saturation:"));
		gtk_label_set_text (GTK_LABEL (_l[2]), _("Value:"));
		gtk_label_set_text (GTK_LABEL (_l[3]), _("Alpha:"));
		sp_color_slider_set_map (SP_COLOR_SLIDER (_s[0]), (guchar*)sp_color_scales_hue_map ());
		gtk_widget_hide (_l[4]);
		gtk_widget_hide (_s[4]);
		gtk_widget_hide (_b[4]);
		_updating = TRUE;
		c[0] = 0.0;
		sp_color_rgb_to_hsv_floatv (c, rgba[0], rgba[1], rgba[2]);
		gtk_adjustment_set_value (_a[0], c[0]);
		gtk_adjustment_set_value (_a[1], c[1]);
		gtk_adjustment_set_value (_a[2], c[2]);
		gtk_adjustment_set_value (_a[3], rgba[3]);
		_updating = FALSE;
		_updateSliders( CSC_CHANNELS_ALL );
		break;
	case SP_COLOR_SCALES_MODE_CMYK:
		gtk_label_set_text (GTK_LABEL (_l[0]), _("Cyan:"));
		gtk_label_set_text (GTK_LABEL (_l[1]), _("Magenta:"));
		gtk_label_set_text (GTK_LABEL (_l[2]), _("Yellow:"));
		gtk_label_set_text (GTK_LABEL (_l[3]), _("Black:"));
		gtk_label_set_text (GTK_LABEL (_l[4]), _("Alpha:"));
		sp_color_slider_set_map (SP_COLOR_SLIDER (_s[0]), NULL);
		gtk_widget_show (_l[4]);
		gtk_widget_show (_s[4]);
		gtk_widget_show (_b[4]);
		_updating = TRUE;
		sp_color_rgb_to_cmyk_floatv (c, rgba[0], rgba[1], rgba[2]);
		gtk_adjustment_set_value (_a[0], c[0]);
		gtk_adjustment_set_value (_a[1], c[1]);
		gtk_adjustment_set_value (_a[2], c[2]);
		gtk_adjustment_set_value (_a[3], c[3]);
		gtk_adjustment_set_value (_a[4], rgba[3]);
		_updating = FALSE;
		_updateSliders( CSC_CHANNELS_ALL );
		break;
	default:
		g_warning ("file %s: line %d: Illegal color selector mode", __FILE__, __LINE__);
		break;
	}
}

SPColorScalesMode ColorScales::getMode() const
{
	return _mode;
}

void ColorScales::setSubmode( guint submode )
{
	g_return_if_fail (_csel != NULL);
	g_return_if_fail (SP_IS_COLOR_SCALES (_csel));
	g_return_if_fail (submode < 3);

	switch ( submode )
	{
	default:
	case 0:
		setMode(SP_COLOR_SCALES_MODE_RGB);
		break;
	case 1:
		setMode(SP_COLOR_SCALES_MODE_HSV);
		break;
	case 2:
		setMode(SP_COLOR_SCALES_MODE_CMYK);
		break;
	}
}

guint ColorScales::getSubmode() const
{
	guint submode = 0;

	switch ( _mode )
	{
	case SP_COLOR_SCALES_MODE_HSV:
		submode = 1;
		break;
	case SP_COLOR_SCALES_MODE_CMYK:
		submode = 2;
		break;
	case SP_COLOR_SCALES_MODE_RGB:
	default:
		submode = 0;
	}

	return submode;
}

void ColorScales::_adjustmentAnyChanged( GtkAdjustment *adjustment, SPColorScales *cs )
{
	gint channel;

	channel = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (adjustment), "channel"));

	_adjustmentChanged(cs, channel);
}

void ColorScales::_sliderAnyGrabbed( SPColorSlider *slider, SPColorScales *cs )
{
    ColorScales* scales = (ColorScales*)(SP_COLOR_SELECTOR(cs)->base);
	if (!scales->_dragging) {
		scales->_dragging = TRUE;
        scales->_grabbed();
        scales->_recalcColor( FALSE );
	}
}

void ColorScales::_sliderAnyReleased( SPColorSlider *slider, SPColorScales *cs )
{
    ColorScales* scales = (ColorScales*)(SP_COLOR_SELECTOR(cs)->base);
	if (scales->_dragging) {
		scales->_dragging = FALSE;
        scales->_released();
        scales->_recalcColor( FALSE );
	}
}

void ColorScales::_sliderAnyChanged( SPColorSlider *slider, SPColorScales *cs )
{
    ColorScales* scales = (ColorScales*)(SP_COLOR_SELECTOR(cs)->base);

    scales->_recalcColor( TRUE );
}

void ColorScales::_adjustmentChanged( SPColorScales *cs, guint channel )
{
    ColorScales* scales = (ColorScales*)(SP_COLOR_SELECTOR(cs)->base);
	if (scales->_updating) return;

	scales->_updating = TRUE;

	scales->_updateSliders( (1 << channel) );

    scales->_updateInternals( scales->_color, scales->_alpha, scales->_dragging );

    scales->_recalcColor( TRUE );

	scales->_updating = FALSE;
}

void ColorScales::_updateSliders( guint channels )
{
	gfloat rgb0[3], rgb1[3];
#ifdef SPCS_PREVIEW
	guint32 rgba;
#endif
	switch (_mode) {
	case SP_COLOR_SCALES_MODE_RGB:
		if ((channels != CSC_CHANNEL_R) && (channels != CSC_CHANNEL_A)) {
			/* Update red */
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[0]),
							SP_RGBA32_F_COMPOSE (0.0, _a[1]->value, _a[2]->value, 1.0),
							SP_RGBA32_F_COMPOSE (1.0, _a[1]->value, _a[2]->value, 1.0));
		}
		if ((channels != CSC_CHANNEL_G) && (channels != CSC_CHANNEL_A)) {
			/* Update green */
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[1]),
							SP_RGBA32_F_COMPOSE (_a[0]->value, 0.0, _a[2]->value, 1.0),
							SP_RGBA32_F_COMPOSE (_a[0]->value, 1.0, _a[2]->value, 1.0));
		}
		if ((channels != CSC_CHANNEL_B) && (channels != CSC_CHANNEL_A)) {
			/* Update blue */
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[2]),
							SP_RGBA32_F_COMPOSE (_a[0]->value, _a[1]->value, 0.0, 1.0),
							SP_RGBA32_F_COMPOSE (_a[0]->value, _a[1]->value, 1.0, 1.0));
		}
		if (channels != CSC_CHANNEL_A) {
			/* Update alpha */
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[3]),
							SP_RGBA32_F_COMPOSE (_a[0]->value, _a[1]->value, _a[2]->value, 0.0),
							SP_RGBA32_F_COMPOSE (_a[0]->value, _a[1]->value, _a[2]->value, 1.0));
		}
		break;
	case SP_COLOR_SCALES_MODE_HSV:
		/* Hue is never updated */
		if ((channels != CSC_CHANNEL_S) && (channels != CSC_CHANNEL_A)) {
			/* Update saturation */
			sp_color_hsv_to_rgb_floatv (rgb0, _a[0]->value, 0.0, _a[2]->value);
			sp_color_hsv_to_rgb_floatv (rgb1, _a[0]->value, 1.0, _a[2]->value);
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[1]),
							SP_RGBA32_F_COMPOSE (rgb0[0], rgb0[1], rgb0[2], 1.0),
							SP_RGBA32_F_COMPOSE (rgb1[0], rgb1[1], rgb1[2], 1.0));
		}
		if ((channels != CSC_CHANNEL_V) && (channels != CSC_CHANNEL_A)) {
			/* Update value */
			sp_color_hsv_to_rgb_floatv (rgb0, _a[0]->value, _a[1]->value, 0.0);
			sp_color_hsv_to_rgb_floatv (rgb1, _a[0]->value, _a[1]->value, 1.0);
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[2]),
							SP_RGBA32_F_COMPOSE (rgb0[0], rgb0[1], rgb0[2], 1.0),
							SP_RGBA32_F_COMPOSE (rgb1[0], rgb1[1], rgb1[2], 1.0));
		}
		if (channels != CSC_CHANNEL_A) {
			/* Update alpha */
			sp_color_hsv_to_rgb_floatv (rgb0, _a[0]->value, _a[1]->value, _a[2]->value);
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[3]),
							SP_RGBA32_F_COMPOSE (rgb0[0], rgb0[1], rgb0[2], 0.0),
							SP_RGBA32_F_COMPOSE (rgb0[0], rgb0[1], rgb0[2], 1.0));
		}
		break;
	case SP_COLOR_SCALES_MODE_CMYK:
		if ((channels != CSC_CHANNEL_C) && (channels != CSC_CHANNEL_CMYKA)) {
			/* Update saturation */
			sp_color_cmyk_to_rgb_floatv (rgb0, 0.0, _a[1]->value, _a[2]->value, _a[3]->value);
			sp_color_cmyk_to_rgb_floatv (rgb1, 1.0, _a[1]->value, _a[2]->value, _a[3]->value);
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[0]),
							SP_RGBA32_F_COMPOSE (rgb0[0], rgb0[1], rgb0[2], 1.0),
							SP_RGBA32_F_COMPOSE (rgb1[0], rgb1[1], rgb1[2], 1.0));
		}
		if ((channels != CSC_CHANNEL_M) && (channels != CSC_CHANNEL_CMYKA)) {
			/* Update saturation */
			sp_color_cmyk_to_rgb_floatv (rgb0, _a[0]->value, 0.0, _a[2]->value, _a[3]->value);
			sp_color_cmyk_to_rgb_floatv (rgb1, _a[0]->value, 1.0, _a[2]->value, _a[3]->value);
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[1]),
							SP_RGBA32_F_COMPOSE (rgb0[0], rgb0[1], rgb0[2], 1.0),
							SP_RGBA32_F_COMPOSE (rgb1[0], rgb1[1], rgb1[2], 1.0));
		}
		if ((channels != CSC_CHANNEL_Y) && (channels != CSC_CHANNEL_CMYKA)) {
			/* Update saturation */
			sp_color_cmyk_to_rgb_floatv (rgb0, _a[0]->value, _a[1]->value, 0.0, _a[3]->value);
			sp_color_cmyk_to_rgb_floatv (rgb1, _a[0]->value, _a[1]->value, 1.0, _a[3]->value);
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[2]),
							SP_RGBA32_F_COMPOSE (rgb0[0], rgb0[1], rgb0[2], 1.0),
							SP_RGBA32_F_COMPOSE (rgb1[0], rgb1[1], rgb1[2], 1.0));
		}
		if ((channels != CSC_CHANNEL_K) && (channels != CSC_CHANNEL_CMYKA)) {
			/* Update saturation */
			sp_color_cmyk_to_rgb_floatv (rgb0, _a[0]->value, _a[1]->value, _a[2]->value, 0.0);
			sp_color_cmyk_to_rgb_floatv (rgb1, _a[0]->value, _a[1]->value, _a[2]->value, 1.0);
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[3]),
							SP_RGBA32_F_COMPOSE (rgb0[0], rgb0[1], rgb0[2], 1.0),
							SP_RGBA32_F_COMPOSE (rgb1[0], rgb1[1], rgb1[2], 1.0));
		}
		if (channels != CSC_CHANNEL_CMYKA) {
			/* Update saturation */
			sp_color_cmyk_to_rgb_floatv (rgb0, _a[0]->value, _a[1]->value, _a[2]->value, _a[3]->value);
			sp_color_slider_set_colors (SP_COLOR_SLIDER (_s[4]),
							SP_RGBA32_F_COMPOSE (rgb0[0], rgb0[1], rgb0[2], 0.0),
							SP_RGBA32_F_COMPOSE (rgb0[0], rgb0[1], rgb0[2], 1.0));
		}
		break;
	default:
		g_warning ("file %s: line %d: Illegal color selector mode", __FILE__, __LINE__);
		break;
	}

	// Force the internal color to be updated
    if ( !_updating )
    {
        _recalcColor( TRUE );
    }

#ifdef SPCS_PREVIEW
	rgba = sp_color_scales_get_rgba32 (cs);
	sp_color_preview_set_rgba32 (SP_COLOR_PREVIEW (_p), rgba);
#endif
}

static const gchar *
sp_color_scales_hue_map (void)
{
	static gchar *map = NULL;

	if (!map) {
		gchar *p;
		gint h;
		map = g_new (gchar, 4 * 1024);
		p = map;
		for (h = 0; h < 1024; h++) {
			gfloat rgb[3];
			sp_color_hsv_to_rgb_floatv (rgb, h / 1024.0, 1.0, 1.0);
			*p++ = SP_COLOR_F_TO_U (rgb[0]);
			*p++ = SP_COLOR_F_TO_U (rgb[1]);
			*p++ = SP_COLOR_F_TO_U (rgb[2]);
			*p++ = 255;
		}
	}

	return map;
}

