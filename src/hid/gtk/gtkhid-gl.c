/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "crosshair.h"
#include "clip.h"
#include "../hidint.h"
#include "gui.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif


RCSID ("$Id$");


extern HID ghid_hid;
extern int ghid_gui_is_up;

static hidGC current_gc = NULL;

/* Sets gport->u_gc to the "right" GC to use (wrt mask or window)
*/
#define USE_GC(gc) if (!use_gc(gc)) return

static int cur_mask = -1;


typedef struct hid_gc_struct
{
  HID *me_pointer;

  gchar *colorname;
  gint width;
  gint cap, join;
  gchar xor;
  gchar erase;
}
hid_gc_struct;


void
ghid_destroy_gc (hidGC gc)
{
  g_free (gc);
}

hidGC
ghid_make_gc (void)
{
  hidGC rv;

  rv = g_new0 (hid_gc_struct, 1);
  rv->me_pointer = &ghid_hid;
  rv->colorname = Settings.BackgroundColor;
  return rv;
}

void
ghid_draw_grid ()
{
  static GLfloat *points = 0;
  static int npoints = 0;
  int x1, y1, x2, y2, n, i;
  double x, y;

  if (!Settings.DrawGrid)
    return;
  if (Vz (PCB->Grid) < MIN_GRID_DISTANCE)
    return;

  if (gdk_color_parse (Settings.GridColor, &gport->grid_color))
    {
      gport->grid_color.red ^= gport->bg_color.red;
      gport->grid_color.green ^= gport->bg_color.green;
      gport->grid_color.blue ^= gport->bg_color.blue;
    }

  hidgl_flush_triangles (&buffer);

  glEnable (GL_COLOR_LOGIC_OP);
  glLogicOp (GL_XOR);

  glColor3f (gport->grid_color.red / 65535.,
             gport->grid_color.green / 65535.,
             gport->grid_color.blue / 65535.);

  x1 = GRIDFIT_X (SIDE_X (gport->view_x0), PCB->Grid);
  y1 = GRIDFIT_Y (SIDE_Y (gport->view_y0), PCB->Grid);
  x2 = GRIDFIT_X (SIDE_X (gport->view_x0 + gport->view_width - 1), PCB->Grid);
  y2 = GRIDFIT_Y (SIDE_Y (gport->view_y0 + gport->view_height - 1), PCB->Grid);
  if (x1 > x2)
    {
      int tmp = x1;
      x1 = x2;
      x2 = tmp;
    }
  if (y1 > y2)
    {
      int tmp = y1;
      y1 = y2;
      y2 = tmp;
    }
  if (Vx (x1) < 0)
    x1 += PCB->Grid;
  if (Vy (y1) < 0)
    y1 += PCB->Grid;
  if (Vx (x2) >= gport->width)
    x2 -= PCB->Grid;
  if (Vy (y2) >= gport->height)
    y2 -= PCB->Grid;
  n = (int) ((x2 - x1) / PCB->Grid + 0.5) + 1;
  if (n > npoints)
    {
      npoints = n + 10;
      points =
	MyRealloc (points, npoints * 2 * sizeof (GLfloat), "gtk_draw_grid");
    }

  glEnableClientState (GL_VERTEX_ARRAY);
  glVertexPointer (2, GL_FLOAT, 0, points);

  n = 0;
  for (x = x1; x <= x2; x += PCB->Grid)
    {
      points[2 * n] = Vx (x);
      n++;
    }
  for (y = y1; y <= y2; y += PCB->Grid)
    {
      int vy = Vy (y);
      for (i = 0; i < n; i++)
	points[2 * i + 1] = vy;
      glDrawArrays (GL_POINTS, 0, n);
    }

  glDisableClientState (GL_VERTEX_ARRAY);
  glDisable (GL_COLOR_LOGIC_OP);
}

