/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "crosshair.h"
#include "clip.h"
#include "../hidint.h"
#include "gui.h"
#include "draw.h"
#include "rtree.h"
#include "polygon.h"
#include "gui-pinout-preview.h"

/* The Linux OpenGL ABI 1.0 spec requires that we define
 * GL_GLEXT_PROTOTYPES before including gl.h or glx.h for extensions
 * in order to get prototypes:
 *   http://www.opengl.org/registry/ABI/
 */
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <gtk/gtkgl.h>
#include "hid/common/hidgl.h"

#include "hid/common/draw_helpers.h"
#include "hid/common/trackball.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif


RCSID ("$Id$");


extern HID ghid_hid;
extern int ghid_gui_is_up;

static void ghid_global_alpha_mult (hidGC, double);

static hidGC current_gc = NULL;
static char *current_color = NULL;
static double global_alpha_mult = 1.0;
static int alpha_changed = 0;
static bool check_gl_drawing_ok_hack = false;


/* Sets gport->u_gc to the "right" GC to use (wrt mask or window)
*/
#define USE_GC(gc) if (!use_gc(gc)) return

static int cur_mask = -1;
static GLfloat view_matrix[4][4] = {{1.0, 0.0, 0.0, 0.0},
                                    {0.0, 1.0, 0.0, 0.0},
                                    {0.0, 0.0, 1.0, 0.0},
                                    {0.0, 0.0, 0.0, 1.0}};
static GLfloat last_modelview_matrix[4][4] = {{1.0, 0.0, 0.0, 0.0},
                                              {0.0, 1.0, 0.0, 0.0},
                                              {0.0, 0.0, 1.0, 0.0},
                                              {0.0, 0.0, 0.0, 1.0}};
static int global_view_2d = 1;

typedef struct render_priv {
  GdkGLConfig *glconfig;
  bool trans_lines;
} render_priv;


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


static int
compute_depth (int group)
{
  static int last_depth_computed = 0;

  int solder_group;
  int component_group;
  int min_phys_group;
  int max_phys_group;
  int max_depth;
  int depth = last_depth_computed;
  int newgroup;
  int idx = (group >= 0
             && group <
             max_group) ? PCB->LayerGroups.Entries[group][0] : group;

  solder_group = GetLayerGroupNumberByNumber (solder_silk_layer);
  component_group = GetLayerGroupNumberByNumber (component_silk_layer);

  min_phys_group = MIN (solder_group, component_group);
  max_phys_group = MAX (solder_group, component_group);

  max_depth = (1 + max_phys_group - min_phys_group) * 10;

  if (group >= 0 && group < max_group) {
    newgroup = group;

    depth = (max_depth - (newgroup - min_phys_group) * 10) * 200 / gport->zoom;
  } else if (SL_TYPE (idx) == SL_MASK) {
    if (SL_SIDE (idx) == SL_TOP_SIDE) {
      depth = (max_depth + 3) * 200 / gport->zoom;
    } else {
      depth = (10 - 3) * 200 / gport->zoom;
    }
  } else if (SL_TYPE (idx) == SL_SILK) {
    if (SL_SIDE (idx) == SL_TOP_SIDE) {
      depth = (max_depth + 5) * 200 / gport->zoom;
    } else {
      depth = (10 - 5) * 200 / gport->zoom;
    }
  } else if (SL_TYPE (idx) == SL_INVISIBLE) {
    if (Settings.ShowSolderSide) {
      depth = (max_depth + 5) * 200 / gport->zoom;
    } else {
      depth = (10 - 5) * 200 / gport->zoom;
    }
  }

  last_depth_computed = depth;
  return depth;
}

int
ghid_set_layer (const char *name, int group, int empty)
{
  render_priv *priv = gport->render_priv;
  static int stencil_bit = 0;
  int idx = group;
  if (idx >= 0 && idx < max_group)
    {
      int n = PCB->LayerGroups.Number[group];
      for (idx = 0; idx < n-1; idx ++)
	{
	  int ni = PCB->LayerGroups.Entries[group][idx];
	  if (ni >= 0 && ni < max_copper_layer + 2
	      && PCB->Data->Layer[ni].On)
	    break;
	}
      idx = PCB->LayerGroups.Entries[group][idx];
    }

  /* Flush out any existing geoemtry to be rendered */
  hidgl_flush_triangles (&buffer);

  hidgl_set_depth (compute_depth (group));

  glEnable (GL_STENCIL_TEST);                   // Enable Stencil test
  glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE);   // Stencil pass => replace stencil value (with 1)
  hidgl_return_stencil_bit (stencil_bit);       // Relinquish any bitplane we previously used
  if (SL_TYPE (idx) != SL_FINISHED) {
    stencil_bit = hidgl_assign_clear_stencil_bit();       // Get a new (clean) bitplane to stencil with
    glStencilMask (stencil_bit);                          // Only write to our subcompositing stencil bitplane
    glStencilFunc (GL_GREATER, stencil_bit, stencil_bit); // Pass stencil test if our assigned bit is clear
  } else {
    stencil_bit = 0;
    glStencilMask (0);
    glStencilFunc (GL_ALWAYS, 0, 0);  // Always pass stencil test
  }

  if (idx >= 0 && idx < max_copper_layer + 2)
    {
      priv->trans_lines = true;
      return PCB->Data->Layer[idx].On;
    }

  if (idx < 0)
    {
      switch (SL_TYPE (idx))
	{
	case SL_INVISIBLE:
	  return PCB->InvisibleObjectsOn;
	case SL_MASK:
	  if (SL_MYSIDE (idx))
	    return TEST_FLAG (SHOWMASKFLAG, PCB);
	  return 0;
	case SL_SILK:
	  priv->trans_lines = true;
	  if (SL_MYSIDE (idx))
	    return PCB->ElementOn;
	  return 0;
	case SL_ASSY:
	  return 0;
	case SL_PDRILL:
	case SL_UDRILL:
	  return 1;
	case SL_RATS:
	  if (PCB->RatOn)
	    priv->trans_lines = true;
	  return PCB->RatOn;
	}
    }
  return 0;
}

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

static void
ghid_draw_grid (BoxTypePtr drawn_area)
{
  static GLfloat *points = 0;
  static int npoints = 0;
  int x1, y1, x2, y2, n, i;
  double x, y;
  extern float global_depth;

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

  x1 = GRIDFIT_X (MAX (0, drawn_area->X1), PCB->Grid);
  y1 = GRIDFIT_Y (MAX (0, drawn_area->Y1), PCB->Grid);
  x2 = GRIDFIT_X (MIN (PCB->MaxWidth, drawn_area->X2), PCB->Grid);
  y2 = GRIDFIT_Y (MIN (PCB->MaxHeight, drawn_area->Y2), PCB->Grid);
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
      points = realloc (points, npoints * 3 * sizeof (GLfloat));
    }

  glEnableClientState (GL_VERTEX_ARRAY);
  glVertexPointer (3, GL_FLOAT, 0, points);

  n = 0;
  for (x = x1; x <= x2; x += PCB->Grid)
    {
      points[3 * n] = Vx (x);
      points[3 * n + 2] = global_depth;
      n++;
    }
  for (y = y1; y <= y2; y += PCB->Grid)
    {
      int vy = Vy (y);
      for (i = 0; i < n; i++)
	points[3 * i + 1] = vy;
      glDrawArrays (GL_POINTS, 0, n);
    }

  glDisableClientState (GL_VERTEX_ARRAY);
  glDisable (GL_COLOR_LOGIC_OP);
}

#if 0
static void
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
  render_priv *priv = gport->render_priv;
  static void *cache = NULL;
  hidval cval;
  ColorCache *cc;
  double alpha_mult = 1.0;
  double r, g, b, a;
  a = 1.0;

  if (!alpha_changed && current_color != NULL)
    {
      if (strcmp (name, current_color) == 0)
        return;
      free (current_color);
    }

  alpha_changed = 0;

  current_color = strdup (name);

  if (name == NULL)
    {
      fprintf (stderr, "%s():  name = NULL, setting to magenta\n",
               __FUNCTION__);
      name = "magenta";
    }

  gc->colorname = (char *) name;

  if (!check_gl_drawing_ok_hack)
    {
      current_color = NULL;
      return;
    }

  if (gport->colormap == NULL)
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
    alpha_mult *= global_alpha_mult;
    if (priv->trans_lines)
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

