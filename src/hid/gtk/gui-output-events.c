/* $Id$ */

/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996,1997,1998,1999 Thomas Nau
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Contact addresses for paper mail and Email:
 *  Thomas Nau, Schlehenweg 15, 88471 Baustetten, Germany
 *  Thomas.Nau@rz.uni-ulm.de
 *
 */

/* This file written by Bill Wilson for the PCB Gtk port */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gui.h"
#include "gtkhid.h"
#include "hid/common/hid_resource.h"
#include "hid/common/draw_helpers.h"
#include "hid/common/trackball.h"

#include <gdk/gdkkeysyms.h>

#include "action.h"
#include "crosshair.h"
#include "draw.h"
#include "error.h"
#include "misc.h"
#include "set.h"
#include "rtree.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

RCSID ("$Id$");

static gint x_pan_speed, y_pan_speed;
static GLfloat view_matrix[4][4] = {{1.0, 0.0, 0.0, 0.0},
                                    {0.0, 1.0, 0.0, 0.0},
                                    {0.0, 0.0, 1.0, 0.0},
                                    {0.0, 0.0, 0.0, 1.0}};
static GLfloat last_modelview_matrix[4][4] = {{1.0, 0.0, 0.0, 0.0},
                                              {0.0, 1.0, 0.0, 0.0},
                                              {0.0, 0.0, 1.0, 0.0},
                                              {0.0, 0.0, 0.0, 1.0}};
static int global_view_2d = 1;


/* Set to true if cursor is currently in viewport. This is a hack to prevent
 * Crosshair stack corruption due to unmatching window enter / leave events */
gboolean cursor_in_viewport = False;

void
ghid_port_ranges_changed (void)
{
  GtkAdjustment *h_adj, *v_adj;

  if (!ghidgui->combine_adjustments)
    HideCrosshair (FALSE);
  if (ghidgui->combine_adjustments)
    {
      ghidgui->combine_adjustments = FALSE;
      return;
    }

  ghidgui->need_restore_crosshair = TRUE;

  h_adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->h_range));
  v_adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->v_range));
  gport->view_x0 = h_adj->value;
  gport->view_y0 = v_adj->value;

  ghid_invalidate_all ();
}

gboolean
ghid_port_ranges_pan (gdouble x, gdouble y, gboolean relative)
{
  GtkAdjustment *h_adj, *v_adj;
  gdouble x0, y0, x1, y1;

  h_adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->h_range));
  v_adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->v_range));
  x0 = h_adj->value;
  y0 = v_adj->value;

  if (relative)
    {
      x1 = x0 + x;
      y1 = y0 + y;
    }
  else
    {
      x1 = x;
      y1 = y;
    }

  if (x1 < h_adj->lower)
    x1 = h_adj->lower;
  if (x1 > h_adj->upper - h_adj->page_size)
    x1 = h_adj->upper - h_adj->page_size;

  if (y1 < v_adj->lower)
    y1 = v_adj->lower;
  if (y1 > v_adj->upper - v_adj->page_size)
    y1 = v_adj->upper - v_adj->page_size;

  if (x0 != x1 && y0 != y1)
    ghidgui->combine_adjustments = TRUE;
  if (x0 != x1)
    gtk_range_set_value (GTK_RANGE (ghidgui->h_range), x1);
  if (y0 != y1)
    gtk_range_set_value (GTK_RANGE (ghidgui->v_range), y1);

  ghid_note_event_location (NULL);
  return ((x0 != x1) || (y0 != y1));
}

  /* Do scrollbar scaling based on current port drawing area size and
     |  overall PCB board size.
   */
void
ghid_port_ranges_scale (gboolean emit_changed)
{
  GtkAdjustment *adj;

  /* Update the scrollbars with PCB units.  So Scale the current
     |  drawing area size in pixels to PCB units and that will be
     |  the page size for the Gtk adjustment.
   */
  gport->view_width = gport->width * gport->zoom;
  gport->view_height = gport->height * gport->zoom;

  if (gport->view_width >= PCB->MaxWidth)
    gport->view_width = PCB->MaxWidth;
  if (gport->view_height >= PCB->MaxHeight)
    gport->view_height = PCB->MaxHeight;

  adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->h_range));
  adj->page_size = gport->view_width;
  adj->page_increment = adj->page_size/10.0;
  adj->step_increment = adj->page_size/100.0;
  adj->upper = PCB->MaxWidth;
  if (emit_changed)
    gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");

  adj = gtk_range_get_adjustment (GTK_RANGE (ghidgui->v_range));
  adj->page_size = gport->view_height;
  adj->page_increment = adj->page_size/10.0;
  adj->step_increment = adj->page_size/100.0;
  adj->upper = PCB->MaxHeight;
  if (emit_changed)
    gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
}

