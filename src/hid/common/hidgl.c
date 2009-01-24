/* $Id: */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <math.h>
#include <time.h>


#include "action.h"
#include "crosshair.h"
#include "data.h"
#include "draw.h"
#include "error.h"
#include "global.h"
#include "mymem.h"
#include "draw.h"
#include "clip.h"

#include "hid.h"
#include "hidgl.h"


#include <GL/gl.h>
#include <GL/glut.h>

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

#define PIXELS_PER_CIRCLINE 5.

RCSID ("$Id: $");


#define TRIANGLE_ARRAY_SIZE 5000
static GLfloat triangle_array [2 * 3 * TRIANGLE_ARRAY_SIZE];
static unsigned int triangle_count;
static int coord_comp_count;

void
hidgl_init_triangle_array (void)
{
  glEnableClientState (GL_VERTEX_ARRAY);
  glVertexPointer (2, GL_FLOAT, 0, &triangle_array);
  triangle_count = 0;
  coord_comp_count = 0;
}

void
hidgl_flush_triangles ()
{
  if (triangle_count == 0)
    return;

  glDrawArrays (GL_TRIANGLES, 0, triangle_count * 3);
  triangle_count = 0;
  coord_comp_count = 0;
}

static void
ensure_triangle_space (int count)
{
  if (count > TRIANGLE_ARRAY_SIZE)
    {
      fprintf (stderr, "Not enough space in vertex buffer\n");
      fprintf (stderr, "Requested %i triangles, %i available\n", count, TRIANGLE_ARRAY_SIZE);
      exit (1);
    }
  if (count > TRIANGLE_ARRAY_SIZE - triangle_count)
    hidgl_flush_triangles ();
}

static inline void
add_triangle (GLfloat x1, GLfloat y1,
              GLfloat x2, GLfloat y2,
              GLfloat x3, GLfloat y3)
{
  triangle_array [coord_comp_count++] = x1;
  triangle_array [coord_comp_count++] = y1;
  triangle_array [coord_comp_count++] = x2;
  triangle_array [coord_comp_count++] = y2;
  triangle_array [coord_comp_count++] = x3;
  triangle_array [coord_comp_count++] = y3;
  triangle_count++;
}

static int cur_mask = -1;

/* Px converts view->pcb, Vx converts pcb->view */
      
static inline int 
Vx (int x)
{     
  int rv;
  if (hidgl_flip_x) 
    rv = (PCB->MaxWidth - x - gport->view_x0) / gport->zoom + 0.5;
  else
    rv = (x - gport->view_x0) / gport->zoom + 0.5;
  return rv;
}       
      
static inline int 
Vx2 (int x)
{     
  return (x - gport->view_x0) / gport->zoom + 0.5;
}       
      
static inline int
Vy (int y)
{         
  int rv;
  if (hidgl_flip_y)
    rv = (PCB->MaxHeight - y - gport->view_y0) / gport->zoom + 0.5;
  else
    rv = (y - gport->view_y0) / gport->zoom + 0.5;
  return rv;
}     
        
static inline int 
Vy2 (int y)
{     
  return (y - gport->view_y0) / gport->zoom + 0.5;
}       
      
static inline int
Vz (int z)
{           
  return z / gport->zoom + 0.5;
}         
                
static inline int
Px (int x)
{  
  int rv = x * gport->zoom + gport->view_x0;
  if (hidgl_flip_x)
    rv = PCB->MaxWidth - (x * gport->zoom + gport->view_x0);
  return  rv;
}  

static inline int
Py (int y)
{  
  int rv = y * gport->zoom + gport->view_y0;
  if (hidgl_flip_y)
    rv = PCB->MaxHeight - (y * gport->zoom + gport->view_y0);
  return  rv;
}  

static inline int  
Pz (int z)
{
  return (z * gport->zoom);
}

/* ------------------------------------------------------------ */

/* ------------------------------------------------------------ */

/*static*/ void
draw_grid ()
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

  hidgl_flush_triangles ();

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
  glFlush ();
}