static void
ghid_global_alpha_mult (hidGC gc, double alpha_mult)
{
  if (alpha_mult != global_alpha_mult) {
    global_alpha_mult = alpha_mult;
    alpha_changed = 1;
    ghid_set_color (gc, current_color);
  }
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
ghid_thindraw_pcb_polygon (hidGC gc, PolygonType *poly, const BoxType *clip_box)
{
  common_thindraw_pcb_polygon (gc, poly, clip_box);
  ghid_global_alpha_mult (gc, 0.25);
  ghid_fill_pcb_polygon (gc, poly, clip_box);
  ghid_global_alpha_mult (gc, 1.0);
}

void
ghid_fill_rect (hidGC gc, int x1, int y1, int x2, int y2)
{
  USE_GC (gc);

  hidgl_fill_rect (x1, y1, x2, y2);
}

void
ghid_invalidate_lr (int left, int right, int top, int bottom)
{
  ghid_invalidate_all ();
}

void
ghid_invalidate_all ()
{
  if (ghidgui->need_restore_crosshair)
    RestoreCrosshair (FALSE);
  ghidgui->need_restore_crosshair = FALSE;
  ghid_draw_area_update (gport, NULL);
}

static void
draw_right_cross (gint x, gint y, gint z)
{
  glVertex3i (x, 0, z);
  glVertex3i (x, gport->height, z);
  glVertex3i (0, y, z);
  glVertex3i (gport->width, y, z);
}

static void
draw_slanted_cross (gint x, gint y, gint z)
{
  gint x0, y0, x1, y1;

  x0 = x + (gport->height - y);
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x - y;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + (gport->width - x);
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - x;
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex3i (x0, y0, z);
  glVertex3i (x1, y1, z);

  x0 = x - (gport->height - y);
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x + y;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + x;
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - (gport->width - x);
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex3i (x0, y0, z);
  glVertex3i (x1, y1, z);
}

static void
draw_dozen_cross (gint x, gint y, gint z)
{
  gint x0, y0, x1, y1;
  gdouble tan60 = sqrt (3);

  x0 = x + (gport->height - y) / tan60;
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x - y / tan60;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + (gport->width - x) * tan60;
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - x * tan60;
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex3i (x0, y0, z);
  glVertex3i (x1, y1, z);

  x0 = x + (gport->height - y) * tan60;
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x - y * tan60;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + (gport->width - x) / tan60;
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - x / tan60;
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex3i (x0, y0, z);
  glVertex3i (x1, y1, z);

  x0 = x - (gport->height - y) / tan60;
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x + y / tan60;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + x * tan60;
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - (gport->width - x) * tan60;
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex3i (x0, y0, z);
  glVertex3i (x1, y1, z);

  x0 = x - (gport->height - y) * tan60;
  x0 = MAX(0, MIN (x0, gport->width));
  x1 = x + y * tan60;
  x1 = MAX(0, MIN (x1, gport->width));
  y0 = y + x / tan60;
  y0 = MAX(0, MIN (y0, gport->height));
  y1 = y - (gport->width - x) / tan60;
  y1 = MAX(0, MIN (y1, gport->height));
  glVertex3i (x0, y0, z);
  glVertex3i (x1, y1, z);
}

static void
draw_crosshair (gint x, gint y, gint z)
{
  static enum crosshair_shape prev = Basic_Crosshair_Shape;

  draw_right_cross (x, y, z);
  if (prev == Union_Jack_Crosshair_Shape)
    draw_slanted_cross (x, y, z);
  if (prev == Dozen_Crosshair_Shape)
    draw_dozen_cross (x, y, z);
  prev = Crosshair.shape;
}

#define VCW 16
#define VCD 8

void
ghid_show_crosshair (gboolean show)
{
  gint x, y, z;
  static gint x_prev = -1, y_prev = -1, z_prev = -1;
  static gboolean draw_markers, draw_markers_prev = FALSE;
  static int done_once = 0;
  static GdkColor cross_color;
  extern float global_depth;

  if (!check_gl_drawing_ok_hack)
    return;

  if (gport->x_crosshair < 0 || ghidgui->creating) {// || !gport->has_entered) {
    printf ("Returning\n");
    return;
  }

  if (!done_once)
    {
      done_once = 1;
      /* FIXME: when CrossColor changed from config */
      ghid_map_color_string (Settings.CrossColor, &cross_color);
    }
  x = DRAW_X (gport->x_crosshair);
  y = DRAW_Y (gport->y_crosshair);
  z = global_depth;

  glEnable (GL_COLOR_LOGIC_OP);
  glLogicOp (GL_XOR);

  hidgl_flush_triangles (&buffer);

  glColor3f (cross_color.red / 65535.,
             cross_color.green / 65535.,
             cross_color.blue / 65535.);

  glBegin (GL_LINES);

#if 0
  if (x_prev >= 0)
    draw_crosshair (x_prev, y_prev, z_prev);
#endif

  if (x >= 0 && show)
    draw_crosshair (x, y, z);

  glEnd ();


  glBegin (GL_QUADS);

#if 0
  if (x_prev >= 0 && draw_markers_prev)
    {
      glVertex3i (0,                  y_prev - VCD,        z_prev);
      glVertex3i (0,                  y_prev - VCD + VCW,  z_prev);
      glVertex3i (VCD,                y_prev - VCD + VCW,  z_prev);
      glVertex3i (VCD,                y_prev - VCD,        z_prev);
      glVertex3i (gport->width,       y_prev - VCD,        z_prev);
      glVertex3i (gport->width,       y_prev - VCD + VCW,  z_prev);
      glVertex3i (gport->width - VCD, y_prev - VCD + VCW,  z_prev);
      glVertex3i (gport->width - VCD, y_prev - VCD,        z_prev);
      glVertex3i (x_prev - VCD,       0,                   z_prev);
      glVertex3i (x_prev - VCD,       VCD,                 z_prev);
      glVertex3i (x_prev - VCD + VCW, VCD,                 z_prev);
      glVertex3i (x_prev - VCD + VCW, 0,                   z_prev);
      glVertex3i (x_prev - VCD,       gport->height - VCD, z_prev);
      glVertex3i (x_prev - VCD,       gport->height,       z_prev);
      glVertex3i (x_prev - VCD + VCW, gport->height,       z_prev);
      glVertex3i (x_prev - VCD + VCW, gport->height - VCD, z_prev);
    }
#endif

  draw_markers = ghidgui->auto_pan_on && have_crosshair_attachments ();
  if (x >= 0 && show && draw_markers)
    {
      glVertex3i (0,                  y - VCD,             z);
      glVertex3i (0,                  y - VCD + VCW,       z);
      glVertex3i (VCD,                y - VCD + VCW,       z);
      glVertex3i (VCD,                y - VCD,             z);
      glVertex3i (gport->width,       y - VCD,             z);
      glVertex3i (gport->width,       y - VCD + VCW,       z);
      glVertex3i (gport->width - VCD, y - VCD + VCW,       z);
      glVertex3i (gport->width - VCD, y - VCD,             z);
      glVertex3i (x - VCD,            0,                   z);
      glVertex3i (x - VCD,            VCD,                 z);
      glVertex3i (x - VCD + VCW,      VCD,                 z);
      glVertex3i (x - VCD + VCW,      0,                   z);
      glVertex3i (x - VCD,            gport->height - VCD, z);
      glVertex3i (x - VCD,            gport->height,       z);
      glVertex3i (x - VCD + VCW,      gport->height,       z);
      glVertex3i (x - VCD + VCW,      gport->height - VCD, z);
    }

  glEnd ();

  if (x >= 0 && show)
    {
      x_prev = x;
      y_prev = y;
      z_prev = z;
      draw_markers_prev = draw_markers;
    }
  else
    {
      x_prev = y_prev = z_prev = -1;
      draw_markers_prev = FALSE;
    }

  glDisable (GL_COLOR_LOGIC_OP);
}

void
ghid_init_renderer (int *argc, char ***argv, GHidPort *port)
{
  render_priv *priv;

  port->render_priv = priv = g_new0 (render_priv, 1);

  gtk_gl_init(argc, argv);

  /* setup GL-context */
  priv->glconfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGBA    |
                                              GDK_GL_MODE_STENCIL |
                                           // GDK_GL_MODE_DEPTH   |
                                              GDK_GL_MODE_DOUBLE);
  if (!priv->glconfig)
    {
      printf ("Could not setup GL-context!\n");
      return; /* Should we abort? */
    }
}

void
ghid_init_drawing_widget (GtkWidget *widget, GHidPort *port)
{
  render_priv *priv = port->render_priv;

  gtk_widget_set_gl_capability (widget,
                                priv->glconfig,
                                NULL,
                                TRUE,
                                GDK_GL_RGBA_TYPE);
}

void
ghid_drawing_area_configure_hook (GHidPort *port)
{
}

gboolean
ghid_start_drawing (GHidPort *port)
{
  GtkWidget *widget = port->drawing_area;
  GdkGLContext *pGlContext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (widget);

  /* make GL-context "current" */
  if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
    return FALSE;

  return TRUE;
}