#if 0
void
ghid_draw_bg_image (void)
{
  static GdkPixbuf *pixbuf = NULL;
  static gint vw_scaled, vh_scaled, x_cached, y_cached;
  GdkInterpType interp_type;
  gint x, y, vw, vh, w, h, w_src, h_src;
  int bits_per_sample;
  gboolean has_alpha;

  if (!ghidgui->bg_pixbuf)
    return;

  w = PCB->MaxWidth / gport->zoom;
  h = PCB->MaxHeight / gport->zoom;
  x = gport->view_x0 / gport->zoom;
  y = gport->view_y0 / gport->zoom;
  vw = gport->view_width / gport->zoom;
  vh = gport->view_height / gport->zoom;

  if (pixbuf == NULL || vw_scaled != vw || vh_scaled != vh)
    {
      if (pixbuf != NULL)
        g_object_unref(G_OBJECT(pixbuf));

      bits_per_sample = gdk_pixbuf_get_bits_per_sample(ghidgui->bg_pixbuf);
      has_alpha = gdk_pixbuf_get_has_alpha (ghidgui->bg_pixbuf);
      pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                              has_alpha,
                              bits_per_sample,
                              vw, vh);
    }

  if (pixbuf == NULL)
    return;

  if (vw_scaled != vw || vh_scaled != vh ||
       x_cached != x  ||  y_cached != y)
    {
      w_src = gdk_pixbuf_get_width(ghidgui->bg_pixbuf);
      h_src = gdk_pixbuf_get_height(ghidgui->bg_pixbuf);

      if (w > w_src && h > h_src)
        interp_type = GDK_INTERP_NEAREST;
      else
        interp_type = GDK_INTERP_BILINEAR;

      gdk_pixbuf_scale(ghidgui->bg_pixbuf, pixbuf,
                       0, 0, vw, vh,
                       (double) -x,
                       (double) -y,
                       (double) w / w_src,
                       (double) h / h_src,
                       interp_type);

      x_cached = x;
      y_cached = y;
      vw_scaled = vw;
      vh_scaled = vh;
    }

  if (pixbuf != NULL)
    gdk_draw_pixbuf(gport->drawable, gport->bg_gc, pixbuf,
                    0, 0, 0, 0, vw, vh, GDK_RGB_DITHER_NORMAL, 0, 0);
}
#endif


void
ghid_use_mask (int use_it)
{
  static int stencil_bit = 0;

  /* THE FOLLOWING IS COMPLETE ABUSE OF THIS MASK RENDERING API... NOT IMPLEMENTED */
  if (use_it == HID_LIVE_DRAWING ||
      use_it == HID_LIVE_DRAWING_OFF ||
      use_it == HID_FLUSH_DRAW_Q) {
    return;
  }

  if (use_it == cur_mask)
    return;

  /* Flush out any existing geoemtry to be rendered */
  hidgl_flush_triangles (&buffer);

  switch (use_it)
    {
    case HID_MASK_BEFORE:
      /* Write '1' to the stencil buffer where the solder-mask is drawn. */
      glColorMask (0, 0, 0, 0);                   // Disable writting in color buffer
      glEnable (GL_STENCIL_TEST);                 // Enable Stencil test
      stencil_bit = hidgl_assign_clear_stencil_bit();       // Get a new (clean) bitplane to stencil with
      glStencilFunc (GL_ALWAYS, stencil_bit, stencil_bit);  // Always pass stencil test, write stencil_bit
      glStencilMask (stencil_bit);                          // Only write to our subcompositing stencil bitplane
      glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE); // Stencil pass => replace stencil value (with 1)
      break;

    case HID_MASK_CLEAR:
      /* Drawing operations clear the stencil buffer to '0' */
      glStencilFunc (GL_ALWAYS, 0, stencil_bit);  // Always pass stencil test, write 0
      glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE); // Stencil pass => replace stencil value (with 0)
      break;

    case HID_MASK_AFTER:
      /* Drawing operations as masked to areas where the stencil buffer is '1' */
      glColorMask (1, 1, 1, 1);                   // Enable drawing of r, g, b & a
      glStencilFunc (GL_LEQUAL, stencil_bit, stencil_bit);   // Draw only where our bit of the stencil buffer is set
      glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);    // Stencil buffer read only
      break;

    case HID_MASK_OFF:
      /* Disable stenciling */
      hidgl_return_stencil_bit (stencil_bit);               // Relinquish any bitplane we previously used
      glDisable (GL_STENCIL_TEST);                // Disable Stencil test
      break;
    }
  cur_mask = use_it;
}


  /* Config helper functions for when the user changes color preferences.
     |  set_special colors used in the gtkhid.
   */