void
ghid_port_ranges_zoom (gdouble zoom)
{
  gdouble xtmp, ytmp;
  gint		x0, y0;

  /* figure out zoom values in that would just make the width fit and
   * that would just make the height fit
   */
  xtmp = (gdouble) PCB->MaxWidth / gport->width;
  ytmp = (gdouble) PCB->MaxHeight / gport->height;

  /* if we've tried to zoom further out than what would make the
   * entire board fit or we passed 0, then pick a zoom that just makes
   * the board fit.
   */
  if ((zoom > xtmp && zoom > ytmp) || zoom == 0.0)
    zoom = (xtmp > ytmp) ? xtmp : ytmp;

  xtmp = (gport->view_x - gport->view_x0) / (gdouble) gport->view_width;
  ytmp = (gport->view_y - gport->view_y0) / (gdouble) gport->view_height;

  gport->zoom = zoom;
  pixel_slop = zoom;
  ghid_port_ranges_scale(FALSE);

  x0 = gport->view_x - xtmp * gport->view_width;
  if (x0 < 0)
    x0 = 0;
  gport->view_x0 = x0;

  y0 = gport->view_y - ytmp * gport->view_height;
  if (y0 < 0)
    y0 = 0;
  gport->view_y0 = y0;

  ghidgui->adjustment_changed_holdoff = TRUE;
  gtk_range_set_value (GTK_RANGE (ghidgui->h_range), gport->view_x0);
  gtk_range_set_value (GTK_RANGE (ghidgui->v_range), gport->view_y0);
  ghidgui->adjustment_changed_holdoff = FALSE;

  ghid_port_ranges_changed();
}


/* ---------------------------------------------------------------------- 
 * handles all events from PCB drawing area
 */

static gint event_x, event_y;