void
ghid_end_drawing (GHidPort *port)
{
  GtkWidget *widget = port->drawing_area;
  GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (widget);

  if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
    gdk_gl_drawable_swap_buffers (pGlDrawable);
  else
    glFlush ();

  /* end drawing to current GL-context */
  gdk_gl_drawable_gl_end (pGlDrawable);
}

void
ghid_screen_update (void)
{
}


struct pin_info
{
  bool arg;
  LayerTypePtr Layer;
  const BoxType *drawn_area;
};

static int
backE_callback (const BoxType * b, void *cl)
{
  ElementTypePtr element = (ElementTypePtr) b;

  if (!FRONT (element))
    {
      DrawElementPackage (element, 0);
    }
  return 1;
}

static int
backN_callback (const BoxType * b, void *cl)
{
  TextTypePtr text = (TextTypePtr) b;
  ElementTypePtr element = (ElementTypePtr) text->Element;

  if (!FRONT (element) && !TEST_FLAG (HIDENAMEFLAG, element))
    DrawElementName (element, 0);
  return 0;
}

static int
backPad_callback (const BoxType * b, void *cl)
{
  PadTypePtr pad = (PadTypePtr) b;

  if (!FRONT (pad))
    DrawPad (pad, 0);
  return 1;
}

static int
EMark_callback (const BoxType * b, void *cl)
{
  ElementTypePtr element = (ElementTypePtr) b;

  DrawEMark (element, element->MarkX, element->MarkY, !FRONT (element));
  return 1;
}

static void
SetPVColor_inlayer (PinTypePtr Pin, LayerTypePtr Layer, int Type)
{
  char *color;

  if (TEST_FLAG (WARNFLAG, Pin))
    color = PCB->WarnColor;
  else if (TEST_FLAG (SELECTEDFLAG, Pin))
    color = (Type == VIA_TYPE) ? PCB->ViaSelectedColor : PCB->PinSelectedColor;
  else if (TEST_FLAG (FOUNDFLAG, Pin))
    color = PCB->ConnectedColor;
  else
    {
      int component_group = GetLayerGroupNumberByNumber (component_silk_layer);
      int solder_group    = GetLayerGroupNumberByNumber (solder_silk_layer);
      int this_group      = GetLayerGroupNumberByPointer (Layer);

      if (this_group == component_group || this_group == solder_group)
        color = (SWAP_IDENT == (this_group == solder_group))
                  ? PCB->ViaColor : PCB->InvisibleObjectsColor;
      else
        color = Layer->Color;
    }

  gui->set_color (Output.fgGC, color);
}


static int
pin_name_callback (const BoxType * b, void *cl)
{
  PinTypePtr pin = (PinTypePtr) b;

  if (!TEST_FLAG (HOLEFLAG, pin) && TEST_FLAG (DISPLAYNAMEFLAG, pin))
    DrawPinName (pin, 0);
  return 1;
}

static int
pin_inlayer_callback (const BoxType * b, void *cl)
{
  SetPVColor_inlayer ((PinTypePtr) b, cl, PIN_TYPE);
  DrawPinOrViaLowLevel ((PinTypePtr) b, false);
  return 1;
}

static int
via_inlayer_callback (const BoxType * b, void *cl)
{
  SetPVColor_inlayer ((PinTypePtr) b, cl, VIA_TYPE);
  DrawPinOrViaLowLevel ((PinTypePtr) b, false);
  return 1;
}

static int
pin_callback (const BoxType * b, void *cl)
{
  PinTypePtr pin = (PinTypePtr) b;

  if (!TEST_FLAG (HOLEFLAG, pin) && TEST_FLAG (DISPLAYNAMEFLAG, pin))
    DrawPinName (pin, 0);
  DrawPlainPin (pin, false);
  return 1;
}

static int
pad_callback (const BoxType * b, void *cl)
{
  PadTypePtr pad = (PadTypePtr) b;
  if (FRONT (pad))
  {
    if (TEST_FLAG (DISPLAYNAMEFLAG, pad))
      DrawPadName (pad, 0);
    DrawPad (pad, 0);
  }
  return 1;
}


static int
hole_callback (const BoxType * b, void *cl)
{
#if 0
  PinTypePtr pin = (PinTypePtr) b;
  int plated = cl ? *(int *) cl : -1;

  switch (plated)
    {
    case -1:
      break;
    case 0:
      if (!TEST_FLAG (HOLEFLAG, pin))
	return 1;
      break;
    case 1:
      if (TEST_FLAG (HOLEFLAG, pin))
	return 1;
      break;
    }
#endif
  DrawHole ((PinTypePtr) b);
  return 1;
}

static int
via_callback (const BoxType * b, void *cl)
{
  PinTypePtr via = (PinTypePtr) b;
  DrawPlainVia (via, false);
  return 1;
}

static int
line_callback (const BoxType * b, void *cl)
{
  DrawLine ((LayerTypePtr) cl, (LineTypePtr) b, 0);
  return 1;
}

static int
arc_callback (const BoxType * b, void *cl)
{
  DrawArc ((LayerTypePtr) cl, (ArcTypePtr) b, 0);
  return 1;
}

static int
text_callback (const BoxType * b, void *cl)
{
  DrawRegularText ((LayerTypePtr) cl, (TextTypePtr) b, 0);
  return 1;
}

static void
DrawPlainPolygon (LayerTypePtr Layer, PolygonTypePtr Polygon, const BoxType *drawn_area)
{
  static char *color;

  if (!Polygon->Clipped)
    return;

  /* Re-use HOLEFLAG to cut out islands */
  if (TEST_FLAG (HOLEFLAG, Polygon))
    return;

  if (TEST_FLAG (SELECTEDFLAG, Polygon))
    color = Layer->SelectedColor;
  else if (TEST_FLAG (FOUNDFLAG, Polygon))
    color = PCB->ConnectedColor;
  else
    color = Layer->Color;
  gui->set_color (Output.fgGC, color);

  if (gui->thindraw_pcb_polygon != NULL &&
      (TEST_FLAG (THINDRAWFLAG, PCB) ||
       TEST_FLAG (THINDRAWPOLYFLAG, PCB)))
    gui->thindraw_pcb_polygon (Output.fgGC, Polygon, drawn_area);
  else
    gui->fill_pcb_polygon (Output.fgGC, Polygon, drawn_area);

  /* If checking planes, thin-draw any pieces which have been clipped away */
  if (gui->thindraw_pcb_polygon != NULL &&
      TEST_FLAG (CHECKPLANESFLAG, PCB) &&
      !TEST_FLAG (FULLPOLYFLAG, Polygon))
    {
      PolygonType poly = *Polygon;

      for (poly.Clipped = Polygon->Clipped->f;
           poly.Clipped != Polygon->Clipped;
           poly.Clipped = poly.Clipped->f)
        gui->thindraw_pcb_polygon (Output.fgGC, &poly, drawn_area);
    }
}

static int
poly_callback (const BoxType * b, void *cl)
{
  struct pin_info *i = (struct pin_info *) cl;

  DrawPlainPolygon (i->Layer, (PolygonTypePtr) b, i->drawn_area);
  return 1;
}

static int
pour_callback (const BoxType * b, void *cl)
{
  struct pin_info *i = (struct pin_info *) cl;
  PourType *pour = (PourType *)b;

  DrawPour (i->Layer, pour, 0);

  if (pour->PolygonN)
    {
      r_search (pour->polygon_tree, i->drawn_area, NULL, poly_callback, i);
    }

  return 1;
}

static void
DrawPadLowLevelSolid (hidGC gc, PadTypePtr Pad, bool clear, bool mask)
{
  int w = clear ? (mask ? Pad->Mask : Pad->Thickness + Pad->Clearance)
		: Pad->Thickness;

  if (Pad->Point1.X == Pad->Point2.X &&
      Pad->Point1.Y == Pad->Point2.Y)
    {
      if (TEST_FLAG (SQUAREFLAG, Pad))
        {
          int l, r, t, b;
          l = Pad->Point1.X - w / 2;
          b = Pad->Point1.Y - w / 2;
          r = l + w;
          t = b + w;
          gui->fill_rect (gc, l, b, r, t);
        }
      else
        {
          gui->fill_circle (gc, Pad->Point1.X, Pad->Point1.Y, w / 2);
        }
    }
  else
    {
      gui->set_line_cap (gc,
                         TEST_FLAG (SQUAREFLAG,
                                    Pad) ? Square_Cap : Round_Cap);
      gui->set_line_width (gc, w);

      gui->draw_line (gc,
                      Pad->Point1.X, Pad->Point1.Y,
                      Pad->Point2.X, Pad->Point2.Y);
    }
}

