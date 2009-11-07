/* $Id$ */

/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996 Thomas Nau
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Contact addresses for paper mail and Email:
 *  Thomas Nau, Schlehenweg 15, 88471 Baustetten, Germany
 *  Thomas.Nau@rz.uni-ulm.de
 *
 */

/* This file copied and modified by Peter Clifton, starting from
 * gui-pinout-window.c, written by Bill Wilson for the PCB Gtk port */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "global.h"

#include "gui.h"

#include "copy.h"
#include "data.h"
#include "draw.h"
#include "mymem.h"
#include "move.h"
#include "rotate.h"
#include "gui-pinout-preview.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

RCSID ("$Id$");


static gboolean
pinout_zoom_fit (GhidPinoutPreview * pinout, gint zoom)
{
  pinout->zoom = zoom;

  /* Should be a function I can call for this:
   */
  pinout->scale = 1.0 / (100.0 * exp (pinout->zoom * LN_2_OVER_2));

  pinout->x_max = pinout->element.BoundingBox.X2 + Settings.PinoutOffsetX;
  pinout->y_max = pinout->element.BoundingBox.Y2 + Settings.PinoutOffsetY;
  pinout->w_pixels = (gint) (pinout->scale *
			     (pinout->element.BoundingBox.X2 -
			      pinout->element.BoundingBox.X1));
  pinout->h_pixels = (gint) (pinout->scale *
			     (pinout->element.BoundingBox.Y2 -
			      pinout->element.BoundingBox.Y1));

  if (pinout->w_pixels > 3 * Output.Width / 4 ||
      pinout->h_pixels > 3 * Output.Height / 4)
    return FALSE;
  return TRUE;
}


static void
pinout_set_data (GhidPinoutPreview * pinout, ElementType * element)
{
  gint tx, ty, x_min = 0, y_min = 0;

  if (element == NULL)
    {
      FreeElementMemory (&pinout->element);
      pinout->w_pixels = 0;
      pinout->h_pixels = 0;
      return;
    }

  /* 
   * copy element data 
   * enable output of pin and padnames
   * move element to a 5% offset from zero position
   * set all package lines/arcs to zero width
   */
  CopyElementLowLevel (NULL, &pinout->element, element, FALSE, 0, 0);
  PIN_LOOP (&pinout->element);
  {
    tx = abs (pinout->element.Pin[0].X - pin->X);
    ty = abs (pinout->element.Pin[0].Y - pin->Y);
    if (x_min == 0 || (tx != 0 && tx < x_min))
      x_min = tx;
    if (y_min == 0 || (ty != 0 && ty < y_min))
      y_min = ty;
    SET_FLAG (DISPLAYNAMEFLAG, pin);
  }
  END_LOOP;

  PAD_LOOP (&pinout->element);
  {
    tx = abs (pinout->element.Pad[0].Point1.X - pad->Point1.X);
    ty = abs (pinout->element.Pad[0].Point1.Y - pad->Point1.Y);
    if (x_min == 0 || (tx != 0 && tx < x_min))
      x_min = tx;
    if (y_min == 0 || (ty != 0 && ty < y_min))
      y_min = ty;
    SET_FLAG (DISPLAYNAMEFLAG, pad);
  }
  END_LOOP;


  MoveElementLowLevel (NULL, &pinout->element,
		       Settings.PinoutOffsetX -
		       pinout->element.BoundingBox.X1,
		       Settings.PinoutOffsetY -
		       pinout->element.BoundingBox.Y1);

  if (!pinout_zoom_fit (pinout, 2))
    pinout_zoom_fit (pinout, 3);

  ELEMENTLINE_LOOP (&pinout->element);
  {
    line->Thickness = 0;
  }
  END_LOOP;

  ARC_LOOP (&pinout->element);
  {
    /* 
     * for whatever reason setting a thickness of 0 causes the arcs to
     * not display so pick 1 which does display but is still quite
     * thin.
     */
    arc->Thickness = 1;
  }
  END_LOOP;
}


#define Z_NEAR 3.0