void
ghid_get_coords (const char *msg, int *x, int *y)
{
  if (!ghid_port.has_entered && msg)
    ghid_get_user_xy (msg);
  if (ghid_port.has_entered)
    {
      *x = SIDE_X (gport->view_x);
      *y = SIDE_Y (gport->view_y);
    }
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


gboolean
ghid_note_event_location (GdkEventButton * ev)
{
  gint x, y;
  gboolean moved;

  if (!ev)
    {
      gdk_window_get_pointer (ghid_port.drawing_area->window, &x, &y, NULL);
      event_x = x;
      event_y = y;
    }
  else
    {
      event_x = ev->x;
      event_y = ev->y;
    }

  /* Unproject event_x and event_y to world coordinates of the plane we are on */
  ghid_unproject_to_z_plane (event_x, event_y, global_depth,
                             &event_x, &event_y);

  gport->view_x = event_x * gport->zoom + gport->view_x0;
  gport->view_y = event_y * gport->zoom + gport->view_y0;

  moved = MoveCrosshairAbsolute (SIDE_X (gport->view_x), 
				 SIDE_Y (gport->view_y));
  if (moved)
    {
      AdjustAttachedObjects ();
      RestoreCrosshair (False);
    }
  ghid_set_cursor_position_labels ();
  return moved;
}

static gboolean
have_crosshair_attachments (void)
{
  gboolean result = FALSE;

  switch (Settings.Mode)
    {
    case COPY_MODE:
    case MOVE_MODE:
    case INSERTPOINT_MODE:
      if (Crosshair.AttachedObject.Type != NO_TYPE)
	result = TRUE;
      break;
    case PASTEBUFFER_MODE:
    case VIA_MODE:
      result = TRUE;
      break;
    case POLYGON_MODE:
      if (Crosshair.AttachedLine.State != STATE_FIRST)
	result = TRUE;
      break;
    case ARC_MODE:
      if (Crosshair.AttachedBox.State != STATE_FIRST)
	result = TRUE;
      break;
    case LINE_MODE:
      if (Crosshair.AttachedLine.State != STATE_FIRST)
	result = TRUE;
      break;
    default:
      if (Crosshair.AttachedBox.State == STATE_SECOND
	  || Crosshair.AttachedBox.State == STATE_THIRD)
	result = TRUE;
      break;
    }
  return result;
}


#define	VCW		16
#define VCD		8

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

void
ghid_show_crosshair (gboolean show)
{
  gint x, y, z;
  static gint x_prev = -1, y_prev = -1, z_prev = -1;
  static int done_once = 0;
  static GdkColor cross_color;
  extern float global_depth;

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

#if 1
  if (x_prev >= 0)
    {
      draw_crosshair (x_prev, y_prev, z_prev);
    }
#endif

  if (x >= 0 && show)
    {
      draw_crosshair (x, y, z);
    }

  glEnd ();

  if (ghidgui->auto_pan_on && have_crosshair_attachments ())
    {
      glBegin (GL_QUADS);

#if 1
      if (x_prev >= 0)
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

      if (x >= 0 && show)
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
    }

  if (x >= 0 && show)
    {
      x_prev = x;
      y_prev = y;
      z_prev = z;
    }
  else
    x_prev = y_prev = z_prev = -1;

  glDisable (GL_COLOR_LOGIC_OP);
}

static gboolean
ghid_idle_cb (gpointer data)
{
  if (Settings.Mode == NO_MODE)
    SetMode (ARROW_MODE);
  ghid_mode_cursor (Settings.Mode);
  if (ghidgui->settings_mode != Settings.Mode)
    {
      ghid_mode_buttons_update ();
    }
  ghidgui->settings_mode = Settings.Mode;

  ghid_update_toggle_flags ();
  return FALSE;
}

gboolean
ghid_port_key_release_cb (GtkWidget * drawing_area, GdkEventKey * kev,
			  GtkUIManager * ui)
{
  gint ksym = kev->keyval;

  if (ghid_is_modifier_key_sym (ksym))
    ghid_note_event_location (NULL);

  HideCrosshair (TRUE);
  AdjustAttachedObjects ();
  ghid_invalidate_all ();
  RestoreCrosshair (TRUE);
  ghid_screen_update ();
  g_idle_add (ghid_idle_cb, NULL);
  return FALSE;
}

/* Handle user keys in the output drawing area.
 * Note that the default is for all hotkeys to be handled by the
 * menu accelerators.
 *
 * Key presses not handled by the menus will show up here.  This means
 * the key press was either not defined in the menu resource file or
 * that the key press is special in that gtk doesn't allow the normal
 * menu code to ever see it.  We capture those here (like Tab and the
 * arrow keys) and feed it back to the normal menu callback.
 */

gboolean
ghid_port_key_press_cb (GtkWidget * drawing_area,
			GdkEventKey * kev, GtkUIManager * ui)
{
  ModifierKeysState mk;
  gint  ksym = kev->keyval;
  gboolean handled;
  extern  void ghid_hotkey_cb (int);
  GdkModifierType state;

  if (ghid_is_modifier_key_sym (ksym))
    ghid_note_event_location (NULL);

  state = (GdkModifierType) (kev->state);
  mk = ghid_modifier_keys_state (&state);

  ghid_show_crosshair (FALSE);

  handled = TRUE;		/* Start off assuming we handle it */
  switch (ksym)
    {
    case GDK_Alt_L:
    case GDK_Alt_R:
    case GDK_Control_L:
    case GDK_Control_R:
    case GDK_Shift_L:
    case GDK_Shift_R:
    case GDK_Shift_Lock:
      break;

    case GDK_Up:
      ghid_hotkey_cb (GHID_KEY_UP);
      break;
      
    case GDK_Down:
      ghid_hotkey_cb (GHID_KEY_DOWN);
      break;
    case GDK_Left:
      ghid_hotkey_cb (GHID_KEY_LEFT);
      break;
    case GDK_Right:
      ghid_hotkey_cb (GHID_KEY_RIGHT);
      break;

    case GDK_ISO_Left_Tab: 
    case GDK_3270_BackTab: 
      switch (mk) 
	{
	case NONE_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case CONTROL_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_CONTROL | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case MOD1_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_ALT | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case SHIFT_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case SHIFT_CONTROL_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_CONTROL | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case SHIFT_MOD1_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_ALT | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	  
	default:
	  handled = FALSE;
	  break;
	}
      break;

    case GDK_Tab: 
      switch (mk) 
	{
	case NONE_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_TAB);
	  break;
	case CONTROL_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_CONTROL | GHID_KEY_TAB);
	  break;
	case MOD1_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_ALT | GHID_KEY_TAB);
	  break;
	case SHIFT_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case SHIFT_CONTROL_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_CONTROL | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	case SHIFT_MOD1_PRESSED:
	  ghid_hotkey_cb (GHID_KEY_ALT | GHID_KEY_SHIFT | GHID_KEY_TAB);
	  break;
	  
	default:
	  handled = FALSE;
	  break;
	}
      break;

    default:
      handled = FALSE;
    }

  return handled;
}