static void
ClearPadSolid (PadTypePtr Pad, bool mask)
{
  DrawPadLowLevelSolid(Output.pmGC, Pad, true, mask);
}

static void
ClearOnlyPinSolid (PinTypePtr Pin, bool mask)
{
  BDimension half =
    (mask ? Pin->Mask / 2 : (Pin->Thickness + Pin->Clearance) / 2);

  if (!mask && TEST_FLAG (HOLEFLAG, Pin))
    return;
  if (half == 0)
    return;
  if (!mask && Pin->Clearance <= 0)
    return;

  /* Clear the area around the pin */
  if (TEST_FLAG (SQUAREFLAG, Pin))
    {
      int l, r, t, b;
      l = Pin->X - half;
      b = Pin->Y - half;
      r = l + half * 2;
      t = b + half * 2;
      gui->fill_rect (Output.pmGC, l, b, r, t);
    }
  else if (TEST_FLAG (OCTAGONFLAG, Pin))
    {
      DrawSpecialPolygon (Output.pmGC, Pin->X, Pin->Y, half * 2, false);
    }
  else
    {
      gui->fill_circle (Output.pmGC, Pin->X, Pin->Y, half);
    }
}

static int
clearPin_callback_solid (const BoxType * b, void *cl)
{
  PinTypePtr pin = (PinTypePtr) b;
  struct pin_info *i = (struct pin_info *) cl;
  if (i->arg)
    ClearOnlyPinSolid (pin, true);
  return 1;
}

static int
clearPad_callback_solid (const BoxType * b, void *cl)
{
  PadTypePtr pad = (PadTypePtr) b;
  if (!XOR (TEST_FLAG (ONSOLDERFLAG, pad), SWAP_IDENT))
    ClearPadSolid (pad, true);
  return 1;
}

int clearPin_callback (const BoxType * b, void *cl);
int clearPad_callback (const BoxType * b, void *cl);


static void
DrawMask (BoxType * screen)
{
  struct pin_info info;
  int thin = TEST_FLAG(THINDRAWFLAG, PCB) || TEST_FLAG(THINDRAWPOLYFLAG, PCB);
  PolygonType polygon;

  OutputType *out = &Output;

  info.arg = true;
  info.drawn_area = screen;

  if (thin)
    {
      gui->set_line_width (Output.pmGC, 0);
      gui->set_color (Output.pmGC, PCB->MaskColor);
      r_search (PCB->Data->pin_tree, screen, NULL, clearPin_callback, &info);
      r_search (PCB->Data->via_tree, screen, NULL, clearPin_callback, &info);
      r_search (PCB->Data->pad_tree, screen, NULL, clearPad_callback, &info);
      gui->set_color (Output.pmGC, "erase");
    }

  gui->use_mask (HID_MASK_BEFORE);
  gui->set_color (out->fgGC, PCB->MaskColor);
  gui->fill_rect (out->fgGC, 0, 0, PCB->MaxWidth, PCB->MaxHeight);

  gui->use_mask (HID_MASK_CLEAR);
  r_search (PCB->Data->pin_tree, screen, NULL, clearPin_callback_solid, &info);
  r_search (PCB->Data->via_tree, screen, NULL, clearPin_callback_solid, &info);
  r_search (PCB->Data->pad_tree, screen, NULL, clearPad_callback_solid, &info);

  gui->use_mask (HID_MASK_AFTER);
  gui->set_color (out->fgGC, PCB->MaskColor);
  ghid_global_alpha_mult (out->fgGC, thin ? 0.35 : 1.0);

  polygon.Clipped = board_outline_poly ();
  polygon.NoHoles = NULL;
  polygon.NoHolesValid = 0;
  polygon.BoundingBox = *screen;
  SET_FLAG (FULLPOLYFLAG, &polygon);
  common_fill_pcb_polygon (out->fgGC, &polygon, screen);
  poly_Free (&polygon.Clipped);
  poly_FreeContours (&polygon.NoHoles);
  /* THE GL fill_pcb_polygon doesn't work whilst masking */
//  gui->fill_pcb_polygon (out->fgGC, &polygon, screen);
//  gui->fill_rect (out->fgGC, 0, 0, PCB->MaxWidth, PCB->MaxHeight);
  ghid_global_alpha_mult (out->fgGC, 1.0);

  gui->use_mask (HID_MASK_OFF);
}

static int
DrawLayerGroup (int group, const BoxType * screen)
{
  int i, rv = 1;
  int layernum;
  struct pin_info info;
  LayerTypePtr Layer;
  int n_entries = PCB->LayerGroups.Number[group];
  Cardinal *layers = PCB->LayerGroups.Entries[group];
  int first_run = 1;
  int component_group = GetLayerGroupNumberByNumber (component_silk_layer);
  int solder_group    = GetLayerGroupNumberByNumber (solder_silk_layer);

  if (!gui->set_layer (0, group, 0)) {
    gui->set_layer (NULL, SL (FINISHED, 0), 0);
    return 0;
  }

  /* HACK: Subcomposite each layer in a layer group separately */
  for (i = n_entries - 1; i >= 0; i--) {
    layernum = layers[i];
    Layer = PCB->Data->Layer + layers[i];

    if (strcmp (Layer->Name, "outline") == 0 ||
        strcmp (Layer->Name, "route") == 0)
      rv = 0;

    if (layernum < max_copper_layer && Layer->On) {

      if (!first_run)
        gui->set_layer (0, group, 0);

      first_run = 0;

      if (rv) {
        /* Mask out drilled holes on this layer */
        hidgl_flush_triangles (&buffer);
        glPushAttrib (GL_COLOR_BUFFER_BIT);
        glColorMask (0, 0, 0, 0);
        gui->set_color (Output.bgGC, PCB->MaskColor);
        if (PCB->PinOn) r_search (PCB->Data->pin_tree, screen, NULL, hole_callback, NULL);
        if (PCB->ViaOn) r_search (PCB->Data->via_tree, screen, NULL, hole_callback, NULL);
        hidgl_flush_triangles (&buffer);
        glPopAttrib ();
      }

      /* draw all polygons on this layer */
      if (Layer->PourN) {
        info.Layer = Layer;
        info.drawn_area = screen;
        r_search (Layer->pour_tree, screen, NULL, pour_callback, &info);

        /* HACK: Subcomposite polygons separately from other layer primitives */
        /* Reset the compositing */
        gui->set_layer (NULL, SL (FINISHED, 0), 0);
        gui->set_layer (0, group, 0);

        if (rv) {
          hidgl_flush_triangles (&buffer);
          glPushAttrib (GL_COLOR_BUFFER_BIT);
          glColorMask (0, 0, 0, 0);
          /* Mask out drilled holes on this layer */
          if (PCB->PinOn) r_search (PCB->Data->pin_tree, screen, NULL, hole_callback, NULL);
          if (PCB->ViaOn) r_search (PCB->Data->via_tree, screen, NULL, hole_callback, NULL);
          hidgl_flush_triangles (&buffer);
          glPopAttrib ();
        }
      }

      /* Draw pins, vias and pads on this layer */
      if (!global_view_2d && rv) {
        if (PCB->PinOn &&
            (group == solder_group || group == component_group))
          r_search (PCB->Data->pin_tree, screen, NULL, pin_name_callback, Layer);
        if (PCB->PinOn) r_search (PCB->Data->pin_tree, screen, NULL, pin_inlayer_callback, Layer);
        if (PCB->ViaOn) r_search (PCB->Data->via_tree, screen, NULL, via_inlayer_callback, Layer);
        if ((group == component_group && !SWAP_IDENT) ||
            (group == solder_group    &&  SWAP_IDENT))
          if (PCB->PinOn)
            r_search (PCB->Data->pad_tree, screen, NULL, pad_callback, Layer);
        if ((group == solder_group    && !SWAP_IDENT) ||
            (group == component_group &&  SWAP_IDENT))
          if (PCB->PinOn)
            r_search (PCB->Data->pad_tree, screen, NULL, backPad_callback, Layer);
      }

      if (TEST_FLAG (CHECKPLANESFLAG, PCB))
        continue;

      r_search (Layer->line_tree, screen, NULL, line_callback, Layer);
      r_search (Layer->arc_tree, screen, NULL, arc_callback, Layer);
      r_search (Layer->text_tree, screen, NULL, text_callback, Layer);
    }
  }

  gui->set_layer (NULL, SL (FINISHED, 0), 0);

  return (n_entries > 1);
}

