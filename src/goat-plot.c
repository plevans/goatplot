/*
 * goat-plot.c
 * This file is part of GoatPlot
 *
 * Copyright (C) 2014 - Bernhard Schuster <schuster.bernhard@gmail.com>
 *
 * GoatPlot is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GoatPlot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with GoatPlot. If not, see <http://www.gnu.org/licenses/>.
 */

#include "goat-plot.h"

#include <glib/gprintf.h>
#include <math.h>

static gboolean draw (GtkWidget *widget, cairo_t *cr);
static void get_prefered_width (GtkWidget *widget, int *minimal, int *natural);
static void get_prefered_height (GtkWidget *widget, int *minimal, int *natural);
static gboolean event (GtkWidget *widget, GdkEvent *event);
static gboolean scroll_event (GtkWidget *widget, GdkEventScroll *event);

struct _GoatPlotPrivate {
	GArray *array; // array of GoatDataset pointers
	// remove this, its the users duty to prescale data properly FIXME

	GdkRGBA color_background;
	GdkRGBA color_border;

	GoatScale *scale_x;
	GoatScale *scale_y;
};

G_DEFINE_TYPE_WITH_PRIVATE (GoatPlot, goat_plot, GTK_TYPE_DRAWING_AREA);

#include "goat-plot-internal.h"

static void goat_plot_finalize (GObject *object)
{
	GoatPlot *plot = GOAT_PLOT (object);
	GoatPlotPrivate *priv = goat_plot_get_instance_private (plot);
	gint register i = priv->array->len;
	while (--i >= 0) {
		g_object_unref (g_array_index (priv->array, GoatDataset *, i));
	}
	g_array_free (plot->priv->array, TRUE);

	G_OBJECT_CLASS (goat_plot_parent_class)->finalize (object);
}

enum {
	PROP_0,

