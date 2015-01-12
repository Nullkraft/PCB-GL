/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 2009-2011 PCB Contributers (See ChangeLog for details)
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cairo.h>
#include <gdk/gdk.h>

#include "data.h"
#include "hid_draw.h"
#include "hidcairo.h"
#include "hid/hidint.h"
#include "draw_helpers.h"

typedef struct hidcairo_gc_struct {
  struct hid_gc_struct gc; /* Parent */

  char *colorname;
  double brightness;
  double saturation;
  cairo_line_cap_t line_cap;
  cairo_line_join_t line_join;
  double line_width;
} *hidcairoGC;

typedef struct render_priv {
  cairo_t *cr;
  GdkColormap *colormap;
  int cur_mask;
} render_priv;


static hidGC
hidcairo_make_gc (HID_DRAW *hid_draw)
{
  hidGC gc = (hidGC) calloc(1, sizeof(struct hidcairo_gc_struct));
  hidcairoGC hidcairo_gc = (hidcairoGC)gc;

  gc->hid = NULL;
  gc->hid_draw = hid_draw;

  /* Initial color settings need configuring */
  hidcairo_gc->colorname = NULL;
  hidcairo_gc->brightness = 1.0;
  hidcairo_gc->saturation = 1.0;

  hidcairo_gc->line_cap = CAIRO_LINE_CAP_ROUND;
  hidcairo_gc->line_join = CAIRO_LINE_JOIN_MITER;
  hidcairo_gc->line_width = 0.0;

  return gc;
}

static void
hidcairo_destroy_gc (hidGC gc)
{
  free (gc);
}

static void
hidcairo_use_mask (HID_DRAW *hid_draw, enum mask_mode mode)
{
  render_priv *priv = hid_draw->priv;

  if (mode == priv->cur_mask)
    return;

  switch (mode)
    {
    case HID_MASK_BEFORE:
      cairo_push_group (priv->cr);
      break;

    case HID_MASK_CLEAR:
      cairo_set_operator (priv->cr, CAIRO_OPERATOR_CLEAR);
      break;

    case HID_MASK_AFTER:
      /* The HID asks not to receive this mask type, so warn if we get it */
      g_return_if_reached ();

    case HID_MASK_OFF:
      cairo_pop_group_to_source (priv->cr);
      cairo_set_operator (priv->cr, CAIRO_OPERATOR_OVER);
      cairo_paint (priv->cr);
      break;
    }
  priv->cur_mask = mode;
}

typedef struct
{
  int color_set;
  GdkColor color;
  double red;
  double green;
  double blue;
} ColorCache;

static void
_set_color (hidGC gc)
{
  render_priv *priv = gc->hid_draw->priv;
  hidcairoGC hidcairo_gc = (hidcairoGC)gc;

  static void *cache = NULL;
  hidval cval;
  ColorCache *cc;
  double r, g, b;
  double luminance;

  if (hidcairo_gc->colorname == NULL)
    return;

  if (priv->colormap == NULL)
    priv->colormap = gdk_colormap_get_system ();

  if (strcmp (hidcairo_gc->colorname, "erase") == 0)
    {
      r = 0.0; // gport->bg_color.red   / 65535.;
      g = 0.0; // gport->bg_color.green / 65535.;
      b = 1.0; // gport->bg_color.blue  / 65535.;
    }
  else if (strcmp (hidcairo_gc->colorname, "drill") == 0)
    {
      r = 1.0; // gport->offlimits_color.red   / 65535.;
      g = 0.0; // gport->offlimits_color.green / 65535.;
      b = 0.0; // gport->offlimits_color.blue  / 65535.;
    }
  else
    {
      if (hid_cache_color (0, hidcairo_gc->colorname, &cval, &cache))
        cc = (ColorCache *) cval.ptr;
      else
        {
          cc = (ColorCache *) malloc (sizeof (ColorCache));
          memset (cc, 0, sizeof (*cc));
          cval.ptr = cc;
          hid_cache_color (1, hidcairo_gc->colorname, &cval, &cache);
        }

      if (!cc->color_set)
        {
          if (gdk_color_parse (hidcairo_gc->colorname, &cc->color))
            gdk_color_alloc (priv->colormap, &cc->color);
          else
            gdk_color_white (priv->colormap, &cc->color);
          cc->red   = cc->color.red   / 65535.;
          cc->green = cc->color.green / 65535.;
          cc->blue  = cc->color.blue  / 65535.;
          cc->color_set = 1;
        }
      r = cc->red;
      g = cc->green;
      b = cc->blue;
    }

  r *= hidcairo_gc->brightness;
  g *= hidcairo_gc->brightness;
  b *= hidcairo_gc->brightness;

  /* B/W Equivalent brightness */
  luminance = (r + g + b) / 3.0;

  /* Fade between B/W and colour */
  r = r * hidcairo_gc->saturation + luminance * (1.0 - hidcairo_gc->saturation);
  g = g * hidcairo_gc->saturation + luminance * (1.0 - hidcairo_gc->saturation);
  b = b * hidcairo_gc->saturation + luminance * (1.0 - hidcairo_gc->saturation);

  cairo_set_source_rgb (priv->cr, r, g, b);
}