gboolean
ghid_port_button_press_cb (GtkWidget * drawing_area,
			   GdkEventButton * ev, GtkUIManager * ui)
{
  ModifierKeysState mk;
  gboolean drag;
  GdkModifierType state;

  /* Reject double and triple click events */
  if (ev->type != GDK_BUTTON_PRESS) return TRUE;

  ghid_note_event_location (ev);
  state = (GdkModifierType) (ev->state);
  mk = ghid_modifier_keys_state (&state);
  ghid_show_crosshair (FALSE);
  HideCrosshair (TRUE);
  drag = have_crosshair_attachments ();

  do_mouse_action(ev->button, mk);

  ghid_invalidate_all ();
  RestoreCrosshair (TRUE);
  ghid_set_status_line_label ();
  ghid_show_crosshair (TRUE);
  if (!gport->panning)
    g_idle_add (ghid_idle_cb, NULL);
  return TRUE;
}


gboolean
ghid_port_button_release_cb (GtkWidget * drawing_area,
			     GdkEventButton * ev, GtkUIManager * ui)
{
  ModifierKeysState mk;
  gboolean drag;
  GdkModifierType state;

  ghid_note_event_location (ev);
  state = (GdkModifierType) (ev->state);
  mk = ghid_modifier_keys_state (&state);

  drag = have_crosshair_attachments ();
  if (drag)
    HideCrosshair (TRUE);

  do_mouse_action(ev->button, mk + M_Release);

  if (drag)
    {
      AdjustAttachedObjects ();
      ghid_invalidate_all ();
      RestoreCrosshair (TRUE);
      ghid_screen_update ();
    }
  ghid_set_status_line_label ();
  g_idle_add (ghid_idle_cb, NULL);
  return TRUE;
}


gboolean
ghid_port_drawing_area_configure_event_cb (GtkWidget * widget,
					   GdkEventConfigure * ev,
					   GHidPort * out)
{
  static gboolean first_time_done;

  HideCrosshair (TRUE);
  gport->width = ev->width;
  gport->height = ev->height;

  if (gport->pixmap)
    gdk_pixmap_unref (gport->pixmap);

  gport->pixmap = gdk_pixmap_new (widget->window,
				  gport->width, gport->height, -1);
  gport->drawable = gport->pixmap;

  if (!first_time_done)
    {
      gport->colormap = gtk_widget_get_colormap (gport->top_window);
      if (gdk_color_parse (Settings.BackgroundColor, &gport->bg_color))
	gdk_color_alloc (gport->colormap, &gport->bg_color);
      else
	gdk_color_white (gport->colormap, &gport->bg_color);

      if (gdk_color_parse (Settings.OffLimitColor, &gport->offlimits_color))
	gdk_color_alloc (gport->colormap, &gport->offlimits_color);
      else
	gdk_color_white (gport->colormap, &gport->offlimits_color);
      first_time_done = TRUE;
      PCBChanged (0, NULL, 0, 0);
    }
//  if (gport->mask)
//    {
//      gdk_pixmap_unref (gport->mask);
//      gport->mask = gdk_pixmap_new (0, gport->width, gport->height, 1);
//    }
  ghid_port_ranges_scale (FALSE);
  ghid_invalidate_all ();
  RestoreCrosshair (TRUE);
  return 0;
}

void
ghid_screen_update (void)
{
#if 0
  ghid_show_crosshair (FALSE);
  gdk_draw_drawable (gport->drawing_area->window, gport->bg_gc, gport->pixmap,
		     0, 0, 0, 0, gport->width, gport->height);
  ghid_show_crosshair (TRUE);
#endif
}

void DrawAttached (Boolean);
void draw_grid (BoxTypePtr drawn_area);

struct pin_info
{
  Boolean arg;
  LayerTypePtr Layer;
  const BoxType *drawn_area;
};

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
    color = Layer->Color;

  gui->set_color (Output.fgGC, color);
}


static int
pin_inlayer_callback (const BoxType * b, void *cl)
{
  SetPVColor_inlayer ((PinTypePtr) b, cl, PIN_TYPE);
  DrawPinOrViaLowLevel ((PinTypePtr) b, False);
  return 1;
}