	PROP_SCALE_X,
	PROP_SCALE_Y,

	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

static void goat_plot_set_gproperty (GObject *object, guint prop_id, const GValue *value, GParamSpec *spec)
{
	GoatPlot *dataset = GOAT_PLOT (object);
	GoatPlotPrivate *priv = goat_plot_get_instance_private (dataset);

	switch (prop_id) {
	case PROP_SCALE_X:
		priv->scale_x = g_value_get_pointer (value);
		break;
	case PROP_SCALE_Y:
		priv->scale_y = g_value_get_pointer (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (dataset, prop_id, spec);
	}
}

static void goat_plot_get_gproperty (GObject *object, guint prop_id, GValue *value, GParamSpec *spec)
{
	GoatPlot *dataset = GOAT_PLOT (object);
	GoatPlotPrivate *priv = goat_plot_get_instance_private (dataset);

	switch (prop_id) {
	case PROP_SCALE_X:
		g_value_set_pointer (value, priv->scale_x);
		break;
	case PROP_SCALE_Y:
		g_value_set_pointer (value, priv->scale_y);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (dataset, prop_id, spec);
	}
}

static void goat_plot_class_init (GoatPlotClass *klass)
{
	g_type_class_add_private (klass, sizeof (GoatPlotPrivate));

	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = goat_plot_finalize;

	object_class->set_property = goat_plot_set_gproperty;
	object_class->get_property = goat_plot_get_gproperty;

	obj_properties[PROP_SCALE_X] = g_param_spec_pointer ("scale_x", "GoatPlot::scale_x", "scale x", G_PARAM_READWRITE);

	obj_properties[PROP_SCALE_Y] = g_param_spec_pointer ("scale_y", "GoatPlot::scale_y", "scale y", G_PARAM_READWRITE);

	g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);

	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->draw = draw;

#if 0
	widget_class->button_press_event = goat_plot_button_press_event;
	widget_class->button_release_event = goat_plot_button_release_event;
	widget_class->key_press_event = goat_plot_key_press_event;
	widget_class->key_release_event = goat_plot_key_release_event;
#endif
	widget_class->scroll_event = scroll_event;
	widget_class->get_preferred_width = get_prefered_width;
	widget_class->get_preferred_height = get_prefered_height;
}

static void goat_plot_init (GoatPlot *self)
{
	gboolean assure;
	GtkWidget *widget = GTK_WIDGET (self);
	gtk_widget_set_has_window (widget, FALSE);

	self->priv = goat_plot_get_instance_private (self);

	self->priv->array = g_array_new (FALSE, TRUE, sizeof (void *));

	assure = gdk_rgba_parse (&(self->priv->color_background), "white");
	g_assert (assure);
	assure = gdk_rgba_parse (&(self->priv->color_border), "black");
	g_assert (assure);

	gtk_widget_add_events (widget, GDK_SCROLL_MASK);
	g_assert ((gtk_widget_get_events (widget) & GDK_SCROLL_MASK) != 0);

	gtk_widget_set_sensitive (widget, TRUE);
	gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_grab_focus (widget);
}

/**
 * create a new GoatPlot widget
 */
GoatPlot *goat_plot_new (GoatScale *x, GoatScale *y)
{
	return GOAT_PLOT (gtk_widget_new (GOAT_TYPE_PLOT, "scale_x", x, "scale_y", y, NULL));
}

/**
 *
 */
void goat_plot_set_scale_x (GoatPlot *plot, GoatScale *scale)
{
	g_object_set (G_OBJECT (plot), "scale_x", scale, NULL);
}

/**
 *
 */
void goat_plot_set_scale_y (GoatPlot *plot, GoatScale *scale)
{
	g_object_set (G_OBJECT (plot), "scale_y", scale, NULL);
}

/**
 * add a dataset to the plot
 * @returns identifier to use for removal
 */
gint goat_plot_add_dataset (GoatPlot *plot, GoatDataset *dataset)
{
	g_return_val_if_fail (plot != NULL, -1);
	g_return_val_if_fail (GOAT_IS_PLOT (plot), -1);
	g_return_val_if_fail (dataset != NULL, -1);
	g_return_val_if_fail (GOAT_IS_DATASET (dataset), -1);

	gpointer tmp = g_object_ref (dataset);
	g_array_append_val (plot->priv->array, tmp);

	return plot->priv->array->len - 1;
}

/**
 * remove a dataset from the plot
 */
GoatDataset *goat_plot_remove_dataset (GoatPlot *plot, gint datasetid)
{
	GoatDataset *dataset = NULL;
	GoatPlotPrivate *priv = goat_plot_get_instance_private (plot);
	GoatDataset **datasetptr = &(g_array_index (priv->array, GoatDataset *, datasetid));
	if (datasetptr != NULL) {
		dataset = *datasetptr;
		*datasetptr = NULL;
	}
	return dataset;
}

/**
 * @param window window height or width, or if fixed scale, use a const for that
 * @param min in units
 * @param max in units
 * @param unit_to_pixel [out]
 */
gboolean get_unit_to_pixel_factor (int window, gdouble min, gdouble max, gdouble *unit_to_pixel)
{
	gdouble delta = max - min;
	if (delta > 0.) {
		*unit_to_pixel = (double)window / delta;
		g_printf ("MAPPING: %i --{%lf}--> %lf\n", window, (*unit_to_pixel), delta);
		return TRUE;
	}
	*unit_to_pixel = 1.;
	return FALSE;
}

static gboolean draw (GtkWidget *widget, cairo_t *cr)
{
	GoatPlot *plot;
	GoatDataset *dataset;
	GoatPlotPrivate *priv;
	GtkAllocation allocation; //==gint x,y,width,heightynamically
	// TODO this needs to be done dynamically
	// TODO based on the style context and where the scales are
	// TODO https://github.com/drahnr/goatplot/issues/8
	GtkBorder padding = {50, 50, 50, 50}; // left, right, top, bottom
	gint i;
	gdouble x_nil_pixel;
	gdouble y_nil_pixel;
	gdouble x_unit_to_pixel;
	gdouble y_unit_to_pixel;

	if (gtk_widget_is_drawable (widget)) {
		plot = GOAT_PLOT (widget);
		priv = goat_plot_get_instance_private (plot);
		cairo_save (cr);

		gtk_widget_get_allocation (widget, &allocation);

#if 0
		//FIXME this is all zer0es
		gtk_style_context_get_padding (gtk_widget_get_style_context (widget), GTK_STATE_FLAG_ACTIVE, &padding);

		g_print ("alocation x,y %i %i    w,h %i %i\n",
		         allocation.x, allocation.y, allocation.width, allocation.height);
		g_print ("padding left,right,top,bottom %i %i %i %i\n",
		         padding.left, padding.right, padding.top, padding.bottom);
#endif
		// translate origin to plot graphs to (0,0) of our plot
		cairo_translate (cr, padding.left, allocation.height - padding.bottom);

		// make it plot naturally +up, -down
		cairo_scale (cr, 1., -1.);
		// draw the background (scale + grid + white box)

		// the scale we just drew should not be drawable either
		// now the coords are y↑ x→
		const gint width = allocation.width - padding.left - padding.right;
		const gint height = allocation.height - padding.top - padding.bottom;

		g_printf ("--- w=%i h=%i\n", width, height);
		gdouble ref_x_min = +G_MAXDOUBLE;
		gdouble ref_x_max = -G_MAXDOUBLE;
		gdouble ref_y_min = +G_MAXDOUBLE;
		gdouble ref_y_max = -G_MAXDOUBLE;

		// get the common extends of all data sets if any scale used is set to autorange
		//
		// vfunc, we use it in a loop, so caching is good idea
		const gboolean register autorange_x = goat_scale_is_auto_range (priv->scale_x);
		const gboolean register autorange_y = goat_scale_is_auto_range (priv->scale_y);
		if (!autorange_x) {
			goat_scale_get_range (priv->scale_x, &ref_x_min, &ref_x_max);
		}
		if (!autorange_y) {
			goat_scale_get_range (priv->scale_y, &ref_y_min, &ref_y_max);
		}
		if (autorange_x || autorange_y) {

			for (i = 0; i < priv->array->len; i++) {
				dataset = g_array_index (priv->array, GoatDataset *, i);
				gdouble x_min, x_max, y_min, y_max;
				goat_dataset_get_extrema (dataset, &x_min, &x_max, &y_min, &y_max);
				if (autorange_x) {
					if (ref_x_min > x_min)
						ref_x_min = x_min;
					if (ref_x_max < x_max)
						ref_x_max = x_max;
				}
				if (autorange_y) {
					if (ref_y_min > y_min)
						ref_y_min = y_min;
					if (ref_y_max < y_max)
						ref_y_max = y_max;
				}
			}

			goat_scale_update_range (priv->scale_x, ref_x_min, ref_x_max);
			goat_scale_update_range (priv->scale_y, ref_y_min, ref_y_max);

			// TODO add some fixup if x_min is very close to x_max
			// TODO add some additional padding for niceness :)
			// TODO think about extra padding to compensate marker sizes
		}
		gboolean draw = TRUE;
		if (!get_unit_to_pixel_factor (width, ref_x_min, ref_x_max, &x_unit_to_pixel)) {
			g_warning ("Bad x range");
			draw = FALSE;
		}
		if (!get_unit_to_pixel_factor (height, ref_y_min, ref_y_max, &y_unit_to_pixel)) {
			g_warning ("Bad y range");
			draw = FALSE;
		}

		x_nil_pixel = ref_x_min * -x_unit_to_pixel;
		y_nil_pixel = ref_y_min * -y_unit_to_pixel;

		draw_background (plot, cr, &allocation, &padding, x_nil_pixel, y_nil_pixel, x_unit_to_pixel, y_unit_to_pixel,
		                 &priv->color_background, &priv->color_border);

		if (draw) {
			draw_scales (plot, cr, &allocation, &padding, x_nil_pixel, y_nil_pixel, x_unit_to_pixel, y_unit_to_pixel);
		}

		clip_drawable_area (plot, cr, &allocation, &padding);

		if (draw) {
			draw_nil_lines (plot, cr, width, height, x_nil_pixel, y_nil_pixel);

			for (i = 0; i < priv->array->len; i++) {
				dataset = g_array_index (priv->array, GoatDataset *, i);
				draw_dataset (cr, dataset, height, width, ref_x_min, ref_x_max, x_nil_pixel, x_unit_to_pixel, ref_y_min,
				              ref_y_max, y_nil_pixel, y_unit_to_pixel);
			}
		}
		cairo_restore (cr);

		return TRUE;
	}
	return FALSE;
}

static void get_prefered_width (GtkWidget *widget, int *minimal, int *natural)
{
	*minimal = 200;
	*natural = 350;
}

static void get_prefered_height (GtkWidget *widget, int *minimal, int *natural)
{
	*minimal = 200;
	*natural = 350;
}

void goat_plot_set_background_color (GoatPlot *plot, GdkRGBA *color)
{
	g_return_if_fail (plot);
	g_return_if_fail (GOAT_IS_PLOT (plot));
	g_return_if_fail (color != NULL);

	GoatPlotPrivate *priv;

	priv = goat_plot_get_instance_private (plot);

	priv->color_background = *color;
}

void goat_plot_set_border_color (GoatPlot *plot, GdkRGBA *color)
{
	g_return_if_fail (plot);
	g_return_if_fail (GOAT_IS_PLOT (plot));
	g_return_if_fail (color != NULL);

	GoatPlotPrivate *priv;

	priv = goat_plot_get_instance_private (plot);

	priv->color_border = *color;
}

/**
 * TODO handle zooming and scrolling
 * https://github.com/drahnr/goatplot/issues/9
 */
static gboolean scroll_event (GtkWidget *widget, GdkEventScroll *event)
{
	g_print ("scroll event\n");
	return FALSE;
}

/**
 * TODO handle envents
 * https://github.com/drahnr/goatplot/issues/9
 */
static gboolean event (GtkWidget *widget, GdkEvent *event)
{
	g_warning ("ehllllo\n??\n");
	switch (event->type) {
	case GDK_SCROLL:
	case GDK_SCROLL_UP:
	case GDK_SCROLL_DOWN:
		g_print ("got a scroll event!");
		break;
	default:
		break;
	}
	return TRUE;
}