static void
set_special_grid_color (void)
{
  if (!gport->colormap)
    return;
  gport->grid_color.red ^= gport->bg_color.red;
  gport->grid_color.green ^= gport->bg_color.green;
  gport->grid_color.blue ^= gport->bg_color.blue;
//  gdk_color_alloc (gport->colormap, &gport->grid_color);
}

void
ghid_set_special_colors (HID_Attribute * ha)
{
  if (!ha->name || !ha->value)
    return;
  if (!strcmp (ha->name, "background-color"))
    {
      ghid_map_color_string (*(char **) ha->value, &gport->bg_color);
      set_special_grid_color ();
    }
  else if (!strcmp (ha->name, "off-limit-color"))
  {
      ghid_map_color_string (*(char **) ha->value, &gport->offlimits_color);
    }
  else if (!strcmp (ha->name, "grid-color"))
    {
      ghid_map_color_string (*(char **) ha->value, &gport->grid_color);
      set_special_grid_color ();
    }
}

typedef struct
{
  int color_set;
  GdkColor color;
  int xor_set;
  GdkColor xor_color;
  double red;
  double green;
  double blue;
} ColorCache;

void
ghid_set_color (hidGC gc, const char *name)
{
  static void *cache = NULL;
  static char *old_name = NULL;
  hidval cval;
  ColorCache *cc;
  double alpha_mult = 1.0;
  double r, g, b, a;
  a = 1.0;

  if (old_name != NULL)
    {
      if (strcmp (name, old_name) == 0)
        return;
      free (old_name);
    }

  old_name = strdup (name);

  if (name == NULL)
    {
      fprintf (stderr, "%s():  name = NULL, setting to magenta\n",
               __FUNCTION__);
      name = "magenta";
    }

  gc->colorname = (char *) name;

  if (gport->colormap == 0)
    gport->colormap = gtk_widget_get_colormap (gport->top_window);
  if (strcmp (name, "erase") == 0)
    {
      gc->erase = 1;
      r = gport->bg_color.red   / 65535.;
      g = gport->bg_color.green / 65535.;
      b = gport->bg_color.blue  / 65535.;
    }
  else if (strcmp (name, "drill") == 0)
    {
      gc->erase = 0;
      alpha_mult = 0.85;
      r = gport->offlimits_color.red   / 65535.;
      g = gport->offlimits_color.green / 65535.;
      b = gport->offlimits_color.blue  / 65535.;
    }
  else
    {
      alpha_mult = 0.7;
      if (hid_cache_color (0, name, &cval, &cache))
        cc = (ColorCache *) cval.ptr;
      else
        {
          cc = (ColorCache *) malloc (sizeof (ColorCache));
          memset (cc, 0, sizeof (*cc));
          cval.ptr = cc;
          hid_cache_color (1, name, &cval, &cache);
        }

      if (!cc->color_set)
        {
          if (gdk_color_parse (name, &cc->color))
            gdk_color_alloc (gport->colormap, &cc->color);
          else
            gdk_color_white (gport->colormap, &cc->color);
          cc->red   = cc->color.red   / 65535.;
          cc->green = cc->color.green / 65535.;
          cc->blue  = cc->color.blue  / 65535.;
          cc->color_set = 1;
        }
      if (gc->xor)
        {
          if (!cc->xor_set)
            {
              cc->xor_color.red = cc->color.red ^ gport->bg_color.red;
              cc->xor_color.green = cc->color.green ^ gport->bg_color.green;
              cc->xor_color.blue = cc->color.blue ^ gport->bg_color.blue;
              gdk_color_alloc (gport->colormap, &cc->xor_color);
              cc->red   = cc->color.red   / 65535.;
              cc->green = cc->color.green / 65535.;
              cc->blue  = cc->color.blue  / 65535.;
              cc->xor_set = 1;
            }
        }
      r = cc->red;
      g = cc->green;
      b = cc->blue;

      gc->erase = 0;
    }
  if (1) {
    double maxi, mult;
    if (gport->trans_lines)
      a = a * alpha_mult;
    maxi = r;
    if (g > maxi) maxi = g;
    if (b > maxi) maxi = b;
    mult = MIN (1 / alpha_mult, 1 / maxi);
#if 1
    r = r * mult;
    g = g * mult;
    b = b * mult;
#endif
  }

  if( ! ghid_gui_is_up )
    return;

  hidgl_flush_triangles (&buffer);
  glColor4d (r, g, b, a);
}