/* ------------------------------------------------------------ */


int
hidgl_set_layer (const char *name, int group, int empty)
{
  int idx = (group >= 0
	     && group <
	     max_layer) ? PCB->LayerGroups.Entries[group][0] : group;

  if (idx >= 0 && idx < max_layer + 2) {
    gport->trans_lines = TRUE;
    return /*pinout ? 1 : */ PCB->Data->Layer[idx].On;
  }
  if (idx < 0)
    {
      switch (SL_TYPE (idx))
	{
	case SL_INVISIBLE:
	  return /* pinout ? 0 : */ PCB->InvisibleObjectsOn;
	case SL_MASK:
	  if (SL_MYSIDE (idx) /*&& !pinout */ )
	    return TEST_FLAG (SHOWMASKFLAG, PCB);
	  return 0;
	case SL_SILK:
//          gport->trans_lines = TRUE;
          gport->trans_lines = FALSE;
	  if (SL_MYSIDE (idx) /*|| pinout */ )
	    return PCB->ElementOn;
	  return 0;
	case SL_ASSY:
	  return 0;
	case SL_RATS:
	  gport->trans_lines = TRUE;
	  return 1;
	case SL_PDRILL:
	case SL_UDRILL:
	  return 1;
	}
    }
  return 0;
}

void
hidgl_use_mask (int use_it)
{
  if (use_it == cur_mask)
    return;

  hidgl_flush_triangles ();

  switch (use_it)
    {
    case HID_MASK_BEFORE:
      /* Write '1' to the stencil buffer where the solder-mask is drawn. */
      glColorMask (0, 0, 0, 0);                   // Disable writting in color buffer
      glEnable (GL_STENCIL_TEST);                 // Enable Stencil test
      glStencilFunc (GL_ALWAYS, 1, 1);            // Test always passes, value written 1
      glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE); // Stencil pass => replace stencil value (with 1)
      break;

    case HID_MASK_CLEAR:
      /* Drawing operations clear the stencil buffer to '0' */
      glStencilFunc (GL_ALWAYS, 0, 1);            // Test always passes, value written 0
      glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE); // Stencil pass => replace stencil value (with 0)
      break;

    case HID_MASK_AFTER:
      /* Drawing operations as masked to areas where the stencil buffer is '1' */
      glColorMask (1, 1, 1, 1);                   // Enable drawing of r, g, b & a
      glStencilFunc (GL_EQUAL, 1, 1);             // Draw only where stencil buffer is 1
      glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);    // Stencil buffer read only
      break;

    case HID_MASK_OFF:
      /* Disable stenciling */
      glDisable (GL_STENCIL_TEST);                // Disable Stencil test
      break;
    }
  cur_mask = use_it;
}

typedef struct
{
  int color_set;
//  GdkColor color;
  int xor_set;
//  GdkColor xor_color;
  double red;
  double green;
  double blue;
} ColorCache;


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
hidgl_set_special_colors (HID_Attribute * ha)
{
  if (!ha->name || !ha->value)
    return;
  if (!strcmp (ha->name, "background-color"))
    {
      hidgl_map_color_string (*(char **) ha->value, &gport->bg_color);
      set_special_grid_color ();
    }
  else if (!strcmp (ha->name, "off-limit-color"))
  {
      hidgl_map_color_string (*(char **) ha->value, &gport->offlimits_color);
    }
  else if (!strcmp (ha->name, "grid-color"))
    {
      hidgl_map_color_string (*(char **) ha->value, &gport->grid_color);
      set_special_grid_color ();
    }
}


void
hidgl_set_color (hidGC gc, const char *name)
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

  if( ! hidgl_gui_is_up )
    return;

  hidgl_flush_triangles ();
  glColor4d (r, g, b, a);
}

void
hidgl_set_line_cap (hidGC gc, EndCapStyle style)
{
  switch (style)
    {
    case Trace_Cap:
    case Round_Cap:
      gc->cap = GDK_CAP_ROUND;
      gc->join = GDK_JOIN_ROUND;
      break;
    case Square_Cap:
    case Beveled_Cap:
      gc->cap = GDK_CAP_PROJECTING;
      gc->join = GDK_JOIN_MITER;
      break;
    }
}