static int
via_inlayer_callback (const BoxType * b, void *cl)
{
  SetPVColor_inlayer ((PinTypePtr) b, cl, VIA_TYPE);
  DrawPinOrViaLowLevel ((PinTypePtr) b, False);
  return 1;
}

static int
pin_callback (const BoxType * b, void *cl)
{
  DrawPlainPin ((PinTypePtr) b, False);
  return 1;
}

static int
pad_callback (const BoxType * b, void *cl)
{
  PadTypePtr pad = (PadTypePtr) b;
  if (FRONT (pad))
    DrawPad (pad, 0);
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
  DrawPlainVia (via, False);
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

static void
DrawPadLowLevelSolid (hidGC gc, PadTypePtr Pad, Boolean clear, Boolean mask)
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
ClearPadSolid (PadTypePtr Pad, Boolean mask)
{
  DrawPadLowLevelSolid(Output.pmGC, Pad, True, mask);
}

static void
ClearOnlyPinSolid (PinTypePtr Pin, Boolean mask)
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
      gui->set_line_cap (Output.pmGC, Round_Cap);
      gui->set_line_width (Output.pmGC, (Pin->Clearance + Pin->Thickness
					 - Pin->DrillingHole));

      DrawSpecialPolygon (gui, Output.pmGC, Pin->X, Pin->Y, half * 2);
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
    ClearOnlyPinSolid (pin, True);
  return 1;
}

static int
clearPad_callback_solid (const BoxType * b, void *cl)
{
  PadTypePtr pad = (PadTypePtr) b;
  if (!XOR (TEST_FLAG (ONSOLDERFLAG, pad), SWAP_IDENT))
    ClearPadSolid (pad, True);
  return 1;
}

int clearPin_callback (const BoxType * b, void *cl);
int clearPad_callback (const BoxType * b, void *cl);