static void
DrawDrillChannel (int vx, int vy, int vr, int from_layer, int to_layer, double scale)
{
#define PIXELS_PER_CIRCLINE 5.
#define MIN_FACES_PER_CYL 6
#define MAX_FACES_PER_CYL 360
  float radius = vr;
  float x1, y1;
  float x2, y2;
  float z1, z2;
  int i;
  int slices;

  slices = M_PI * 2 * vr / scale / PIXELS_PER_CIRCLINE;

  if (slices < MIN_FACES_PER_CYL)
    slices = MIN_FACES_PER_CYL;

  if (slices > MAX_FACES_PER_CYL)
    slices = MAX_FACES_PER_CYL;

  z1 = compute_depth (from_layer);
  z2 = compute_depth (to_layer);

  x1 = vx + vr;
  y1 = vy;

  hidgl_ensure_triangle_space (&buffer, 2 * slices);
  for (i = 0; i < slices; i++)
    {
      x2 = radius * cosf (((float)(i + 1)) * 2. * M_PI / (float)slices) + vx;
      y2 = radius * sinf (((float)(i + 1)) * 2. * M_PI / (float)slices) + vy;
      hidgl_add_triangle_3D (&buffer, x1, y1, z1,  x2, y2, z1,  x1, y1, z2);
      hidgl_add_triangle_3D (&buffer, x2, y2, z1,  x1, y1, z2,  x2, y2, z2);
      x1 = x2;
      y1 = y2;
    }
}

struct cyl_info {
  int from_layer;
  int to_layer;
  double scale;
};

static int
draw_hole_cyl (PinType *Pin, struct cyl_info *info, int Type)
{
  char *color;

  if (TEST_FLAG (WARNFLAG, Pin))
    color = PCB->WarnColor;
  else if (TEST_FLAG (SELECTEDFLAG, Pin))
    color = (Type == VIA_TYPE) ? PCB->ViaSelectedColor : PCB->PinSelectedColor;
  else if (TEST_FLAG (FOUNDFLAG, Pin))
    color = PCB->ConnectedColor;
  else
    color = "drill";

  gui->set_color (Output.fgGC, color);
  DrawDrillChannel (Pin->X, Pin->Y, Pin->DrillingHole / 2, info->from_layer, info->to_layer, info->scale);
  return 0;
}

static int
pin_hole_cyl_callback (const BoxType * b, void *cl)
{
  return draw_hole_cyl ((PinType *)b, (struct cyl_info *)cl, PIN_TYPE);
}

static int
via_hole_cyl_callback (const BoxType * b, void *cl)
{
  return draw_hole_cyl ((PinType *)b, (struct cyl_info *)cl, VIA_TYPE);
}

void
ghid_draw_everything (BoxTypePtr drawn_area)
{
  int i, ngroups;
  /* This is the list of layer groups we will draw.  */
  int do_group[MAX_LAYER];
  /* This is the reverse of the order in which we draw them.  */
  int drawn_groups[MAX_LAYER];
  struct cyl_info cyl_info;
  int reverse_layers;
  int save_show_solder;
  int solder_group;
  int component_group;
  int min_phys_group;
  int max_phys_group;

  extern bool Gathering;

  current_color = NULL;
  Gathering = false;

  /* Test direction of rendering */
  /* Look at sign of eye coordinate system z-coord when projecting a
     world vector along +ve Z axis, (0, 0, 1). */
  /* FIXME: This isn't strictly correct, as I've ignored the matrix
            elements for homogeneous coordinates. */
  /* NB: last_modelview_matrix is transposed in memory! */
  reverse_layers = (last_modelview_matrix[2][2] < 0);

  save_show_solder = Settings.ShowSolderSide;

  if (reverse_layers)
    Settings.ShowSolderSide = !Settings.ShowSolderSide;

  if (!global_view_2d && save_show_solder)
    reverse_layers = !reverse_layers;
  PCB->Data->SILKLAYER.Color = PCB->ElementColor;
  PCB->Data->BACKSILKLAYER.Color = PCB->InvisibleObjectsColor;

  solder_group = GetLayerGroupNumberByNumber (solder_silk_layer);
  component_group = GetLayerGroupNumberByNumber (component_silk_layer);

  min_phys_group = MIN (solder_group, component_group);
  max_phys_group = MAX (solder_group, component_group);

  memset (do_group, 0, sizeof (do_group));
  if (global_view_2d) {
    // Draw in layer stack order when in 2D view
    for (ngroups = 0, i = 0; i < max_copper_layer; i++) {
      int group = GetLayerGroupNumberByNumber (LayerStack[i]);

      if (!do_group[group]) {
        do_group[group] = 1;
        drawn_groups[ngroups++] = group;
      }
    }
  } else {
    // Draw in group order when in 3D view
    for (ngroups = 0, i = 0; i < max_group; i++) {
      int group = reverse_layers ? max_group - 1 - i : i;

      if (!do_group[group]) {
        do_group[group] = 1;
        drawn_groups[ngroups++] = group;
      }
    }
  }

  /*
   * first draw all 'invisible' stuff
   */
  if (!TEST_FLAG (CHECKPLANESFLAG, PCB) &&
      gui->set_layer ("invisible", SL (INVISIBLE, 0), 0)) {
    if (PCB->ElementOn) {
      r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], drawn_area, NULL, backN_callback, NULL);
      DrawLayer (&(PCB->Data->BACKSILKLAYER), drawn_area);
    }
#if 1
    if (!global_view_2d) {
      /* Draw the solder mask if turned on */
      if (gui->set_layer ("soldermask", SL (MASK, BOTTOM), 0)) {
        int save_swap = SWAP_IDENT;
        gui->set_layer (NULL, SL (FINISHED, 0), 0);
        gui->set_layer ("componentmask", SL (MASK, TOP), 0);
        //^__ HACK, THE GUI DOESNT WANT US TO DRAW THIS!
        SWAP_IDENT = 0;
        DrawMask (drawn_area);
        SWAP_IDENT = save_swap;
        gui->set_layer (NULL, SL (FINISHED, 0), 0);
      }
      if (gui->set_layer ("componentmask", SL (MASK, TOP), 0)) {
        int save_swap = SWAP_IDENT;
        gui->set_layer (NULL, SL (FINISHED, 0), 0);
        gui->set_layer ("soldermask", SL (MASK, BOTTOM), 0);
        //^__ HACK, THE GUI DOESNT WANT US TO DRAW THIS!
        SWAP_IDENT = 1;
        DrawMask (drawn_area);
        SWAP_IDENT = save_swap;
        gui->set_layer (NULL, SL (FINISHED, 0), 0);
      }
      gui->set_layer ("invisible", SL (INVISIBLE, 0), 0);
    }
#endif
    if (global_view_2d)
      r_search (PCB->Data->pad_tree, drawn_area, NULL, backPad_callback, NULL);
    if (PCB->ElementOn)
      r_search (PCB->Data->element_tree, drawn_area, NULL, backE_callback, NULL);
    gui->set_layer (NULL, SL (FINISHED, 0), 0);
  }

  /* draw all layers in layerstack order */
  for (i = ngroups - 1; i >= 0; i--) {
    DrawLayerGroup (drawn_groups [i], drawn_area);

#if 1
    if (!global_view_2d && i > 0 &&
        drawn_groups[i] >= min_phys_group &&
        drawn_groups[i] <= max_phys_group &&
        drawn_groups[i - 1] >= min_phys_group &&
        drawn_groups[i - 1] <= max_phys_group) {
      cyl_info.from_layer = drawn_groups[i];
      cyl_info.to_layer = drawn_groups[i - 1];
      cyl_info.scale = gport->zoom;
//      gui->set_color (Output.fgGC, PCB->MaskColor);
      gui->set_color (Output.fgGC, "drill");
      ghid_global_alpha_mult (Output.fgGC, 0.75);
      if (PCB->PinOn) r_search (PCB->Data->pin_tree, drawn_area, NULL, pin_hole_cyl_callback, &cyl_info);
      if (PCB->ViaOn) r_search (PCB->Data->via_tree, drawn_area, NULL, via_hole_cyl_callback, &cyl_info);
      ghid_global_alpha_mult (Output.fgGC, 1.0);
    }
#endif
  }

  if (TEST_FLAG (CHECKPLANESFLAG, PCB))
    return;

  /* Draw pins, pads, vias below silk */
  if (!Settings.ShowSolderSide)
    gui->set_layer ("topsilk", SL (SILK, TOP), 0);
  else
    gui->set_layer ("bottomsilk", SL (SILK, BOTTOM), 0);