void
hidgl_set_line_width (hidGC gc, int width)
{
  gc->width = width;
}

void
hidgl_set_draw_xor (hidGC gc, int xor)
{
  // printf ("hidgl_set_draw_xor (%p, %d) -- not implemented\n", gc, xor);
  /* NOT IMPLEMENTED */

  /* Only presently called when setting up a crosshair GC.
   * We manage our own drawing model for that anyway. */
}

void
hidgl_set_draw_faded (hidGC gc, int faded)
{
  printf ("hidgl_set_draw_faded(%p,%d) -- not implemented\n", gc, faded);
}

void
hidgl_set_line_cap_angle (hidGC gc, int x1, int y1, int x2, int y2)
{
  printf ("hidgl_set_line_cap_angle() -- not implemented\n");
}

static void
use_gc (hidGC gc)
{
  static hidGC current_gc = NULL;

  if (current_gc == gc)
    return;

  current_gc = gc;

  hidgl_set_color (gc, gc->colorname);
}

void
errorCallback(GLenum errorCode)
{
   const GLubyte *estring;

   estring = gluErrorString(errorCode);
   fprintf(stderr, "Quadric Error: %s\n", estring);
//   exit(0);
}


void
hidgl_draw_line (hidGC gc, int x1, int y1, int x2, int y2)
{
#define TRIANGLES_PER_CAP 15
#define MIN_TRIANGLES_PER_CAP 3
#define MAX_TRIANGLES_PER_CAP 1000
  double dx1, dy1, dx2, dy2;
  double width, angle;
  float deltax, deltay, length;
  float wdx, wdy;
  int slices;
  int circular_caps = 0;

#if 0
  if (! ClipLine (0, 0, gport->width, gport->height,
  if (! ClipLine (0, 0, gport->width, gport->height,
                  &dx1, &dy1, &dx2, &dy2, gc->width / gport->zoom))
    return;
#endif

  USE_GC (gc);

  dx1 = Vx (x1);
  dy1 = Vy (y1);
  dx2 = Vx (x2);
  dy2 = Vy (y2);

  width = Vz (gc->width);

  if (width == 0.0)
    width = 1.0;

  deltax = dx2 - dx1;
  deltay = dy2 - dy1;

  length = sqrt (deltax * deltax + deltay * deltay);

  if (length == 0) {
    angle = 0;
    wdx = -width / 2.;
    wdy = 0;
  } else {
    wdy = deltax * width / 2. / length;
    wdx = -deltay * width / 2. / length;

    if (deltay == 0.)
      angle = (deltax < 0) ? 270. : 90.;
    else
      angle = 180. / M_PI * atanl (deltax / deltay);

    if (deltay < 0)
      angle += 180.;
  }

  slices = M_PI * width / PIXELS_PER_CIRCLINE;

  if (slices < MIN_TRIANGLES_PER_CAP)
    slices = MIN_TRIANGLES_PER_CAP;

  if (slices > MAX_TRIANGLES_PER_CAP)
    slices = MAX_TRIANGLES_PER_CAP;

//  slices = TRIANGLES_PER_CAP;

  switch (gc->cap) {
    case Trace_Cap:
    case Round_Cap:
      circular_caps = 1;
      break;

    case Square_Cap:
    case Beveled_Cap:
      dx1 -= deltax * width / 2. / length;
      dy1 -= deltay * width / 2. / length;
      dx2 += deltax * width / 2. / length;
      dy2 += deltay * width / 2. / length;
      break;
  }

  ensure_triangle_space (2);
  add_triangle (dx1 - wdx, dy1 - wdy, dx2 - wdx, dy2 - wdy, dx2 + wdx, dy2 + wdy);
  add_triangle (dx1 - wdx, dy1 - wdy, dx2 + wdx, dy2 + wdy, dx1 + wdx, dy1 + wdy);

  if (circular_caps) {
    int i;
    float last_capx, last_capy;

    ensure_triangle_space (2 * slices);

    last_capx = ((float)width) / 2. * cos (angle * M_PI / 180.) + dx1;
    last_capy = -((float)width) / 2. * sin (angle * M_PI / 180.) + dy1;
    for (i = 0; i < slices; i++) {
      float capx, capy;
      capx = ((float)width) / 2. * cos (angle * M_PI / 180. + ((float)(i + 1)) * M_PI / (float)slices) + dx1;
      capy = -((float)width) / 2. * sin (angle * M_PI / 180. + ((float)(i + 1)) * M_PI / (float)slices) + dy1;
      add_triangle (last_capx, last_capy, capx, capy, dx1, dy1);
      last_capx = capx;
      last_capy = capy;
    }
    last_capx = -((float)width) / 2. * cos (angle * M_PI / 180.) + dx2;
    last_capy = ((float)width) / 2. * sin (angle * M_PI / 180.) + dy2;
    for (i = 0; i < slices; i++) {
      float capx, capy;
      capx = -((float)width) / 2. * cos (angle * M_PI / 180. + ((float)(i + 1)) * M_PI / (float)slices) + dx2;
      capy = ((float)width) / 2. * sin (angle * M_PI / 180. + ((float)(i + 1)) * M_PI / (float)slices) + dy2;
      add_triangle (last_capx, last_capy, capx, capy, dx2, dy2);
      last_capx = capx;
      last_capy = capy;
    }
  }
}

void
hidgl_draw_arc (hidGC gc, int cx, int cy,
               int xradius, int yradius, int start_angle, int delta_angle)
{
#define MIN_SLICES_PER_ARC 10
  int vrx, vry;
  int w, h, radius, slices;
  double width;
  GLUquadricObj *qobj;

  width = Vz (gc->width);

  if (width == 0.0)
    width = 1.0;

  w = gport->width * gport->zoom;
  h = gport->height * gport->zoom;
  radius = (xradius > yradius) ? xradius : yradius;
  if (SIDE_X (cx) < gport->view_x0 - radius
      || SIDE_X (cx) > gport->view_x0 + w + radius
      || SIDE_Y (cy) < gport->view_y0 - radius 
      || SIDE_Y (cy) > gport->view_y0 + h + radius)
    return;
  
  USE_GC (gc);
  vrx = Vz (xradius);
  vry = Vz (yradius);

  if (hidgl_flip_x)
    {
      start_angle = 180 - start_angle;
      delta_angle = - delta_angle;
    }
  if (hidgl_flip_y)
    {
      start_angle = - start_angle;
      delta_angle = - delta_angle;					
    }
  /* make sure we fall in the -180 to +180 range */
  start_angle = (start_angle + 360 + 180) % 360 - 180;

  if (delta_angle < 0) {
    start_angle += delta_angle;
    delta_angle = - delta_angle;
  }

  slices = M_PI * (vrx + width / 2.) / PIXELS_PER_CIRCLINE;

  if (slices < MIN_SLICES_PER_ARC)
    slices = MIN_SLICES_PER_ARC;

  /* TODO: CHANGE TO USING THE TRIANGLE LIST */
  qobj = gluNewQuadric ();
  gluQuadricCallback (qobj, GLU_ERROR, errorCallback);
  gluQuadricDrawStyle (qobj, GLU_FILL); /* smooth shaded */
  gluQuadricNormals (qobj, GLU_SMOOTH);

  glPushMatrix ();
  glTranslatef (Vx (cx), Vy (cy), 0.0);
  gluPartialDisk (qobj, vrx - width / 2, vrx + width / 2, slices, 1, 270 + start_angle, delta_angle);
  glPopMatrix ();

  slices = M_PI * width / PIXELS_PER_CIRCLINE;

  if (slices < MIN_TRIANGLES_PER_CAP)
    slices = MIN_TRIANGLES_PER_CAP;

  /* TODO: CHANGE TO USING THE TRIANGLE LIST */
  glPushMatrix ();
  glTranslatef (Vx (cx) + vrx * -cos (M_PI / 180. * start_angle),
                Vy (cy) + vrx *  sin (M_PI / 180. * start_angle), 0.0);
  gluPartialDisk (qobj, 0, width / 2, slices, 1, start_angle + 90., 180);
  glPopMatrix ();

  /* TODO: CHANGE TO USING THE TRIANGLE LIST */
  glPushMatrix ();
  glTranslatef (Vx (cx) + vrx * -cos (M_PI / 180. * (start_angle + delta_angle)),
                Vy (cy) + vrx *  sin (M_PI / 180. * (start_angle + delta_angle)), 0.0);
  gluPartialDisk (qobj, 0, width / 2, slices, 1, start_angle + delta_angle + 270., 180);
  glPopMatrix ();

  gluDeleteQuadric (qobj);
}

void
hidgl_draw_rect (hidGC gc, int x1, int y1, int x2, int y2)
{
  int w, h, lw;

  lw = gc->width;
  w = gport->width * gport->zoom;
  h = gport->height * gport->zoom;

  if ((SIDE_X (x1) < gport->view_x0 - lw
       && SIDE_X (x2) < gport->view_x0 - lw)
      || (SIDE_X (x1) > gport->view_x0 + w + lw
	  && SIDE_X (x2) > gport->view_x0 + w + lw)
      || (SIDE_Y (y1) < gport->view_y0 - lw 
	  && SIDE_Y (y2) < gport->view_y0 - lw)
      || (SIDE_Y (y1) > gport->view_y0 + h + lw 
	  && SIDE_Y (y2) > gport->view_y0 + h + lw))
    return;

  x1 = Vx (x1);
  y1 = Vy (y1);
  x2 = Vx (x2);
  y2 = Vy (y2);

  USE_GC (gc);

  glBegin (GL_LINE_LOOP);
  glVertex2f (x1, y1);
  glVertex2f (x1, y2);
  glVertex2f (x2, y2);
  glVertex2f (x2, y1);
  glEnd ();
}


void
hidgl_fill_circle (hidGC gc, int cx, int cy, int radius)
{
#define TRIANGLES_PER_CIRCLE 30
#define MIN_TRIANGLES_PER_CIRCLE 6
#define MAX_TRIANGLES_PER_CIRCLE 2000
  int w, h, vx, vy, vr;
  float last_x, last_y;
  int slices;
  int i;

  w = gport->width * gport->zoom;
  h = gport->height * gport->zoom;
  if (SIDE_X (cx) < gport->view_x0 - radius
      || SIDE_X (cx) > gport->view_x0 + w + radius
      || SIDE_Y (cy) < gport->view_y0 - radius 
      || SIDE_Y (cy) > gport->view_y0 + h + radius)
    return;

  USE_GC (gc);
  vx = Vx (cx);
  vy = Vy (cy);
  vr = Vz (radius);

  slices = M_PI * 2 * vr / PIXELS_PER_CIRCLINE;

  if (slices < MIN_TRIANGLES_PER_CIRCLE)
    slices = MIN_TRIANGLES_PER_CIRCLE;

  if (slices > MAX_TRIANGLES_PER_CIRCLE)
    slices = MAX_TRIANGLES_PER_CIRCLE;

//  slices = TRIANGLES_PER_CIRCLE;

  ensure_triangle_space (slices);

  last_x = vx + vr;
  last_y = vy;

  for (i = 0; i < slices; i++) {
    float x, y;
    x = ((float)vr) * cos (((float)(i + 1)) * 2. * M_PI / (float)slices) + vx;
    y = ((float)vr) * sin (((float)(i + 1)) * 2. * M_PI / (float)slices) + vy;
    add_triangle (vx, vy, last_x, last_y, x, y);
    last_x = x;
    last_y = y;
  }
}

#define MAX_COMBINED_MALLOCS 2500
static void *combined_to_free [MAX_COMBINED_MALLOCS];
static int combined_num_to_free = 0;

static GLenum tessVertexType;
static int stashed_vertices;
static int triangle_comp_idx;


void
myError (GLenum errno)
{
  printf ("gluTess error: %s\n", gluErrorString (errno));
}

static void
myFreeCombined ()
{
  while (combined_num_to_free)
    free (combined_to_free [-- combined_num_to_free]);
}

static void
myCombine ( GLdouble coords[3], void *vertex_data[4], GLfloat weight[4], void **dataOut )
{
#define MAX_COMBINED_VERTICES 2500
  static GLdouble combined_vertices [3 * MAX_COMBINED_VERTICES];
  static int num_combined_vertices = 0;

  GLdouble *new_vertex;

  if (num_combined_vertices < MAX_COMBINED_VERTICES)
    {
      new_vertex = &combined_vertices [3 * num_combined_vertices];
      num_combined_vertices ++;
    }
  else
    {
      new_vertex = malloc (3 * sizeof (GLdouble));

      if (combined_num_to_free < MAX_COMBINED_MALLOCS)
        combined_to_free [combined_num_to_free ++] = new_vertex;
      else
        printf ("myCombine leaking %i bytes of memory\n", 3 * sizeof (GLdouble));
    }

  new_vertex[0] = coords[0];
  new_vertex[1] = coords[1];
  new_vertex[2] = coords[2];

  *dataOut = new_vertex;
}

static void
myBegin (GLenum type)
{
  tessVertexType = type;
  stashed_vertices = 0;
  triangle_comp_idx = 0;
}

void
myVertex (GLdouble *vertex_data)
{
  static GLfloat triangle_vertices [2 * 3];

  if (tessVertexType == GL_TRIANGLE_STRIP ||
      tessVertexType == GL_TRIANGLE_FAN)
    {
      if (stashed_vertices < 2)
        {
          triangle_vertices [triangle_comp_idx ++] = vertex_data [0];
          triangle_vertices [triangle_comp_idx ++] = vertex_data [1];
          stashed_vertices ++;
        }
      else
        {
          ensure_triangle_space (1);
          add_triangle (triangle_vertices [0], triangle_vertices [1],
                        triangle_vertices [2], triangle_vertices [3],
                        vertex_data [0], vertex_data [1]);

          if (tessVertexType == GL_TRIANGLE_STRIP)
            {
              /* STRIP saves the last two vertices for re-use in the next triangle */
              triangle_vertices [0] = triangle_vertices [2];
              triangle_vertices [1] = triangle_vertices [3];
            }
          /* Both FAN and STRIP save the last vertex for re-use in the next triangle */
          triangle_vertices [2] = vertex_data [0];
          triangle_vertices [3] = vertex_data [1];
        }
    }
  else if (tessVertexType == GL_TRIANGLES)
    {
      triangle_vertices [triangle_comp_idx ++] = vertex_data [0];
      triangle_vertices [triangle_comp_idx ++] = vertex_data [1];
      stashed_vertices ++;
      if (stashed_vertices == 3)
        {
          ensure_triangle_space (1);
          add_triangle (triangle_vertices [0], triangle_vertices [1],
                        triangle_vertices [2], triangle_vertices [3],
                        triangle_vertices [4], triangle_vertices [5]);
          triangle_comp_idx = 0;
          stashed_vertices = 0;
        }
    }
  else
    printf ("Vertex recieved with unknown type\n");
}

void
hidgl_fill_polygon (hidGC gc, int n_coords, int *x, int *y)
{
  int i;

  GLUtesselator *tobj;
  GLdouble *vertices;

  USE_GC (gc);

  assert (n_coords > 0);

  vertices = malloc (sizeof(GLdouble) * n_coords * 3);

  tobj = gluNewTess ();
  gluTessCallback(tobj, GLU_TESS_BEGIN, myBegin);
  gluTessCallback(tobj, GLU_TESS_VERTEX, myVertex);
  gluTessCallback(tobj, GLU_TESS_COMBINE, myCombine);
  gluTessCallback(tobj, GLU_TESS_ERROR, myError);

  gluTessBeginPolygon (tobj, NULL);
  gluTessBeginContour (tobj);

  for (i = 0; i < n_coords; i++)
    {
      vertices [0 + i * 3] = Vx (x[i]);
      vertices [1 + i * 3] = Vy (y[i]);
      vertices [2 + i * 3] = 0.;
      gluTessVertex (tobj, &vertices [i * 3], &vertices [i * 3]);
    }

  gluTessEndContour (tobj);
  gluTessEndPolygon (tobj);
  gluDeleteTess (tobj);

  myFreeCombined ();
  free (vertices);
}

void
hidgl_fill_rect (hidGC gc, int x1, int y1, int x2, int y2)
{
  int w, h, lw;

  lw = gc->width;
  w = gport->width * gport->zoom;
  h = gport->height * gport->zoom;

  if ((SIDE_X (x1) < gport->view_x0 - lw
       && SIDE_X (x2) < gport->view_x0 - lw)
      || (SIDE_X (x1) > gport->view_x0 + w + lw
          && SIDE_X (x2) > gport->view_x0 + w + lw)
      || (SIDE_Y (y1) < gport->view_y0 - lw
          && SIDE_Y (y2) < gport->view_y0 - lw)
      || (SIDE_Y (y1) > gport->view_y0 + h + lw
          && SIDE_Y (y2) > gport->view_y0 + h + lw))
    return;

  x1 = Vx (x1);
  y1 = Vy (y1);
  x2 = Vx (x2);
  y2 = Vy (y2);

  USE_GC (gc);
  glBegin (GL_QUADS);
  glVertex2f (x1, y1);
  glVertex2f (x1, y2);
  glVertex2f (x2, y2);
  glVertex2f (x2, y1);
  glEnd ();
}

/* ---------------------------------------------------------------------- */

HID hidgl_hid = {
  sizeof (HID),
  "",
  "",
  1,				/* gui */
  0,				/* printer */
  0,				/* exporter */
  0,				/* poly before */
  1,				/* poly after */
  0,				/* poly dicer */

  NULL, /* hidgl_get_export_options */
  NULL, /* hidgl_do_export */
  NULL, /* hidgl_parse_arguments */

  NULL, /* hidgl_invalidate_wh */
  NULL, /* hidgl_invalidate_lr */
  NULL, /* hidgl_invalidate_all */
  hidgl_set_layer,
  hidgl_make_gc,
  hidgl_destroy_gc,
  hidgl_use_mask,
  hidgl_set_color,
  hidgl_set_line_cap,
  hidgl_set_line_width,
  hidgl_set_draw_xor,
  hidgl_set_draw_faded,
  hidgl_set_line_cap_angle,
  hidgl_draw_line,
  hidgl_draw_arc,
  hidgl_draw_rect,
  hidgl_fill_circle,
  hidgl_fill_polygon,
  hidgl_fill_rect,

  NULL, /* hidgl_calibrate */
  NULL, /* hidgl_shift_is_pressed */
  NULL, /* hidgl_control_is_pressed */
  NULL, /* hidgl_get_coords */
  NULL, /* hidgl_set_crosshair */
  NULL, /* hidgl_add_timer */
  NULL, /* hidgl_stop_timer */
  NULL, /* hidgl_watch_file */
  NULL, /* hidgl_unwatch_file */
  NULL, /* hidgl_add_block_hook */
  NULL, /* hidgl_stop_block_hook */

  NULL, /* hidgl_log */
  NULL, /* hidgl_logv */
  NULL, /* hidgl_confirm_dialog */
  NULL, /* hidgl_close_confirm_dialog */
  NULL, /* hidgl_report_dialog */
  NULL, /* hidgl_prompt_for */
  NULL, /* hidgl_fileselect */
  NULL, /* hidgl_attribute_dialog */
  NULL, /* hidgl_show_item */
  NULL, /* hidgl_beep */
  NULL, /* hidgl_progress */
};