static gboolean
ghid_pinout_preview_expose (GtkWidget * widget, GdkEventExpose * ev)
{
  extern HID ghid_hid;
  GdkGLContext* pGlContext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (widget);
  GhidPinoutPreview *pinout = GHID_PINOUT_PREVIEW (widget);
  double save_zoom;
  int da_w, da_h;
  int save_left, save_top;
  int save_width, save_height;
  int save_view_width, save_view_height;
  double xz, yz;

  save_zoom = gport->zoom;
  save_width = gport->width;
  save_height = gport->height;
  save_left = gport->view_x0;
  save_top = gport->view_y0;
  save_view_width = gport->view_width;
  save_view_height = gport->view_height;

  /* Setup zoom factor for drawing routines */

  gdk_window_get_geometry (widget->window, 0, 0, &da_w, &da_h, 0);
  xz = (double) pinout->x_max / da_w;
  yz = (double) pinout->y_max / da_h;
  if (xz > yz)
    gport->zoom = xz;
  else
    gport->zoom = yz;

  gport->width = da_w;
  gport->height = da_h;
  gport->view_width = da_w * gport->zoom;
  gport->view_height = da_h * gport->zoom;
  gport->view_x0 = (pinout->x_max - gport->view_width) / 2;
  gport->view_y0 = (pinout->y_max - gport->view_height) / 2;

  /* make GL-context "current" */
  if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext)) {
    return FALSE;
  }

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glViewport (0, 0, widget->allocation.width, widget->allocation.height);

  glEnable (GL_SCISSOR_TEST);
  glScissor (ev->area.x,
             widget->allocation.height - ev->area.height - ev->area.y,
             ev->area.width, ev->area.height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (0, widget->allocation.width, widget->allocation.height, 0, -100000, 100000);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  glTranslatef (0.0f, 0.0f, -Z_NEAR);

  glClearColor (gport->bg_color.red / 65535.,
                gport->bg_color.green / 65535.,
                gport->bg_color.blue / 65535.,
                1.);
  glStencilMask (~0);
  glClearStencil (0);
  glClear (GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  hidgl_reset_stencil_usage ();

  /* call the drawing routine */
  hidgl_init_triangle_array (&buffer);
  ghid_invalidate_current_gc ();
  glPushMatrix ();
  glScalef ((ghid_flip_x ? -1. : 1.) / gport->zoom,
            (ghid_flip_y ? -1. : 1.) / gport->zoom, 1);
  glTranslatef (ghid_flip_x ? gport->view_x0 - PCB->MaxWidth  :
                             -gport->view_x0,
                ghid_flip_y ? gport->view_y0 - PCB->MaxHeight :
                             -gport->view_y0, 0);
  hid_expose_callback (&ghid_hid, NULL, &pinout->element);
  hidgl_flush_triangles (&buffer);
  glPopMatrix ();

  glUnmapBuffer (GL_ARRAY_BUFFER);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers (1, &buffer.vbo_name);

  if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
    gdk_gl_drawable_swap_buffers (pGlDrawable);
  else
    glFlush ();

  /* end drawing to current GL-context */
  gdk_gl_drawable_gl_end (pGlDrawable);

  gport->zoom = save_zoom;
  gport->width = save_width;
  gport->height = save_height;
  gport->view_x0 = save_left;
  gport->view_y0 = save_top;
  gport->view_width = save_view_width;
  gport->view_height = save_view_height;

  return FALSE;
}


enum
{
  PROP_ELEMENT_DATA = 1,
};


static GObjectClass *ghid_pinout_preview_parent_class = NULL;


/*! \brief GObject constructor
 *
 *  \par Function Description
 *  Chain up and construct the object, then setup the
 *  necessary state for our widget now it is constructed.
 *
 *  \param [in] type                    The GType of object to be constructed
 *  \param [in] n_construct_properties  Number of construct properties
 *  \param [in] contruct_params         The construct properties
 *
 *  \returns The GObject having just been constructed.
 */
static GObject *
ghid_pinout_preview_constructor (GType type,
                                 guint n_construct_properties,
                                 GObjectConstructParam *construct_properties)
{
  GObject *object;

  /* chain up to constructor of parent class */
  object = G_OBJECT_CLASS (ghid_pinout_preview_parent_class)->
    constructor (type, n_construct_properties, construct_properties);

  gtk_widget_set_gl_capability (GTK_WIDGET (object),
                                gport->glconfig,
                                NULL,
                                TRUE,
                                GDK_GL_RGBA_TYPE);

  return object;
}



/*! \brief GObject finalise handler
 *
 *  \par Function Description
 *  Just before the GhidPinoutPreview GObject is finalized, free our
 *  allocated data, and then chain up to the parent's finalize handler.
 *
 *  \param [in] widget  The GObject being finalized.
 */
static void
ghid_pinout_preview_finalize (GObject * object)
{
  GhidPinoutPreview *pinout = GHID_PINOUT_PREVIEW (object);

  /* Passing NULL for element data will free the old memory */
  pinout_set_data (pinout, NULL);

  G_OBJECT_CLASS (ghid_pinout_preview_parent_class)->finalize (object);
}


/*! \brief GObject property setter function
 *
 *  \par Function Description
 *  Setter function for GhidPinoutPreview's GObject properties,
 *  "settings-name" and "toplevel".
 *
 *  \param [in]  object       The GObject whose properties we are setting
 *  \param [in]  property_id  The numeric id. under which the property was
 *                            registered with g_object_class_install_property()
 *  \param [in]  value        The GValue the property is being set from
 *  \param [in]  pspec        A GParamSpec describing the property being set
 */
static void
ghid_pinout_preview_set_property (GObject * object, guint property_id,
				  const GValue * value, GParamSpec * pspec)
{
  GhidPinoutPreview *pinout = GHID_PINOUT_PREVIEW (object);

  switch (property_id)
    {
    case PROP_ELEMENT_DATA:
      pinout_set_data (pinout, g_value_get_pointer (value));
      if (GTK_WIDGET_REALIZED (GTK_WIDGET (pinout)))
	gdk_window_invalidate_rect (GTK_WIDGET (pinout)->window, NULL, FALSE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }

}


/*! \brief GObject property getter function
 *
 *  \par Function Description
 *  Getter function for GhidPinoutPreview's GObject properties,
 *  "settings-name" and "toplevel".
 *
 *  \param [in]  object       The GObject whose properties we are getting
 *  \param [in]  property_id  The numeric id. under which the property was
 *                            registered with g_object_class_install_property()
 *  \param [out] value        The GValue in which to return the value of the property
 *  \param [in]  pspec        A GParamSpec describing the property being got
 */
static void
ghid_pinout_preview_get_property (GObject * object, guint property_id,
				  GValue * value, GParamSpec * pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }

}


/*! \brief GType class initialiser for GhidPinoutPreview
 *
 *  \par Function Description
 *  GType class initialiser for GhidPinoutPreview. We override our parent
 *  virtual class methods as needed and register our GObject properties.
 *
 *  \param [in]  klass       The GhidPinoutPreviewClass we are initialising
 */
static void
ghid_pinout_preview_class_init (GhidPinoutPreviewClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *gtk_widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructor = ghid_pinout_preview_constructor;
  gobject_class->finalize = ghid_pinout_preview_finalize;
  gobject_class->set_property = ghid_pinout_preview_set_property;
  gobject_class->get_property = ghid_pinout_preview_get_property;

  gtk_widget_class->expose_event = ghid_pinout_preview_expose;

  ghid_pinout_preview_parent_class = g_type_class_peek_parent (klass);

  g_object_class_install_property (gobject_class, PROP_ELEMENT_DATA,
				   g_param_spec_pointer ("element-data",
							 "",
							 "",
							 G_PARAM_WRITABLE));
}


/*! \brief Function to retrieve GhidPinoutPreview's GType identifier.
 *
 *  \par Function Description
 *  Function to retrieve GhidPinoutPreview's GType identifier.
 *  Upon first call, this registers the GhidPinoutPreview in the GType system.
 *  Subsequently it returns the saved value from its first execution.
 *
 *  \return the GType identifier associated with GhidPinoutPreview.
 */
GType
ghid_pinout_preview_get_type ()
{
  static GType ghid_pinout_preview_type = 0;

  if (!ghid_pinout_preview_type)
    {
      static const GTypeInfo ghid_pinout_preview_info = {
	sizeof (GhidPinoutPreviewClass),
	NULL,			/* base_init */
	NULL,			/* base_finalize */
	(GClassInitFunc) ghid_pinout_preview_class_init,
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	sizeof (GhidPinoutPreview),
	0,			/* n_preallocs */
	NULL,			/* instance_init */
      };

      ghid_pinout_preview_type =
	g_type_register_static (GTK_TYPE_DRAWING_AREA, "GhidPinoutPreview",
				&ghid_pinout_preview_info, 0);
    }

  return ghid_pinout_preview_type;
}


/*! \brief Convenience function to create a new pinout preview
 *
 *  \par Function Description
 *  Convenience function which creates a GhidPinoutPreview.
 *
 *  \return  The GhidPinoutPreview created.
 */
GtkWidget *
ghid_pinout_preview_new (ElementType * element)
{
  GhidPinoutPreview *pinout_preview;

  pinout_preview = g_object_new (GHID_TYPE_PINOUT_PREVIEW,
				 "element-data", element, NULL);

  return GTK_WIDGET (pinout_preview);
}


/*! \brief Query the natural size of a pinout preview
 *
 *  \par Function Description
 *  Convenience function to query the natural size of a pinout preview
 */
void
ghid_pinout_preview_get_natural_size (GhidPinoutPreview * pinout,
				      int *width, int *height)
{
  *width = pinout->w_pixels;
  *height = pinout->h_pixels;
}