//  gui->set_layer (NULL, SL (FINISHED, 0), 0);

  if (global_view_2d)
    {
      /* Mask out drilled holes */
      hidgl_flush_triangles (&buffer);
      glPushAttrib (GL_COLOR_BUFFER_BIT);
      glColorMask (0, 0, 0, 0);
      if (PCB->PinOn) r_search (PCB->Data->pin_tree, drawn_area, NULL, hole_callback, NULL);
      if (PCB->ViaOn) r_search (PCB->Data->via_tree, drawn_area, NULL, hole_callback, NULL);
      hidgl_flush_triangles (&buffer);
      glPopAttrib ();

      if (PCB->PinOn) r_search (PCB->Data->pad_tree, drawn_area, NULL, pad_callback, NULL);
      if (PCB->PinOn) r_search (PCB->Data->pin_tree, drawn_area, NULL, pin_callback, NULL);
      if (PCB->ViaOn) r_search (PCB->Data->via_tree, drawn_area, NULL, via_callback, NULL);
    }

  gui->set_layer (NULL, SL (FINISHED, 0), 0);

  /* Draw the solder mask if turned on */
  if (gui->set_layer ("componentmask", SL (MASK, TOP), 0)) {
    int save_swap = SWAP_IDENT;
    SWAP_IDENT = 0;
    DrawMask (drawn_area);
    SWAP_IDENT = save_swap;
    gui->set_layer (NULL, SL (FINISHED, 0), 0);
  }
  if (gui->set_layer ("soldermask", SL (MASK, BOTTOM), 0)) {
    int save_swap = SWAP_IDENT;
    SWAP_IDENT = 1;
    DrawMask (drawn_area);
    SWAP_IDENT = save_swap;
    gui->set_layer (NULL, SL (FINISHED, 0), 0);
  }
  /* Draw top silkscreen */
  if (!Settings.ShowSolderSide &&
      gui->set_layer ("topsilk", SL (SILK, TOP), 0)) {
    DrawSilk (0, component_silk_layer, drawn_area);
    gui->set_layer (NULL, SL (FINISHED, 0), 0);
  }

  if (Settings.ShowSolderSide &&
      gui->set_layer ("bottomsilk", SL (SILK, BOTTOM), 0)) {
    DrawSilk (1, solder_silk_layer, drawn_area);
    gui->set_layer (NULL, SL (FINISHED, 0), 0);
  }

  /* Draw element Marks */
  if (PCB->PinOn)
    r_search (PCB->Data->element_tree, drawn_area, NULL, EMark_callback, NULL);

  /* Draw rat lines on top */
  if (PCB->RatOn && gui->set_layer ("rats", SL (RATS, 0), 0))
    DrawRats(drawn_area);

  Gathering = true;

  Settings.ShowSolderSide = save_show_solder;
}

#define Z_NEAR 3.0
gboolean
ghid_drawing_area_expose_cb (GtkWidget *widget,
                             GdkEventExpose *ev,
                             GHidPort *port)
{
#if 0
  static int one_shot = 1;
  static int display_list;
#endif
  BoxType region;
  int eleft, eright, etop, ebottom;
  int min_x, min_y;
  int max_x, max_y;
  int new_x, new_y;
  int min_depth;
  int max_depth;

  ghid_start_drawing (port);

  hidgl_init ();
  check_gl_drawing_ok_hack = true;

  /* If we don't have any stencil bits available,
     we can't use the hidgl polygon drawing routine */
  /* TODO: We could use the GLU tessellator though */
  if (hidgl_stencil_bits() == 0)
    {
      ghid_hid.fill_pcb_polygon = common_fill_pcb_polygon;
      ghid_hid.poly_dicer = 1;
    }

  ghid_show_crosshair (FALSE);

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

  glTranslatef (widget->allocation.width / 2., widget->allocation.height / 2., 0);
  glMultMatrixf ((GLfloat *)view_matrix);
  glTranslatef (-widget->allocation.width / 2., -widget->allocation.height / 2., 0);
  glGetFloatv (GL_MODELVIEW_MATRIX, (GLfloat *)last_modelview_matrix);

#if 0
  if (one_shot) {

    display_list = glGenLists(1);
    glNewList (display_list, GL_COMPILE);
#endif

  glEnable (GL_STENCIL_TEST);
  glClearColor (port->offlimits_color.red / 65535.,
                port->offlimits_color.green / 65535.,
                port->offlimits_color.blue / 65535.,
                1.);

  glStencilMask (~0);
  glClearStencil (0);
  glClear (GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  hidgl_reset_stencil_usage ();

  /* Disable the stencil test until we need it - otherwise it gets dirty */
  glDisable (GL_STENCIL_TEST);
  glStencilFunc (GL_ALWAYS, 0, 0);

  /* Test the 8 corners of a cube spanning the event */
  min_depth = -50 + compute_depth (0);                /* FIXME */
  max_depth =  50 + compute_depth (max_copper_layer); /* FIXME */

  ghid_unproject_to_z_plane (ev->area.x,
                             ev->area.y,
                             min_depth, &new_x, &new_y);
  max_x = min_x = new_x;
  max_y = min_y = new_y;

  ghid_unproject_to_z_plane (ev->area.x,
                             ev->area.y,
                             max_depth, &new_x, &new_y);
  min_x = MIN (min_x, new_x);  max_x = MAX (max_x, new_x);
  min_y = MIN (min_y, new_y);  max_y = MAX (max_y, new_y);

  /* */
  ghid_unproject_to_z_plane (ev->area.x + ev->area.width,
                             ev->area.y,
                             min_depth, &new_x, &new_y);
  min_x = MIN (min_x, new_x);  max_x = MAX (max_x, new_x);
  min_y = MIN (min_y, new_y);  max_y = MAX (max_y, new_y);

  ghid_unproject_to_z_plane (ev->area.x + ev->area.width, ev->area.y,
                             max_depth, &new_x, &new_y);
  min_x = MIN (min_x, new_x);  max_x = MAX (max_x, new_x);
  min_y = MIN (min_y, new_y);  max_y = MAX (max_y, new_y);

  /* */
  ghid_unproject_to_z_plane (ev->area.x + ev->area.width,
                             ev->area.y + ev->area.height,
                             min_depth, &new_x, &new_y);
  min_x = MIN (min_x, new_x);  max_x = MAX (max_x, new_x);
  min_y = MIN (min_y, new_y);  max_y = MAX (max_y, new_y);

  ghid_unproject_to_z_plane (ev->area.x + ev->area.width,
                             ev->area.y + ev->area.height,
                             max_depth, &new_x, &new_y);
  min_x = MIN (min_x, new_x);  max_x = MAX (max_x, new_x);
  min_y = MIN (min_y, new_y);  max_y = MAX (max_y, new_y);

  /* */
  ghid_unproject_to_z_plane (ev->area.x,
                             ev->area.y + ev->area.height,
                             min_depth,
                             &new_x, &new_y);
  min_x = MIN (min_x, new_x);  max_x = MAX (max_x, new_x);
  min_y = MIN (min_y, new_y);  max_y = MAX (max_y, new_y);

  ghid_unproject_to_z_plane (ev->area.x,
                             ev->area.y + ev->area.height,
                             max_depth,
                             &new_x, &new_y);
  min_x = MIN (min_x, new_x);  max_x = MAX (max_x, new_x);
  min_y = MIN (min_y, new_y);  max_y = MAX (max_y, new_y);

  region.X1 = MIN (Px (min_x), Px (max_x + 1));
  region.X2 = MAX (Px (min_x), Px (max_x + 1));
  region.Y1 = MIN (Py (min_y), Py (max_y + 1));
  region.Y2 = MAX (Py (min_y), Py (max_y + 1));

  eleft = Vx (0);  eright  = Vx (PCB->MaxWidth);
  etop  = Vy (0);  ebottom = Vy (PCB->MaxHeight);

  glColor3f (port->bg_color.red / 65535.,
             port->bg_color.green / 65535.,
             port->bg_color.blue / 65535.);

  /* TODO: Background image */

  hidgl_init_triangle_array (&buffer);
  ghid_invalidate_current_gc ();

  /* Setup stenciling */
  /* Drawing operations set the stencil buffer to '1' */
  glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE); // Stencil pass => replace stencil value (with 1)
  /* Drawing operations as masked to areas where the stencil buffer is '0' */
//  glStencilFunc (GL_GREATER, 1, 1);             // Draw only where stencil buffer is 0

  glPushMatrix ();
  glScalef ((ghid_flip_x ? -1. : 1.) / port->zoom,
            (ghid_flip_y ? -1. : 1.) / port->zoom,
            (ghid_flip_x == ghid_flip_y) ? 1. : -1.);
  glTranslatef (ghid_flip_x ? port->view_x0 - PCB->MaxWidth  :
                             -port->view_x0,
                ghid_flip_y ? port->view_y0 - PCB->MaxHeight :
                             -port->view_y0, 0);

  if (global_view_2d) {
    glBegin (GL_QUADS);
    glVertex3i (0,             0,              0);
    glVertex3i (PCB->MaxWidth, 0,              0);
    glVertex3i (PCB->MaxWidth, PCB->MaxHeight, 0);
    glVertex3i (0,             PCB->MaxHeight, 0);
    glEnd ();
  } else {
    int solder_group;
    int component_group;
    int min_phys_group;
    int max_phys_group;
    int i;

    solder_group = GetLayerGroupNumberByNumber (solder_silk_layer);
    component_group = GetLayerGroupNumberByNumber (component_silk_layer);

    min_phys_group = MIN (solder_group, component_group);
    max_phys_group = MAX (solder_group, component_group);

    glBegin (GL_QUADS);
    for (i = min_phys_group; i <= max_phys_group; i++) {
      int depth = compute_depth (i);
      glVertex3i (0,             0,              depth);
      glVertex3i (PCB->MaxWidth, 0,              depth);
      glVertex3i (PCB->MaxWidth, PCB->MaxHeight, depth);
      glVertex3i (0,             PCB->MaxHeight, depth);
    }
    glEnd ();
  }

  // hid_expose_callback (&ghid_hid, &region, 0);
  ghid_draw_everything (&region);
  hidgl_flush_triangles (&buffer);
  glPopMatrix ();

  /* Just prod the drawing code so the current depth gets set to
     the right value for the layer we are editing */
  gui->set_layer (NULL, GetLayerGroupNumberByNumber (INDEXOFCURRENT), 0);
  gui->set_layer (NULL, SL_FINISHED, 0);

  ghid_draw_grid (&region);

  hidgl_init_triangle_array (&buffer);
  ghid_invalidate_current_gc ();
  glPushMatrix ();
  glScalef ((ghid_flip_x ? -1. : 1.) / port->zoom,
            (ghid_flip_y ? -1. : 1.) / port->zoom, 1);
  glTranslatef (ghid_flip_x ? port->view_x0 - PCB->MaxWidth  :
                             -port->view_x0,
                ghid_flip_y ? port->view_y0 - PCB->MaxHeight :
                             -port->view_y0, 0);
  DrawAttached (TRUE);
  DrawMark (TRUE);
  hidgl_flush_triangles (&buffer);
  glPopMatrix ();

#if 0
    glEndList ();
    one_shot = 0;
  } else {
    /* Second and subsequent times */
    glCallList (display_list);
  }
#endif

  ghid_show_crosshair (TRUE);

  hidgl_flush_triangles (&buffer);

  check_gl_drawing_ok_hack = false;
  ghid_end_drawing (port);

  return FALSE;
}