static void
use_gc (hidGC gc)
{
  render_priv *priv = gc->hid_draw->priv;
  hidcairoGC hidcairo_gc = (hidcairoGC)gc;

  cairo_set_line_cap (priv->cr, hidcairo_gc->line_cap);
  cairo_set_line_join (priv->cr, hidcairo_gc->line_join);
  cairo_set_line_width (priv->cr, hidcairo_gc->line_width);
  _set_color (gc);
}

static void
hidcairo_set_color (hidGC gc, const char *colorname)
{
  hidcairoGC hidcairo_gc = (hidcairoGC)gc;

  if (hidcairo_gc->colorname != NULL && strcmp (hidcairo_gc->colorname, colorname) == 0)
    return;

  free (hidcairo_gc->colorname);
  hidcairo_gc->colorname = strdup (colorname);

//  _set_color (gc);
}

// static void
// hidcairo_saturation (hidGC gc, double saturation)
// {
//   if (hidcairo_gc->saturation == saturation)
//     return;

//   hidcairo_gc->saturation = saturation;
//   _set_color (gc);
// }

// static void
// hidcairo_set_brightness (hidGC gc, double brightness)
// {
//   if (hidcairo_gc->brightness == brightness)
//     return;

//   hidcairo_gc->brightness = brightness;
//   _set_color (gc);
// }

static void
hidcairo_set_line_cap (hidGC gc, EndCapStyle style)
{
  hidcairoGC hidcairo_gc = (hidcairoGC)gc;
  cairo_line_cap_t cairo_cap;
  cairo_line_join_t cairo_join;

  switch (style)
    {
      default:
      case Round_Cap:   cairo_cap = CAIRO_LINE_CAP_ROUND;  cairo_join = CAIRO_LINE_JOIN_MITER; break;
      case Beveled_Cap: cairo_cap = CAIRO_LINE_CAP_SQUARE; cairo_join = CAIRO_LINE_JOIN_BEVEL; break;
      case Trace_Cap:   cairo_cap = CAIRO_LINE_CAP_ROUND;  cairo_join = CAIRO_LINE_JOIN_MITER; break;
      case Square_Cap:  cairo_cap = CAIRO_LINE_CAP_SQUARE; cairo_join = CAIRO_LINE_JOIN_MITER; break;
    }

  hidcairo_gc->line_cap = cairo_cap;
  hidcairo_gc->line_join = cairo_join;
}

static void
hidcairo_set_line_width (hidGC gc, Coord width)
{
  render_priv *priv = gc->hid_draw->priv;
  hidcairoGC hidcairo_gc = (hidcairoGC)gc;
  hidcairo_gc->line_width = width;

  if (width == 0)
    {
      double dx = 1.5; /* XXX: Min line width in device coordinates... for now lets assume pixels */
      double dy = 0.0;

      cairo_device_to_user_distance (priv->cr, &dx, &dy);
      hidcairo_gc->line_width = hypot (dx, dy); /* In case of rotation transform */
    }
}

static void
hidcairo_set_draw_xor (hidGC gc, int xor_)
{
  /* XXX: NOP */
}

static void
hidcairo_set_draw_faded (hidGC gc, int faded)
{
  /* XXX: NOP */
}

static void
hidcairo_draw_line (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2)
{
  render_priv *priv = gc->hid_draw->priv;

  use_gc (gc);
  cairo_move_to (priv->cr, x1, y1);
  cairo_line_to (priv->cr, x2, y2);
  cairo_stroke (priv->cr);
}