static void
DrawMask (BoxType * screen)
{
  struct pin_info info;
  int thin = TEST_FLAG(THINDRAWFLAG, PCB) || TEST_FLAG(THINDRAWPOLYFLAG, PCB);

  OutputType *out = &Output;

  info.arg = True;
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
  gui->fill_rect (out->fgGC, 0, 0, PCB->MaxWidth, PCB->MaxHeight);
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

    if (layernum < max_layer /*&& Layer->On*/) {

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
      if (Layer->PolygonN) {
        info.Layer = Layer;
        info.drawn_area = screen;
        r_search (Layer->polygon_tree, screen, NULL, poly_callback, &info);

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

      /* Draw pins and vias on this layer */
      if (!global_view_2d && rv) {
        if (PCB->PinOn) r_search (PCB->Data->pin_tree, screen, NULL, pin_inlayer_callback, Layer);
        if (PCB->ViaOn) r_search (PCB->Data->via_tree, screen, NULL, via_inlayer_callback, Layer);
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

extern int compute_depth (int group);

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
hole_cyl_callback (const BoxType * b, void *cl)
{
  PinTypePtr Pin = (PinTypePtr) b;
  struct cyl_info *info = cl;
  DrawDrillChannel (Pin->X, Pin->Y, Pin->DrillingHole / 2, info->from_layer, info->to_layer, info->scale);
  return 0;
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

  extern char *current_color;
  extern Boolean Gathering;

  current_color = NULL;
  Gathering = False;

  /* Test direction of rendering */
  /* Look at sign of eye coordinate system z-coord when projecting a
     world vector along +ve Z axis, (0, 0, 1). */
  /* FIXME: This isn't strictly correct, as I've ignored the matrix
            elements for homogeneous coordinates. */
  /* NB: last_modelview_matrix is transposed in memory! */
  reverse_layers = (last_modelview_matrix[2][2] < 0);
  if (Settings.ShowSolderSide)
    reverse_layers = !reverse_layers;

  Settings.ShowSolderSide = reverse_layers ? !Settings.ShowSolderSide : Settings.ShowSolderSide;

  memset (do_group, 0, sizeof (do_group));
  for (ngroups = 0, i = 0; i < max_layer; i++) {
    LayerType *l;
    int group;
    int orderi;

    orderi = reverse_layers ? max_layer - i - 1 : i;

    // Draw in numerical order when in 3D view
    l = global_view_2d ? LAYER_ON_STACK (i) : LAYER_PTR (orderi);
    group = GetLayerGroupNumberByNumber (global_view_2d ? LayerStack[i] : orderi);

    if (/*l->On && */!do_group[group]) {
      do_group[group] = 1;
      drawn_groups[ngroups++] = group;
    }
  }

  /*
   * first draw all 'invisible' stuff
   */
  if (!TEST_FLAG (CHECKPLANESFLAG, PCB) &&
      gui->set_layer ("invisible", SL (INVISIBLE, 0), 0)) {
    r_search (PCB->Data->pad_tree, drawn_area, NULL, backPad_callback, NULL);
    if (PCB->ElementOn) {
      r_search (PCB->Data->element_tree, drawn_area, NULL, backE_callback, NULL);
      r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], drawn_area, NULL, backN_callback, NULL);
      DrawLayer (&(PCB->Data->BACKSILKLAYER), drawn_area);
    }
    gui->set_layer (NULL, SL (FINISHED, 0), 0);
  }

  /* draw all layers in layerstack order */
  for (i = ngroups - 1; i >= 0; i--) {
    DrawLayerGroup (drawn_groups [i], drawn_area);

#if 1
    if (!global_view_2d && i > 0) {
      cyl_info.from_layer = drawn_groups[i];
      cyl_info.to_layer = drawn_groups[i - 1];
      cyl_info.scale = gport->zoom;
//      gui->set_color (Output.fgGC, PCB->MaskColor);
      gui->set_color (Output.fgGC, "drill");
      ghid_global_alpha_mult (Output.fgGC, 0.75);
      if (PCB->PinOn) r_search (PCB->Data->pin_tree, drawn_area, NULL, hole_cyl_callback, &cyl_info);
      if (PCB->ViaOn) r_search (PCB->Data->via_tree, drawn_area, NULL, hole_cyl_callback, &cyl_info);
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

#if 1
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
#endif

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
  if (gui->set_layer ("topsilk", SL (SILK, TOP), 0)) {
    DrawSilk (0, COMPONENT_LAYER, drawn_area);
    gui->set_layer (NULL, SL (FINISHED, 0), 0);
  }

  if (gui->set_layer ("bottomsilk", SL (SILK, BOTTOM), 0)) {
    DrawSilk (1, SOLDER_LAYER, drawn_area);
    gui->set_layer (NULL, SL (FINISHED, 0), 0);
  }

  /* Draw element Marks */
  if (PCB->PinOn)
    r_search (PCB->Data->element_tree, drawn_area, NULL, EMark_callback, NULL);

  /* Draw rat lines on top */
  if (PCB->RatOn && gui->set_layer ("rats", SL (RATS, 0), 0))
    DrawRats(drawn_area);

  Gathering = True;
  Settings.ShowSolderSide = reverse_layers ? !Settings.ShowSolderSide : Settings.ShowSolderSide;
}

static int one_shot = 1;

#define Z_NEAR 3.0
gboolean
ghid_port_drawing_area_expose_event_cb (GtkWidget * widget,
					GdkEventExpose * ev, GHidPort * port)
{
  BoxType region;
  int eleft, eright, etop, ebottom;
  extern HID ghid_hid;
  GdkGLContext* pGlContext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (widget);
  int min_x, min_y;
  int max_x, max_y;
  int new_x, new_y;
  int min_depth;
  int max_depth;

  /* make GL-context "current" */
  if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext)) {
    return FALSE;
  }

  hidgl_init ();

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

//  glEnable(GL_POLYGON_SMOOTH);
//  glHint(GL_POLYGON_SMOOTH_HINT, [GL_FASTEST, GL_NICEST, or GL_DONT_CARE]);

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

  glEnable (GL_STENCIL_TEST);
  glClearColor (gport->offlimits_color.red / 65535.,
                gport->offlimits_color.green / 65535.,
                gport->offlimits_color.blue / 65535.,
                1.);

  glStencilMask (~0);
  glClearStencil (0);
  glClear (GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  hidgl_reset_stencil_usage ();

  /* Disable the stencil test until we need it - otherwise it gets dirty */
  glDisable (GL_STENCIL_TEST);
  glStencilFunc (GL_ALWAYS, 0, 0);

  /* Test the 8 corners of a cube spanning the event */
  min_depth = -50 + compute_depth (0);         /* FIXME */
  max_depth =  50 + compute_depth (max_layer); /* FIXME */

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

  glColor3f (gport->bg_color.red / 65535.,
             gport->bg_color.green / 65535.,
             gport->bg_color.blue / 65535.);

  glBegin (GL_QUADS);
  glVertex3i (eleft,  etop,    -50);
  glVertex3i (eright, etop,    -50);
  glVertex3i (eright, ebottom, -50);
  glVertex3i (eleft,  ebottom, -50);
  glEnd ();

  /* TODO: Background image */

  hidgl_init_triangle_array (&buffer);
  ghid_invalidate_current_gc ();

  /* Setup stenciling */
  /* Drawing operations set the stencil buffer to '1' */
  glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE); // Stencil pass => replace stencil value (with 1)
  /* Drawing operations as masked to areas where the stencil buffer is '0' */
//  glStencilFunc (GL_GREATER, 1, 1);             // Draw only where stencil buffer is 0

  glPushMatrix ();
  glScalef ((ghid_flip_x ? -1. : 1.) / gport->zoom,
            (ghid_flip_y ? -1. : 1.) / gport->zoom, 1);
  glTranslatef (ghid_flip_x ? gport->view_x0 - PCB->MaxWidth  :
                             -gport->view_x0,
                ghid_flip_y ? gport->view_y0 - PCB->MaxHeight :
                             -gport->view_y0, 0);

  // hid_expose_callback (&ghid_hid, &region, 0);
  ghid_draw_everything (&region);

  hidgl_flush_triangles (&buffer);
  glPopMatrix ();

  /* Just prod the drawing code so the current depth gets set to
     the right value for the layer we are editing */
  gui->set_layer (NULL, INDEXOFCURRENT, 0);
  gui->set_layer (NULL, SL_FINISHED, 0);

  draw_grid (&region);

  hidgl_init_triangle_array (&buffer);
  ghid_invalidate_current_gc ();
  glPushMatrix ();
  glScalef ((ghid_flip_x ? -1. : 1.) / gport->zoom,
            (ghid_flip_y ? -1. : 1.) / gport->zoom, 1);
  glTranslatef (ghid_flip_x ? gport->view_x0 - PCB->MaxWidth  :
                             -gport->view_x0,
                ghid_flip_y ? gport->view_y0 - PCB->MaxHeight :
                             -gport->view_y0, 0);
  DrawAttached (TRUE);
  DrawMark (TRUE);
  hidgl_flush_triangles (&buffer);
  glPopMatrix ();

  ghid_show_crosshair (TRUE);

  hidgl_flush_triangles (&buffer);


  if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
    gdk_gl_drawable_swap_buffers (pGlDrawable);
  else
    glFlush ();

  /* end drawing to current GL-context */
  gdk_gl_drawable_gl_end (pGlDrawable);

  one_shot = 0;

  return FALSE;
}

gint
ghid_port_window_motion_cb (GtkWidget * widget,
			    GdkEventButton * ev, GHidPort * out)
{
  gdouble dx, dy;
  static gint x_prev = -1, y_prev = -1;
  gboolean moved;
#if 0
  GdkGLContext* pGlContext = gtk_widget_get_gl_context (widget);
  GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (widget);

  /* make GL-context "current" */
  if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext)) {
    printf ("GL THingy returned\n");
    return FALSE;
  }
#endif

  if (out->panning)
    {
/* Not sure this really matters, since we're using invalidate to get a redraw */
#if 1
      if (gtk_events_pending ())
	return FALSE;
#endif
      dx = gport->zoom * (x_prev - ev->x);
      dy = gport->zoom * (y_prev - ev->y);
      if (x_prev > 0)
	ghid_port_ranges_pan (dx, dy, TRUE);
      x_prev = ev->x;
      y_prev = ev->y;
      return FALSE;
    }
  x_prev = y_prev = -1;
  moved = ghid_note_event_location (ev);

//  ghid_show_crosshair (TRUE);
  if (moved) // && have_crosshair_attachments ())
    ghid_draw_area_update (gport, NULL);

#if 0
  if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
    gdk_gl_drawable_swap_buffers (pGlDrawable);
  else
    glFlush ();

  /* end drawing to current GL-context */
  gdk_gl_drawable_gl_end (pGlDrawable);
#endif

  return FALSE;
}