void
ghid_port_drawing_realize_cb (GtkWidget *widget, gpointer data)
{
  GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
    return;

  gdk_gl_drawable_gl_end (gldrawable);
  /*** OpenGL END ***/

  return;
}

gboolean
ghid_pinout_preview_expose (GtkWidget *widget,
                            GdkEventExpose *ev)
{
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

  check_gl_drawing_ok_hack = true;

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

  if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
    gdk_gl_drawable_swap_buffers (pGlDrawable);
  else
    glFlush ();

  check_gl_drawing_ok_hack = false;

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


GdkPixmap *
ghid_render_pixmap (int cx, int cy, double zoom, int width, int height, int depth)
{
  GdkGLConfig *glconfig;
  GdkPixmap *pixmap;
  GdkGLPixmap *glpixmap;
  GdkGLContext* glcontext;
  GdkGLDrawable* gldrawable;
  double save_zoom;
  int save_left, save_top;
  int save_width, save_height;
  int save_view_width, save_view_height;
  BoxType region;
  bool save_check_gl_drawing_ok_hack;

  save_zoom = gport->zoom;
  save_width = gport->width;
  save_height = gport->height;
  save_left = gport->view_x0;
  save_top = gport->view_y0;
  save_view_width = gport->view_width;
  save_view_height = gport->view_height;

  /* Setup rendering context for drawing routines
   */

  glconfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGB     |
                                        GDK_GL_MODE_STENCIL |
//                                        GDK_GL_MODE_DEPTH   |
                                        GDK_GL_MODE_SINGLE);

  pixmap = gdk_pixmap_new (NULL, width, height, depth);
  glpixmap = gdk_pixmap_set_gl_capability (pixmap, glconfig, NULL);
  gldrawable = GDK_GL_DRAWABLE (glpixmap);
  glcontext = gdk_gl_context_new (gldrawable, NULL, FALSE, GDK_GL_RGBA_TYPE);

  /* Setup zoom factor for drawing routines */

  gport->zoom = zoom;
  gport->width = width;
  gport->height = height;
  gport->view_width = width * gport->zoom;
  gport->view_height = height * gport->zoom;
  gport->view_x0 = ghid_flip_x ? PCB->MaxWidth - cx : cx;
  gport->view_x0 -= gport->view_height / 2;
  gport->view_y0 = ghid_flip_y ? PCB->MaxHeight - cy : cy;
  gport->view_y0 -= gport->view_width  / 2;

  /* make GL-context "current" */
  if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext)) {
    return NULL;
  }

  save_check_gl_drawing_ok_hack = check_gl_drawing_ok_hack;
  check_gl_drawing_ok_hack = true;

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glViewport (0, 0, width, height);

  glEnable (GL_SCISSOR_TEST);
  glScissor (0, 0, width, height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (0, width, height, 0, -100000, 100000);
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
  region.X1 = MIN(Px(0), Px(gport->width + 1));
  region.Y1 = MIN(Py(0), Py(gport->height + 1));
  region.X2 = MAX(Px(0), Px(gport->width + 1));
  region.Y2 = MAX(Py(0), Py(gport->height + 1));
  hid_expose_callback (&ghid_hid, &region, NULL);
  hidgl_flush_triangles (&buffer);
  glPopMatrix ();

  glFlush ();

  check_gl_drawing_ok_hack = save_check_gl_drawing_ok_hack;

  /* end drawing to current GL-context */
  gdk_gl_drawable_gl_end (gldrawable);
//  gdk_gl_context_destroy (glcontext);

  gdk_pixmap_unset_gl_capability (pixmap);

  g_object_unref (glconfig);
  g_object_unref (glcontext);

  gport->zoom = save_zoom;
  gport->width = save_width;
  gport->height = save_height;
  gport->view_x0 = save_left;
  gport->view_y0 = save_top;
  gport->view_width = save_view_width;
  gport->view_height = save_view_height;

  return pixmap;
}

static float
determinant_2x2 (float m[2][2])
{
  float det;
  det = m[0][0] * m[1][1] -
        m[0][1] * m[1][0];
  return det;
}

#if 0
static float
determinant_4x4 (float m[4][4])
{
  float det;
  det = m[0][3] * m[1][2] * m[2][1] * m[3][0]-m[0][2] * m[1][3] * m[2][1] * m[3][0] -
        m[0][3] * m[1][1] * m[2][2] * m[3][0]+m[0][1] * m[1][3] * m[2][2] * m[3][0] +
        m[0][2] * m[1][1] * m[2][3] * m[3][0]-m[0][1] * m[1][2] * m[2][3] * m[3][0] -
        m[0][3] * m[1][2] * m[2][0] * m[3][1]+m[0][2] * m[1][3] * m[2][0] * m[3][1] +
        m[0][3] * m[1][0] * m[2][2] * m[3][1]-m[0][0] * m[1][3] * m[2][2] * m[3][1] -
        m[0][2] * m[1][0] * m[2][3] * m[3][1]+m[0][0] * m[1][2] * m[2][3] * m[3][1] +
        m[0][3] * m[1][1] * m[2][0] * m[3][2]-m[0][1] * m[1][3] * m[2][0] * m[3][2] -
        m[0][3] * m[1][0] * m[2][1] * m[3][2]+m[0][0] * m[1][3] * m[2][1] * m[3][2] +
        m[0][1] * m[1][0] * m[2][3] * m[3][2]-m[0][0] * m[1][1] * m[2][3] * m[3][2] -
        m[0][2] * m[1][1] * m[2][0] * m[3][3]+m[0][1] * m[1][2] * m[2][0] * m[3][3] +
        m[0][2] * m[1][0] * m[2][1] * m[3][3]-m[0][0] * m[1][2] * m[2][1] * m[3][3] -
        m[0][1] * m[1][0] * m[2][2] * m[3][3]+m[0][0] * m[1][1] * m[2][2] * m[3][3];
   return det;
}
#endif

static void
invert_2x2 (float m[2][2], float out[2][2])
{
  float scale = 1 / determinant_2x2 (m);
  out[0][0] =  m[1][1] * scale;
  out[0][1] = -m[0][1] * scale;
  out[1][0] = -m[1][0] * scale;
  out[1][1] =  m[0][0] * scale;
}