void
ghid_set_line_cap (hidGC gc, EndCapStyle style)
{
  gc->cap = style;
}

void
ghid_set_line_width (hidGC gc, int width)
{
  gc->width = width;
}


void
ghid_set_draw_xor (hidGC gc, int xor)
{
  // printf ("ghid_set_draw_xor (%p, %d) -- not implemented\n", gc, xor);
  /* NOT IMPLEMENTED */

  /* Only presently called when setting up a crosshair GC.
   * We manage our own drawing model for that anyway. */
}

void
ghid_set_draw_faded (hidGC gc, int faded)
{
  printf ("ghid_set_draw_faded(%p,%d) -- not implemented\n", gc, faded);
}

void
ghid_set_line_cap_angle (hidGC gc, int x1, int y1, int x2, int y2)
{
  printf ("ghid_set_line_cap_angle() -- not implemented\n");
}

void
ghid_invalidate_current_gc (void)
{
  current_gc = NULL;
}

static int
use_gc (hidGC gc)
{
  if (gc->me_pointer != &ghid_hid)
    {
      fprintf (stderr, "Fatal: GC from another HID passed to GTK HID\n");
      abort ();
    }

  if (current_gc == gc)
    return 1;

  current_gc = gc;

  ghid_set_color (gc, gc->colorname);
  return 1;
}

void
ghid_draw_line (hidGC gc, int x1, int y1, int x2, int y2)
{
  USE_GC (gc);

  hidgl_draw_line (gc->cap, gc->width, x1, y1, x2, y2, gport->zoom);
}

void
ghid_draw_arc (hidGC gc, int cx, int cy, int xradius, int yradius,
                         int start_angle, int delta_angle)
{
  USE_GC (gc);

  hidgl_draw_arc (gc->width, cx, cy, xradius, yradius,
                  start_angle, delta_angle, gport->zoom);
}

void
ghid_draw_rect (hidGC gc, int x1, int y1, int x2, int y2)
{
  USE_GC (gc);

  hidgl_draw_rect (x1, y1, x2, y2);
}


void
ghid_fill_circle (hidGC gc, int cx, int cy, int radius)
{
  USE_GC (gc);

  hidgl_fill_circle (cx, cy, radius, gport->zoom);
}


void
ghid_fill_polygon (hidGC gc, int n_coords, int *x, int *y)
{
  USE_GC (gc);

  hidgl_fill_polygon (n_coords, x, y);
}

void
ghid_fill_pcb_polygon (hidGC gc, PolygonType *poly, const BoxType *clip_box)
{
  USE_GC (gc);

  hidgl_fill_pcb_polygon (poly, clip_box, gport->zoom);
}

void
ghid_fill_rect (hidGC gc, int x1, int y1, int x2, int y2)
{
  USE_GC (gc);

  hidgl_fill_rect (x1, y1, x2, y2);
}