static void
hidcairo_draw_arc (hidGC gc, Coord cx, Coord cy, Coord xradius, Coord yradius, Angle start_angle, Angle delta_angle)
{
  render_priv *priv = gc->hid_draw->priv;

  /* Unfortunately we can get called with zero radius arcs when thindrawing solder mask clearance in vias in some cases */
  if (xradius == 0 || yradius == 0)
    return;

  g_return_if_fail (xradius != 0 || yradius != 0);

  use_gc (gc);
  cairo_save (priv->cr);
  cairo_translate (priv->cr, cx, cy);
  cairo_scale (priv->cr, xradius, yradius);
  if (cairo_status (priv->cr) != CAIRO_STATUS_SUCCESS)
    {
      printf ("Oh crap, xradius = %li, yradius = %li. status = %i\n", xradius, yradius, cairo_status (priv->cr));
    }

  if (delta_angle < 0.0)
    cairo_arc          (priv->cr, 0.0, 0.0, 1.0, (180 - start_angle) * M_PI / 180., (180 - start_angle - delta_angle) * M_PI / 180.);
  else
    cairo_arc_negative (priv->cr, 0.0, 0.0, 1.0, (180 - start_angle) * M_PI / 180., (180 - start_angle - delta_angle) * M_PI / 180.);

  cairo_restore (priv->cr);
  cairo_stroke (priv->cr);
}

static void
hidcairo_draw_rect (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2)
{
  render_priv *priv = gc->hid_draw->priv;

  use_gc (gc);
  cairo_rectangle (priv->cr, x1, y1, x2 - x1, y2 - y1);
  cairo_stroke (priv->cr);
}

static void
hidcairo_fill_circle (hidGC gc, Coord cx, Coord cy, Coord radius)
{
  render_priv *priv = gc->hid_draw->priv;

  use_gc (gc);
  cairo_arc (priv->cr, cx, cy, radius, 0.0, M_PI * 2.0);
  cairo_fill (priv->cr);
}

static void
hidcairo_fill_polygon (hidGC gc, int n_coords, Coord *x, Coord *y)
{
  render_priv *priv = gc->hid_draw->priv;
  int i;

  g_return_if_fail (n_coords > 1);

  use_gc (gc);
  cairo_move_to (priv->cr, x[0], y[0]);
  for (i = 1; i < n_coords; i++)
    cairo_line_to (priv->cr, x[i], y[i]);
  cairo_close_path (priv->cr);
  cairo_fill (priv->cr);
}

static void
hidcairo_fill_rect (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2)
{
  render_priv *priv = gc->hid_draw->priv;

  use_gc (gc);
  cairo_rectangle (priv->cr, x1, y1, x2 - x1, y2 - y1);
  cairo_fill (priv->cr);
}


/* The following APIs render using PCB data-structures, not immediate parameters */

// USE COMMON ROUTINE
// static void
// hidcairo_draw_pcb_line (hidGC gc, LineType *line)
// {
// }

// USE COMMON ROUTINE
// static void
// hidcairo_draw_pcb_arc (hidGC gc, ArcType *arc)
// {
// }

// USE COMMON ROUTINE
// static void
// hidcairo_draw_pcb_text (hidGC gc, TextType *text, Coord min_line_width)
// {
// }

// USE COMMON ROUTINE
// static void
// hidcairo_draw_pcb_polygon (hidGC gc, PolygonType *poly, const BoxType *clip_box)
// {
// }

// USE COMMON ROUTINE
// static void
// hidcairo_draw_pcb_pad (hidGC gc, PadType *pad, bool clip, bool mask)
// {
// }

// USE COMMON ROUTINE
// static void
// hidcairo_draw_pcb_pv (hidGC fg_gc, hidGC bg_gc, PinType *pv, bool drawHole, bool mask)
// {
// }

/* NB: Caller is responsible for calling use_gc (gc) */
static void
fill_polyarea (hidGC gc, POLYAREA * pa)
{
  /* Ignore clip_box, just draw everything */

  render_priv *priv = gc->hid_draw->priv;
  VNODE *v;
  PLINE *pl;

  pl = pa->contours;

  do
    {
      v = &pl->head;
      cairo_move_to (priv->cr, v->point[0], v->point[1]);
      v = v->next;
      do
        {
          cairo_line_to (priv->cr, v->point[0], v->point[1]);
        }
      while ((v = v->next) != &pl->head);
    }
  while ((pl = pl->next) != NULL);

  cairo_fill (priv->cr);
}

static void
hidcairo_fill_pcb_polygon (hidGC gc, PolygonType *poly)
{
  use_gc (gc);
  fill_polyarea (gc, poly->Clipped);
  if (TEST_FLAG (FULLPOLYFLAG, poly))
    {
      POLYAREA *pa;

      for (pa = poly->Clipped->f; pa != poly->Clipped; pa = pa->f)
        fill_polyarea (gc, pa);
    }
}

// USE COMMON ROUTINE
// static void
// hidcairo_thindraw_pcb_polygon (hidGC gc, PolygonType *poly, const BoxType *clip_box)
// {
// }