gint
ghid_port_window_enter_cb (GtkWidget * widget,
			   GdkEventCrossing * ev, GHidPort * out)
{
  /* printf("enter: mode: %d detail: %d\n", ev->mode, ev->detail); */

  /* See comment in ghid_port_window_leave_cb() */

  if(ev->mode != GDK_CROSSING_NORMAL && ev->detail != GDK_NOTIFY_NONLINEAR) 
    {
      return FALSE;
    }

  if (!ghidgui->command_entry_status_line_active)
    {
      out->has_entered = TRUE;
      /* Make sure drawing area has keyboard focus when we are in it.
       */
      gtk_widget_grab_focus (out->drawing_area);
    }
  ghidgui->in_popup = FALSE;

  /* Following expression is true if a you open a menu from the menu bar, 
   * move the mouse to the viewport and click on it. This closes the menu 
   * and moves the pointer to the viewport without the pointer going over 
   * the edge of the viewport */
  if(ev->mode == GDK_CROSSING_UNGRAB && ev->detail == GDK_NOTIFY_NONLINEAR)
    {
      ghid_screen_update ();
    }

  if(! cursor_in_viewport)
    {
      RestoreCrosshair (TRUE);
      cursor_in_viewport = TRUE;
    }

  return FALSE;
}

static gboolean
ghid_pan_idle_cb (gpointer data)
{
  gdouble dx = 0, dy = 0;

  if (gport->has_entered)
    return FALSE;
  dy = gport->zoom * y_pan_speed;
  dx = gport->zoom * x_pan_speed;
  return (ghid_port_ranges_pan (dx, dy, TRUE));
}