#if 0
static void
invert_4x4 (float m[4][4], float out[4][4])
{
  float scale = 1 / determinant_4x4 (m);

  out[0][0] = (m[1][2] * m[2][3] * m[3][1] - m[1][3] * m[2][2] * m[3][1] +
               m[1][3] * m[2][1] * m[3][2] - m[1][1] * m[2][3] * m[3][2] -
               m[1][2] * m[2][1] * m[3][3] + m[1][1] * m[2][2] * m[3][3]) * scale;
  out[0][1] = (m[0][3] * m[2][2] * m[3][1] - m[0][2] * m[2][3] * m[3][1] -
               m[0][3] * m[2][1] * m[3][2] + m[0][1] * m[2][3] * m[3][2] +
               m[0][2] * m[2][1] * m[3][3] - m[0][1] * m[2][2] * m[3][3]) * scale;
  out[0][2] = (m[0][2] * m[1][3] * m[3][1] - m[0][3] * m[1][2] * m[3][1] +
               m[0][3] * m[1][1] * m[3][2] - m[0][1] * m[1][3] * m[3][2] -
               m[0][2] * m[1][1] * m[3][3] + m[0][1] * m[1][2] * m[3][3]) * scale;
  out[0][3] = (m[0][3] * m[1][2] * m[2][1] - m[0][2] * m[1][3] * m[2][1] -
               m[0][3] * m[1][1] * m[2][2] + m[0][1] * m[1][3] * m[2][2] +
               m[0][2] * m[1][1] * m[2][3] - m[0][1] * m[1][2] * m[2][3]) * scale;
  out[1][0] = (m[1][3] * m[2][2] * m[3][0] - m[1][2] * m[2][3] * m[3][0] -
               m[1][3] * m[2][0] * m[3][2] + m[1][0] * m[2][3] * m[3][2] +
               m[1][2] * m[2][0] * m[3][3] - m[1][0] * m[2][2] * m[3][3]) * scale;
  out[1][1] = (m[0][2] * m[2][3] * m[3][0] - m[0][3] * m[2][2] * m[3][0] +
               m[0][3] * m[2][0] * m[3][2] - m[0][0] * m[2][3] * m[3][2] -
               m[0][2] * m[2][0] * m[3][3] + m[0][0] * m[2][2] * m[3][3]) * scale;
  out[1][2] = (m[0][3] * m[1][2] * m[3][0] - m[0][2] * m[1][3] * m[3][0] -
               m[0][3] * m[1][0] * m[3][2] + m[0][0] * m[1][3] * m[3][2] +
               m[0][2] * m[1][0] * m[3][3] - m[0][0] * m[1][2] * m[3][3]) * scale;
  out[1][3] = (m[0][2] * m[1][3] * m[2][0] - m[0][3] * m[1][2] * m[2][0] +
               m[0][3] * m[1][0] * m[2][2] - m[0][0] * m[1][3] * m[2][2] -
               m[0][2] * m[1][0] * m[2][3] + m[0][0] * m[1][2] * m[2][3]) * scale;
  out[2][0] = (m[1][1] * m[2][3] * m[3][0] - m[1][3] * m[2][1] * m[3][0] +
               m[1][3] * m[2][0] * m[3][1] - m[1][0] * m[2][3] * m[3][1] -
               m[1][1] * m[2][0] * m[3][3] + m[1][0] * m[2][1] * m[3][3]) * scale;
  out[2][1] = (m[0][3] * m[2][1] * m[3][0] - m[0][1] * m[2][3] * m[3][0] -
               m[0][3] * m[2][0] * m[3][1] + m[0][0] * m[2][3] * m[3][1] +
               m[0][1] * m[2][0] * m[3][3] - m[0][0] * m[2][1] * m[3][3]) * scale;
  out[2][2] = (m[0][1] * m[1][3] * m[3][0] - m[0][3] * m[1][1] * m[3][0] +
               m[0][3] * m[1][0] * m[3][1] - m[0][0] * m[1][3] * m[3][1] -
               m[0][1] * m[1][0] * m[3][3] + m[0][0] * m[1][1] * m[3][3]) * scale;
  out[2][3] = (m[0][3] * m[1][1] * m[2][0] - m[0][1] * m[1][3] * m[2][0] -
               m[0][3] * m[1][0] * m[2][1] + m[0][0] * m[1][3] * m[2][1] +
               m[0][1] * m[1][0] * m[2][3] - m[0][0] * m[1][1] * m[2][3]) * scale;
  out[3][0] = (m[1][2] * m[2][1] * m[3][0] - m[1][1] * m[2][2] * m[3][0] -
               m[1][2] * m[2][0] * m[3][1] + m[1][0] * m[2][2] * m[3][1] +
               m[1][1] * m[2][0] * m[3][2] - m[1][0] * m[2][1] * m[3][2]) * scale;
  out[3][1] = (m[0][1] * m[2][2] * m[3][0] - m[0][2] * m[2][1] * m[3][0] +
               m[0][2] * m[2][0] * m[3][1] - m[0][0] * m[2][2] * m[3][1] -
               m[0][1] * m[2][0] * m[3][2] + m[0][0] * m[2][1] * m[3][2]) * scale;
  out[3][2] = (m[0][2] * m[1][1] * m[3][0] - m[0][1] * m[1][2] * m[3][0] -
               m[0][2] * m[1][0] * m[3][1] + m[0][0] * m[1][2] * m[3][1] +
               m[0][1] * m[1][0] * m[3][2] - m[0][0] * m[1][1] * m[3][2]) * scale;
  out[3][3] = (m[0][1] * m[1][2] * m[2][0] - m[0][2] * m[1][1] * m[2][0] +
               m[0][2] * m[1][0] * m[2][1] - m[0][0] * m[1][2] * m[2][1] -
               m[0][1] * m[1][0] * m[2][2] + m[0][0] * m[1][1] * m[2][2]) * scale;
}
#endif


void
ghid_unproject_to_z_plane (int ex, int ey, int vz, int *vx, int *vy)
{
  float mat[2][2];
  float inv_mat[2][2];
  float x, y;

  /*
    ex = view_matrix[0][0] * vx +
         view_matrix[0][1] * vy +
         view_matrix[0][2] * vz +
         view_matrix[0][3] * 1;
    ey = view_matrix[1][0] * vx +
         view_matrix[1][1] * vy +
         view_matrix[1][2] * vz +
         view_matrix[1][3] * 1;
    UNKNOWN ez = view_matrix[2][0] * vx +
                 view_matrix[2][1] * vy +
                 view_matrix[2][2] * vz +
                 view_matrix[2][3] * 1;

    ex - view_matrix[0][3] * 1
       - view_matrix[0][2] * vz
      = view_matrix[0][0] * vx +
        view_matrix[0][1] * vy;

    ey - view_matrix[1][3] * 1
       - view_matrix[1][2] * vz
      = view_matrix[1][0] * vx +
        view_matrix[1][1] * vy;
  */

  /* NB: last_modelview_matrix is transposed in memory! */
  x = (float)ex - last_modelview_matrix[3][0] * 1
                - last_modelview_matrix[2][0] * vz;

  y = (float)ey - last_modelview_matrix[3][1] * 1
                - last_modelview_matrix[2][1] * vz;

  /*
    x = view_matrix[0][0] * vx +
        view_matrix[0][1] * vy;

    y = view_matrix[1][0] * vx +
        view_matrix[1][1] * vy;

    [view_matrix[0][0] view_matrix[0][1]] [vx] = [x]
    [view_matrix[1][0] view_matrix[1][1]] [vy]   [y]
  */

  mat[0][0] = last_modelview_matrix[0][0];
  mat[0][1] = last_modelview_matrix[1][0];
  mat[1][0] = last_modelview_matrix[0][1];
  mat[1][1] = last_modelview_matrix[1][1];

//    if (determinant_2x2 (mat) < 0.00001)
//      printf ("Determinant is quite small\n");

  invert_2x2 (mat, inv_mat);

  *vx = (int)(inv_mat[0][0] * x + inv_mat[0][1] * y);
  *vy = (int)(inv_mat[1][0] * x + inv_mat[1][1] * y);
}


void
ghid_view_2d (void *ball, gboolean view_2d, gpointer userdata)
{
  global_view_2d = view_2d;
  ghid_invalidate_all ();
}

void
ghid_port_rotate (void *ball, float *quarternion, gpointer userdata)
{
#ifdef DEBUG_ROTATE
  int row, column;
#endif

  build_rotmatrix (view_matrix, quarternion);

#ifdef DEBUG_ROTATE
  for (row = 0; row < 4; row++) {
    printf ("[ %f", view_matrix[row][0]);
    for (column = 1; column < 4; column++) {
      printf (",\t%f", view_matrix[row][column]);
    }
    printf ("\t]\n");
  }
  printf ("\n");
#endif

  ghid_invalidate_all ();
}