// USE COMMON ROUTINE
// static void
// hidcairo_fill_pcb_pad (hidGC gc, PadType *pad, bool clip, bool mask)
// {
// }

// USE COMMON ROUTINE
// static void
// hidcairo_thindraw_pcb_pad (hidGC gc, PadType *pad, bool clip, bool mask)
// {
// }

// USE COMMON ROUTINE
// static void
// hidcairo_fill_pcb_pv (hidGC fg_gc, hidGC bg_gc, PinType *pv, bool drawHole, bool mask)
// {
// }

// USE COMMON ROUTINE
// static void
// hidcairo_thindraw_pcb_pv (hidGC fg_gc, hidGC bg_gc, PinType *pv, bool drawHole, bool mask)
// {
// }

void
hidcairo_class_init (HID_DRAW_CLASS *klass)
{
//  memset (&klass, 0, sizeof (HID_DRAW));

//  common_draw_helpers_init (&klass);

  klass->make_gc               = hidcairo_make_gc;
  klass->destroy_gc            = hidcairo_destroy_gc;
  klass->use_mask              = hidcairo_use_mask;

  klass->set_line_cap          = hidcairo_set_line_cap;
  klass->set_line_width        = hidcairo_set_line_width;

  klass->set_color             = hidcairo_set_color;
  klass->set_draw_xor          = hidcairo_set_draw_xor;
  klass->set_draw_faded        = hidcairo_set_draw_faded;

  klass->draw_line             = hidcairo_draw_line;
  klass->draw_arc              = hidcairo_draw_arc;
  klass->draw_rect             = hidcairo_draw_rect;
  klass->fill_circle           = hidcairo_fill_circle;
  klass->fill_polygon          = hidcairo_fill_polygon;
  klass->fill_rect             = hidcairo_fill_rect;

  /* The following APIs render using PCB data-structures, not immediate parameters */

  // USE COMMON ROUTINE: klass->draw_pcb_line         = hidcairo_draw_pcb_line;
  // USE COMMON ROUTINE: klass->draw_pcb_arc          = hidcairo_draw_pcb_arc;
  // USE COMMON ROUTINE: klass->draw_pcb_text         = hidcairo_draw_pcb_text;
  // USE COMMON ROUTINE: klass->draw_pcb_polygon      = hidcairo_draw_pcb_polygon;
  // USE COMMON ROUTINE: klass->draw_pcb_polygon      = hidcairo_gui_draw_pcb_polygon;
  // USE COMMON ROUTINE: klass->draw_pcb_pad          = hidcairo_draw_pcb_pad;
  // USE COMMON ROUTINE: klass->draw_pcb_pv           = hidcairo_draw_pcb_pv;
  klass->draw_pcb_polygon      = common_gui_draw_pcb_polygon;      // USE COMMON GUI ROUTINE
  klass->draw_pcb_pad          = common_gui_draw_pcb_pad;          // USE COMMON GUI ROUTINE
  klass->draw_pcb_pv           = common_gui_draw_pcb_pv;           // USE COMMON GUI ROUTINE

  /* The following are not meant to be called outside of the GUI implementations of the above APIs */
  klass->_fill_pcb_polygon     = hidcairo_fill_pcb_polygon;
  // USE COMMON ROUTINE: klass->_thindraw_pcb_polygon = hidcairo_thindraw_pcb_polygon;
  // USE COMMON ROUTINE: klass->_fill_pcb_pad         = hidcairo_fill_pcb_pad;
  // USE COMMON ROUTINE: klass->_thindraw_pcb_pad     = hidcairo_thindraw_pcb_pad;
  // USE COMMON ROUTINE: klass->_fill_pcb_pv          = hidcairo_fill_pcb_pv;
  // USE COMMON ROUTINE: klass->_thindraw_pcb_pv      = hidcairo_thindraw_pcb_pv;
}

void
hidcairo_init (HID_DRAW *hid_draw)
{
}

void
hidcairo_start_render (HID_DRAW *hid_draw, cairo_t *cr)
{
  render_priv *priv;

  g_return_if_fail (hid_draw->priv == NULL);

  priv = (render_priv *)calloc (1, sizeof (render_priv));
  hid_draw->priv = priv;

  cairo_reference (cr);
  priv->cr = cr;
}

void
hidcairo_finish_render (HID_DRAW *hid_draw)
{
  render_priv *priv = hid_draw->priv;

  cairo_destroy (priv->cr);
  free (priv);

  hid_draw->priv = NULL;
}