gint
ghid_port_window_leave_cb (GtkWidget * widget, 
                           GdkEventCrossing * ev, GHidPort * out)
{
  gint x0, y0, x, y, dx, dy, w, h;
  
  /* printf("leave mode: %d detail: %d\n", ev->mode, ev->detail); */

  /* Window leave events can also be triggered because of focus grabs. Some
   * X applications occasionally grab the focus and so trigger this function.
   * At least GNOME's window manager is known to do this on every mouse click.
   *
   * See http://bugzilla.gnome.org/show_bug.cgi?id=102209 
   */

  if(ev->mode != GDK_CROSSING_NORMAL)
    {
      return FALSE;
    }

  if(out->has_entered && !ghidgui->in_popup)
    {
      /* if actively drawing, start scrolling */

      if (have_crosshair_attachments () && ghidgui->auto_pan_on)
	{
	  /* GdkEvent coords are set to 0,0 at leave events, so must figure
	     |  out edge the cursor left.
	   */
	  w = ghid_port.width * gport->zoom;
	  h = ghid_port.height * gport->zoom;

	  x0 = VIEW_X (0);
	  y0 = VIEW_Y (0);
	  ghid_get_coords (NULL, &x, &y);
	  x -= x0;
	  y -= y0;

	  if (ghid_flip_x )
	      x = -x;
	  if (ghid_flip_y )
	      y = -y;

	  dx = w - x;
	  dy = h - y;

	  x_pan_speed = y_pan_speed = 2 * ghidgui->auto_pan_speed;

	  if (x < dx)
	    {
	      x_pan_speed = -x_pan_speed;
	      dx = x;
	    }
	  if (y < dy)
	    {
	      y_pan_speed = -y_pan_speed;
	      dy = y;
	    }
	  if (dx < dy)
	    {
	      if (dy < h / 3)
		y_pan_speed = y_pan_speed - (3 * dy * y_pan_speed) / h;
	      else
		y_pan_speed = 0;
	    }
	  else
	    {
	      if (dx < w / 3)
		x_pan_speed = x_pan_speed - (3 * dx * x_pan_speed) / w;
	      else
		x_pan_speed = 0;
	    }
	  g_idle_add (ghid_pan_idle_cb, NULL);
	}
    }

  if(cursor_in_viewport)
    {
      HideCrosshair (TRUE);
      cursor_in_viewport = FALSE;
    }

  ghid_show_crosshair (FALSE);
  out->has_entered = FALSE;

  ghid_screen_update ();

  return FALSE;
}


  /* Mouse scroll wheel events
   */
gint
ghid_port_window_mouse_scroll_cb (GtkWidget * widget,
				  GdkEventScroll * ev, GHidPort * out)
{
  ModifierKeysState mk;
  GdkModifierType state;
  int button;

  state = (GdkModifierType) (ev->state);
  mk = ghid_modifier_keys_state (&state);

  /* X11 gtk hard codes buttons 4, 5, 6, 7 as below in
   * gtk+/gdk/x11/gdkevents-x11.c:1121, but quartz and windows have
   * special mouse scroll events, so this may conflict with a mouse
   * who has buttons 4 - 7 that aren't the scroll wheel?
   */
  switch(ev->direction)
    {
    case GDK_SCROLL_UP: button = 4; break;
    case GDK_SCROLL_DOWN: button = 5; break;
    case GDK_SCROLL_LEFT: button = 6; break;
    case GDK_SCROLL_RIGHT: button = 7; break;
    default: button = -1;
    }

  do_mouse_action(button, mk);

  return TRUE;
}
