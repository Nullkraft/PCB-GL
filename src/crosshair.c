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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Contact addresses for paper mail and Email:
 *  Thomas Nau, Schlehenweg 15, 88471 Baustetten, Germany
 *  Thomas.Nau@rz.uni-ulm.de
 *
 */


/* crosshair stuff
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory.h>
#include <math.h>

#include "global.h"
#include "hid_draw.h"

#include "crosshair.h"
#include "data.h"
#include "draw.h"
#include "error.h"
#include "line.h"
#include "misc.h"
#include "mymem.h"
#include "search.h"
#include "polygon.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif


static bool
make_arc_from_crosshair (ArcType *arc, Coord thick)
{
  Coord wx, wy;
  Angle sa, dir;

  wx = Crosshair.X - Crosshair.AttachedBox.Point1.X;
  wy = Crosshair.Y - Crosshair.AttachedBox.Point1.Y;

  if (wx == 0 && wy == 0)
    return false;

  memset (arc, 0, sizeof (*arc));

  arc->X = Crosshair.AttachedBox.Point1.X;
  arc->Y = Crosshair.AttachedBox.Point1.Y;

  if (XOR (Crosshair.AttachedBox.otherway, abs (wy) > abs (wx)))
    {
      arc->X = Crosshair.AttachedBox.Point1.X + abs (wy) * SGNZ (wx);
      sa = (wx >= 0) ? 0 : 180;
#ifdef ARC45
      if (abs (wy) >= 2 * abs (wx))
        dir = (SGNZ (wx) == SGNZ (wy)) ? 45 : -45;
      else
#endif
        dir = (SGNZ (wx) == SGNZ (wy)) ? 90 : -90;
    }
  else
    {
      arc->Y = Crosshair.AttachedBox.Point1.Y + abs (wx) * SGNZ (wy);
      sa = (wy >= 0) ? -90 : 90;
#ifdef ARC45
      if (abs (wx) >= 2 * abs (wy))
        dir = (SGNZ (wx) == SGNZ (wy)) ? -45 : 45;
      else
#endif
        dir = (SGNZ (wx) == SGNZ (wy)) ? -90 : 90;
      wy = wx;
    }

  wy = abs (wy);
  arc->StartAngle = sa;
  arc->Delta = dir;
  arc->Width = arc->Height = wy;
  arc->Thickness = thick;

  return true;
}

/* ---------------------------------------------------------------------------
 * draws the rubberband to insert points into polygons/lines/...
 */
static void
XORDrawInsertPointObject (DrawAPI *dapi)
{
  LineType *line;
  PointType *point;

  line  = (LineType  *)Crosshair.AttachedObject.Ptr2;
  point = (PointType *)Crosshair.AttachedObject.Ptr3;

  if (Crosshair.AttachedObject.Type == NO_TYPE)
    return;

  dapi->graphics->draw_line (dapi->gc, point->X, point->Y, line->Point1.X, line->Point1.Y);
  dapi->graphics->draw_line (dapi->gc, point->X, point->Y, line->Point2.X, line->Point2.Y);
}

/* ---------------------------------------------------------------------------
 * draws the attached object while in MOVE_MODE or COPY_MODE
 */
static void
draw_move_or_copy_object (DrawAPI *dapi)
{
  RubberbandType *ptr;
  Cardinal i;
  Coord dx, dy;

  dx = Crosshair.X - Crosshair.AttachedObject.X;
  dy = Crosshair.Y - Crosshair.AttachedObject.Y;

  /* NB: We don't reset this.. other rendering routines are expected to do so before
   *     they draw, so we will have to make some distinction as to which routines may
   *     set this (and not call them from inside one another!)
   */
  // dapi->set_draw_offset (dapi, dx, dy);

  switch (Crosshair.AttachedObject.Type)
    {
    case VIA_TYPE:
      {
        PinType *via = (PinType *)Crosshair.AttachedObject.Ptr1;
        dapi->set_draw_offset (dapi, dx, dy);
        dapi->draw_pcb_via (dapi, via);
        break;
      }

    case LINE_TYPE:
      {
        LineType *line = (LineType *)Crosshair.AttachedObject.Ptr2;
        dapi->set_draw_offset (dapi, dx, dy);
        dapi->draw_pcb_line (dapi, NULL, line);
        break;
      }

    case ARC_TYPE:
      {
        ArcType *arc = (ArcType *)Crosshair.AttachedObject.Ptr2;
        dapi->set_draw_offset (dapi, dx, dy);
        dapi->draw_pcb_arc (dapi, NULL, arc);
        break;
      }

    case POLYGON_TYPE:
      {
        PolygonType *poly = (PolygonType *)Crosshair.AttachedObject.Ptr2;
        dapi->set_draw_offset (dapi, dx, dy);
        dapi->draw_pcb_polygon (dapi, NULL, poly);
        break;
      }

    case LINEPOINT_TYPE:
      {
        PointType *point   = (PointType *) Crosshair.AttachedObject.Ptr3;
        LineType *old_line = (LineType  *) Crosshair.AttachedObject.Ptr2;
        LineType draw_line = *old_line;

        if (point == &old_line->Point1)
          {
            draw_line.Point1.X += dx;
            draw_line.Point1.Y += dy;
          }
        else
          {
            draw_line.Point2.X += dx;
            draw_line.Point2.Y += dy;
          }

        dapi->draw_pcb_line (dapi, NULL, &draw_line);
        break;
      }

    case POLYGONPOINT_TYPE:
      {
        PolygonType *polygon;
        PointType *point;
        Cardinal point_idx, prev, next;

        polygon = (PolygonType *) Crosshair.AttachedObject.Ptr2;
        point = (PointType *) Crosshair.AttachedObject.Ptr3;
        point_idx = polygon_point_idx (polygon, point);

        /* get previous and following point */
        prev = prev_contour_point (polygon, point_idx);
        next = next_contour_point (polygon, point_idx);

        /* XXX: Could do this by adjusting a copy polygon and drawing the entirity of that */

        /* draw the two segments */
        dapi->graphics->draw_line (dapi->gc,
                                   polygon->Points[prev].X, polygon->Points[prev].Y,
                                   point->X + dx, point->Y + dy);
        dapi->graphics->draw_line (dapi->gc,
                                   point->X + dx, point->Y + dy,
                                   polygon->Points[next].X, polygon->Points[next].Y);
        break;
      }

    case ELEMENTNAME_TYPE:
      {
        /* locate the element "mark" and draw an association line from crosshair to it */
        ElementType *element = (ElementType *) Crosshair.AttachedObject.Ptr1;

        dapi->graphics->draw_line (dapi->gc, element->MarkX, element->MarkY, Crosshair.X, Crosshair.Y);
        /* fall through to move the text as a box outline */
      }
    case TEXT_TYPE:
      {
        TextType *text = (TextType *) Crosshair.AttachedObject.Ptr2;
        BoxType *box = &text->BoundingBox;

        dapi->set_draw_offset (dapi, dx, dy);
        /* XXX: DOES THIS WORK IN CONJUNCTION WITH THE ABOVE? */
        dapi->graphics->draw_rect (dapi->gc, box->X1, box->Y1, box->X2, box->Y2);
        break;
      }

      /* pin/pad movements result in moving an element */
    case PAD_TYPE:
    case PIN_TYPE:
    case ELEMENT_TYPE:
      {
        ElementType *element = (ElementType *) Crosshair.AttachedObject.Ptr2;

        dapi->set_draw_offset (dapi, dx, dy);
        dapi->draw_pcb_element (dapi, element);
        break;
      }
    }

  /* draw the attached rubberband lines too */
  i = Crosshair.AttachedObject.RubberbandN;
  ptr = Crosshair.AttachedObject.Rubberband;
  while (i)
    {
      /* If this rat going to a polygon, do not draw for rubberband */
      if (TEST_FLAG (VIAFLAG, ptr->Line))
        continue;

      if (TEST_FLAG (RUBBERENDFLAG, ptr->Line))
        {
          LineType draw_line = *ptr->Line;

          if (ptr->MovedPoint == &ptr->Line->Point1)
            {
              draw_line.Point1.X += dx;
              draw_line.Point1.Y += dy;
            }
          else
            {
              draw_line.Point2.X += dx;
              draw_line.Point2.Y += dy;
            }

          dapi->draw_pcb_line (dapi, NULL, &draw_line);
        }
      else if (ptr->MovedPoint == &ptr->Line->Point1) /* XXX: What is this conditional for ?? */
        {
          dapi->set_draw_offset (dapi, dx, dy);
          dapi->draw_pcb_line (dapi, NULL, ptr->Line);
        }

      ptr++;
      i--;
    }
}

/* ---------------------------------------------------------------------------
 * draws additional stuff that follows the crosshair
 */
void
DrawAttached (DrawAPI *dapi)
{
  dapi->gc = dapi->graphics->make_gc ();

  dapi->graphics->set_color (dapi->gc, Settings.CrosshairColor);
//  dapi->graphics->set_draw_xor (dapi->gc, 1);
  dapi->graphics->set_line_cap (dapi->gc, Trace_Cap);
  dapi->graphics->set_line_width (dapi->gc, 1);

  switch (Settings.Mode)
    {
    case VIA_MODE:
      {
        /* Make a dummy via structure to draw from */
        PinType via;
        via.X = Crosshair.X;
        via.Y = Crosshair.Y;
        via.Thickness = Settings.ViaThickness;
        via.Clearance = 2 * Settings.Keepaway;
        via.DrillingHole = Settings.ViaDrillingHole;
        via.Mask = 0;
        via.Flags = NoFlags ();

        dapi->draw_pcb_via (dapi, &via);

        if (TEST_FLAG (SHOWDRCFLAG, PCB))
          {
            /* XXX: Naughty cheat - use the mask to draw DRC clearance! */
            via.Mask = Settings.ViaThickness + PCB->Bloat * 2;
            dapi->graphics->set_color (dapi->gc, Settings.CrossColor);
            dapi->draw_pcb_via_mask (dapi, &via);
            dapi->graphics->set_color (dapi->gc, Settings.CrosshairColor);
          }
        break;
      }

      /* the attached line is used by both LINEMODE, POLYGON_MODE and POLYGONHOLE_MODE*/
    case POLYGON_MODE:
    case POLYGONHOLE_MODE:
      /* draw only if starting point is set */
      if (Crosshair.AttachedLine.State != STATE_FIRST)
        dapi->graphics->draw_line (dapi->gc,
                                   Crosshair.AttachedLine.Point1.X, Crosshair.AttachedLine.Point1.Y,
                                   Crosshair.AttachedLine.Point2.X, Crosshair.AttachedLine.Point2.Y);

      /* draw attached polygon only if in POLYGON_MODE or POLYGONHOLE_MODE */
      if (Crosshair.AttachedPolygon.PointN > 1)
        dapi->draw_pcb_polygon (dapi, NULL, &Crosshair.AttachedPolygon);
      break;

    case ARC_MODE:
      {
        ArcType arc;

        if (Crosshair.AttachedBox.State == STATE_FIRST)
          break;

        if (!make_arc_from_crosshair (&arc, Settings.LineThickness))
          break;

        dapi->draw_pcb_arc (dapi, NULL, &arc);
        if (TEST_FLAG (SHOWDRCFLAG, PCB))
          {
            if (!make_arc_from_crosshair (&arc, Settings.LineThickness + 2 * (PCB->Bloat + 1)))
              break;
            dapi->graphics->set_color (dapi->gc, Settings.CrossColor);
            dapi->draw_pcb_arc (dapi, NULL, &arc);
            dapi->graphics->set_color (dapi->gc, Settings.CrosshairColor);
          }

        break;
      }

    case LINE_MODE:
      /* draw only if starting point exists and the line has length */
      if (Crosshair.AttachedLine.State != STATE_FIRST &&
          Crosshair.AttachedLine.draw)
        {
          /* Make a dummy line structure to draw from */
          LineType draw_line;

          draw_line.Point1 = Crosshair.AttachedLine.Point1;
          draw_line.Point2 = Crosshair.AttachedLine.Point2;
          draw_line.Thickness = Settings.LineThickness;
          draw_line.Clearance = 2 * Settings.Keepaway;
          draw_line.Flags = NoFlags ();

          draw_line.Thickness = PCB->RatDraw ? 10 : Settings.LineThickness;
          dapi->draw_pcb_line (dapi, NULL, &draw_line);

          if (PCB->Clipping)
            {
              draw_line.Point1.X = Crosshair.X;
              draw_line.Point1.Y = Crosshair.Y;
              dapi->draw_pcb_line (dapi, NULL, &draw_line);
            }

          if (TEST_FLAG (SHOWDRCFLAG, PCB))
            {
              dapi->graphics->set_color (dapi->gc, Settings.CrossColor);

              draw_line.Point1 = Crosshair.AttachedLine.Point1;
              draw_line.Point2 = Crosshair.AttachedLine.Point2;
              draw_line.Thickness = PCB->RatDraw ? 10 : Settings.LineThickness + 2 * (PCB->Bloat + 1);
              dapi->draw_pcb_line (dapi, NULL, &draw_line);

              if (PCB->Clipping)
                {
                  draw_line.Point1.X = Crosshair.X;
                  draw_line.Point1.Y = Crosshair.Y;
                  dapi->draw_pcb_line (dapi, NULL, &draw_line);
                }

              dapi->graphics->set_color (dapi->gc, Settings.CrosshairColor);
            }
        }
      break;

    case PASTEBUFFER_MODE:
      /* NB: We don't reset this.. other rendering routines are expected to do so before
       *     they draw, so we will have to make some distinction as to which routines may
       *     set this (and not call them from inside one another!)
       */
      dapi->set_draw_offset (dapi, Crosshair.X - PASTEBUFFER->X,
                             Crosshair.Y - PASTEBUFFER->Y);
      dapi->draw_pcb_buffer (dapi, PASTEBUFFER);
      break;

    case COPY_MODE:
    case MOVE_MODE:
      draw_move_or_copy_object (dapi);
      break;

    case INSERTPOINT_MODE:
      XORDrawInsertPointObject (dapi);
      break;
    }

  /* an attached box does not depend on a special mode */
  if (Crosshair.AttachedBox.State == STATE_SECOND ||
      Crosshair.AttachedBox.State == STATE_THIRD)
    {
      Coord x1, y1, x2, y2;

      x1 = Crosshair.AttachedBox.Point1.X;
      y1 = Crosshair.AttachedBox.Point1.Y;
      x2 = Crosshair.AttachedBox.Point2.X;
      y2 = Crosshair.AttachedBox.Point2.Y;
      dapi->graphics->draw_rect (dapi->gc, x1, y1, x2, y2);
    }

  dapi->graphics->destroy_gc (dapi->gc);
}


/* --------------------------------------------------------------------------
 * draw the marker position
 */
void
DrawMark (DrawAPI *dapi)
{
  /* Mark is not drawn when it is not set */
  if (!Marked.status)
    return;

  dapi->gc = dapi->graphics->make_gc ();

  dapi->graphics->set_color (dapi->gc, Settings.CrosshairColor);
//  dapi->graphics->set_draw_xor (dapi->gc, 1);
  dapi->graphics->set_line_cap (dapi->gc, Trace_Cap);
  dapi->graphics->set_line_width (dapi->gc, 1);

  dapi->graphics->draw_line (dapi->gc, Marked.X - MARK_SIZE, Marked.Y - MARK_SIZE,
                                       Marked.X + MARK_SIZE, Marked.Y + MARK_SIZE);
  dapi->graphics->draw_line (dapi->gc, Marked.X + MARK_SIZE, Marked.Y - MARK_SIZE,
                                       Marked.X - MARK_SIZE, Marked.Y + MARK_SIZE);
  dapi->graphics->destroy_gc (dapi->gc);

}

/* ---------------------------------------------------------------------------
 * Returns the nearest grid-point to the given Coord
 */
Coord
GridFit (Coord x, Coord grid_spacing, Coord grid_offset)
{
  x -= grid_offset;
  x = grid_spacing * round ((double) x / grid_spacing);
  x += grid_offset;
  return x;
}


/* ---------------------------------------------------------------------------
 * notify the GUI that data relating to the crosshair is being changed.
 *
 * The argument passed is false to notify "changes are about to happen",
 * and true to notify "changes have finished".
 *
 * Each call with a 'false' parameter must be matched with a following one
 * with a 'true' parameter. Unmatched 'true' calls are currently not permitted,
 * but might be allowed in the future.
 *
 * GUIs should not complain if they receive extra calls with 'true' as parameter.
 * They should initiate a redraw of the crosshair attached objects - which may
 * (if necessary) mean repainting the whole screen if the GUI hasn't tracked the
 * location of existing attached drawing.
 */
void
notify_crosshair_change (bool changes_complete)
{
  if (gui->notify_crosshair_change)
    gui->notify_crosshair_change (changes_complete);
}


/* ---------------------------------------------------------------------------
 * notify the GUI that data relating to the mark is being changed.
 *
 * The argument passed is false to notify "changes are about to happen",
 * and true to notify "changes have finished".
 *
 * Each call with a 'false' parameter must be matched with a following one
 * with a 'true' parameter. Unmatched 'true' calls are currently not permitted,
 * but might be allowed in the future.
 *
 * GUIs should not complain if they receive extra calls with 'true' as parameter.
 * They should initiate a redraw of the mark - which may (if necessary) mean
 * repainting the whole screen if the GUI hasn't tracked the mark's location.
 */
void
notify_mark_change (bool changes_complete)
{
  if (gui->notify_mark_change)
    gui->notify_mark_change (changes_complete);
}


/* ---------------------------------------------------------------------------
 * Convenience for plugins using the old {Hide,Restore}Crosshair API.
 * This links up to notify the GUI of the expected changes using the new APIs.
 *
 * Use of this old API is deprecated, as the names don't necessarily reflect
 * what all GUIs may do in response to the notifications. Keeping these APIs
 * is aimed at easing transition to the newer API, they will emit a harmless
 * warning at the time of their first use.
 *
 */
void
HideCrosshair (void)
{
  static bool warned_old_api = false;
  if (!warned_old_api)
    {
      Message (_("WARNING: A plugin is using the deprecated API HideCrosshair().\n"
                 "         This API may be removed in a future release of PCB.\n"));
      warned_old_api = true;
    }

  notify_crosshair_change (false);
  notify_mark_change (false);
}

void
RestoreCrosshair (void)
{
  static bool warned_old_api = false;
  if (!warned_old_api)
    {
      Message (_("WARNING: A plugin is using the deprecated API RestoreCrosshair().\n"
                 "         This API may be removed in a future release of PCB.\n"));
      warned_old_api = true;
    }

  notify_crosshair_change (true);
  notify_mark_change (true);
}

/* ---------------------------------------------------------------------------
 * Returns the square of the given number
 */
static double
square (double x)
{
  return x * x;
}

static double
crosshair_sq_dist (CrosshairType *crosshair, Coord x, Coord y)
{
  return square (x - crosshair->X) + square (y - crosshair->Y);
}

struct snap_data {
  CrosshairType *crosshair;
  double nearest_sq_dist;
  bool nearest_is_grid;
  Coord x, y;
};

/* Snap to a given location if it is the closest thing we found so far.
 * If "prefer_to_grid" is set, the passed location will take preference
 * over a closer grid points we already snapped to UNLESS the user is
 * pressing the SHIFT key. If the SHIFT key is pressed, the closest object
 * (including grid points), is always preferred.
 */
static void
check_snap_object (struct snap_data *snap_data, Coord x, Coord y,
                   bool prefer_to_grid)
{
  double sq_dist;

  sq_dist = crosshair_sq_dist (snap_data->crosshair, x, y);
  if (sq_dist <= snap_data->nearest_sq_dist ||
      (prefer_to_grid && snap_data->nearest_is_grid && !gui->shift_is_pressed()))
    {
      snap_data->x = x;
      snap_data->y = y;
      snap_data->nearest_sq_dist = sq_dist;
      snap_data->nearest_is_grid = false;
    }
}

static void
check_snap_offgrid_line (struct snap_data *snap_data,
                         Coord nearest_grid_x,
                         Coord nearest_grid_y)
{
  void *ptr1, *ptr2, *ptr3;
  int ans;
  LineType *line;
  Coord try_x, try_y;
  double dx, dy;
  double dist;

  if (!TEST_FLAG (SNAPPINFLAG, PCB))
    return;

  /* Code to snap at some sensible point along a line */
  /* Pick the nearest grid-point in the x or y direction
   * to align with, then adjust until we hit the line
   */
  ans = SearchScreenGridSlop (Crosshair.X, Crosshair.Y,
                              LINE_TYPE, &ptr1, &ptr2, &ptr3);

  if (ans == NO_TYPE)
    return;

  line = (LineType *)ptr2;

  /* Allow snapping to off-grid lines when drawing new lines (on
   * the same layer), and when moving a line end-point
   * (but don't snap to the same line)
   */
  if ((Settings.Mode != LINE_MODE || CURRENT != ptr1) &&
      (Settings.Mode != MOVE_MODE ||
       Crosshair.AttachedObject.Ptr1 != ptr1 ||
       Crosshair.AttachedObject.Type != LINEPOINT_TYPE ||
       Crosshair.AttachedObject.Ptr2 == line))
    return;

  dx = line->Point2.X - line->Point1.X;
  dy = line->Point2.Y - line->Point1.Y;

  /* Try snapping along the X axis */
  if (dy != 0.)
    {
      /* Move in the X direction until we hit the line */
      try_x = (nearest_grid_y - line->Point1.Y) / dy * dx + line->Point1.X;
      try_y = nearest_grid_y;
      check_snap_object (snap_data, try_x, try_y, true);
    }

  /* Try snapping along the Y axis */
  if (dx != 0.)
    {
      try_x = nearest_grid_x;
      try_y = (nearest_grid_x - line->Point1.X) / dx * dy + line->Point1.Y;
      check_snap_object (snap_data, try_x, try_y, true);
    }

  if (dx != dy) /* If line not parallel with dX = dY direction.. */
    {
      /* Try snapping diagonally towards the line in the dX = dY direction */

      if (dy == 0)
        dist = line->Point1.Y - nearest_grid_y;
      else
        dist = ((line->Point1.X - nearest_grid_x) -
                (line->Point1.Y - nearest_grid_y) * dx / dy) / (1 - dx / dy);

      try_x = nearest_grid_x + dist;
      try_y = nearest_grid_y + dist;

      check_snap_object (snap_data, try_x, try_y, true);
    }

  if (dx != -dy) /* If line not parallel with dX = -dY direction.. */
    {
      /* Try snapping diagonally towards the line in the dX = -dY direction */

      if (dy == 0)
        dist = nearest_grid_y - line->Point1.Y;
      else
        dist = ((line->Point1.X - nearest_grid_x) -
                (line->Point1.Y - nearest_grid_y) * dx / dy) / (1 + dx / dy);

      try_x = nearest_grid_x + dist;
      try_y = nearest_grid_y - dist;

      check_snap_object (snap_data, try_x, try_y, true);
    }
}

/* ---------------------------------------------------------------------------
 * recalculates the passed coordinates to fit the current grid setting
 */
void
FitCrosshairIntoGrid (Coord X, Coord Y)
{
  Coord nearest_grid_x, nearest_grid_y;
  void *ptr1, *ptr2, *ptr3;
  struct snap_data snap_data;
  int ans;

  Crosshair.X = CLAMP (X, Crosshair.MinX, Crosshair.MaxX);
  Crosshair.Y = CLAMP (Y, Crosshair.MinY, Crosshair.MaxY);

  if (PCB->RatDraw)
    {
      nearest_grid_x = -MIL_TO_COORD (6);
      nearest_grid_y = -MIL_TO_COORD (6);
    }
  else
    {
      nearest_grid_x = GridFit (Crosshair.X, PCB->Grid, PCB->GridOffsetX);
      nearest_grid_y = GridFit (Crosshair.Y, PCB->Grid, PCB->GridOffsetY);

      if (Marked.status && TEST_FLAG (ORTHOMOVEFLAG, PCB))
	{
	  Coord dx = Crosshair.X - Marked.X;
	  Coord dy = Crosshair.Y - Marked.Y;
	  if (ABS (dx) > ABS (dy))
	    nearest_grid_y = Marked.Y;
	  else
	    nearest_grid_x = Marked.X;
	}

    }

  snap_data.crosshair = &Crosshair;
  snap_data.nearest_sq_dist =
    crosshair_sq_dist (&Crosshair, nearest_grid_x, nearest_grid_y);
  snap_data.nearest_is_grid = true;
  snap_data.x = nearest_grid_x;
  snap_data.y = nearest_grid_y;

  ans = NO_TYPE;
  if (!PCB->RatDraw)
    ans = SearchScreenGridSlop (Crosshair.X, Crosshair.Y,
                                ELEMENT_TYPE, &ptr1, &ptr2, &ptr3);

  if (ans & ELEMENT_TYPE)
    {
      ElementType *el = (ElementType *) ptr1;
      check_snap_object (&snap_data, el->MarkX, el->MarkY, false);
    }

  ans = NO_TYPE;
  if (PCB->RatDraw || TEST_FLAG (SNAPPINFLAG, PCB))
    ans = SearchScreenGridSlop (Crosshair.X, Crosshair.Y,
                                PAD_TYPE, &ptr1, &ptr2, &ptr3);

  /* Avoid self-snapping when moving */
  if (ans != NO_TYPE &&
      Settings.Mode == MOVE_MODE &&
      Crosshair.AttachedObject.Type == ELEMENT_TYPE &&
      ptr1 == Crosshair.AttachedObject.Ptr1)
    ans = NO_TYPE;

  if (ans != NO_TYPE &&
      ( Settings.Mode == LINE_MODE ||
       (Settings.Mode == MOVE_MODE &&
        Crosshair.AttachedObject.Type == LINEPOINT_TYPE)))
    {
      PadType *pad = (PadType *) ptr2;
      LayerType *desired_layer;
      Cardinal desired_group;
      Cardinal SLayer, CLayer;
      int found_our_layer = false;

      desired_layer = CURRENT;
      if (Settings.Mode == MOVE_MODE &&
          Crosshair.AttachedObject.Type == LINEPOINT_TYPE)
        {
          desired_layer = (LayerType *)Crosshair.AttachedObject.Ptr1;
        }

      /* find layer groups of the component side and solder side */
      SLayer = GetLayerGroupNumberByNumber (solder_silk_layer);
      CLayer = GetLayerGroupNumberByNumber (component_silk_layer);
      desired_group = TEST_FLAG (ONSOLDERFLAG, pad) ? SLayer : CLayer;

      GROUP_LOOP (PCB->Data, desired_group);
      {
        if (layer == desired_layer)
          {
            found_our_layer = true;
            break;
          }
      }
      END_LOOP;

      if (found_our_layer == false)
        ans = NO_TYPE;
    }

  if (ans != NO_TYPE)
    {
      PadType *pad = (PadType *)ptr2;
      check_snap_object (&snap_data, pad->Point1.X + (pad->Point2.X - pad->Point1.X) / 2,
                                     pad->Point1.Y + (pad->Point2.Y - pad->Point1.Y) / 2,
                         true);
    }

  ans = NO_TYPE;
  if (PCB->RatDraw || TEST_FLAG (SNAPPINFLAG, PCB))
    ans = SearchScreenGridSlop (Crosshair.X, Crosshair.Y,
                                PIN_TYPE, &ptr1, &ptr2, &ptr3);

  /* Avoid self-snapping when moving */
  if (ans != NO_TYPE &&
      Settings.Mode == MOVE_MODE &&
      Crosshair.AttachedObject.Type == ELEMENT_TYPE &&
      ptr1 == Crosshair.AttachedObject.Ptr1)
    ans = NO_TYPE;

  if (ans != NO_TYPE)
    {
      PinType *pin = (PinType *)ptr2;
      check_snap_object (&snap_data, pin->X, pin->Y, true);
    }

  ans = NO_TYPE;
  if (TEST_FLAG (SNAPPINFLAG, PCB))
    ans = SearchScreenGridSlop (Crosshair.X, Crosshair.Y,
                                VIA_TYPE, &ptr1, &ptr2, &ptr3);

  /* Avoid snapping vias to any other vias */
  if (Settings.Mode == MOVE_MODE &&
      Crosshair.AttachedObject.Type == VIA_TYPE &&
      (ans & PIN_TYPES))
    ans = NO_TYPE;

  if (ans != NO_TYPE)
    {
      PinType *pin = (PinType *)ptr2;
      check_snap_object (&snap_data, pin->X, pin->Y, true);
    }

  ans = NO_TYPE;
  if (TEST_FLAG (SNAPPINFLAG, PCB))
    ans = SearchScreenGridSlop (Crosshair.X, Crosshair.Y,
                                LINEPOINT_TYPE | ARCPOINT_TYPE,
                                &ptr1, &ptr2, &ptr3);

  if (ans != NO_TYPE)
    {
      PointType *pnt = (PointType *)ptr3;
      check_snap_object (&snap_data, pnt->X, pnt->Y, true);
    }

  check_snap_offgrid_line (&snap_data, nearest_grid_x, nearest_grid_y);

  ans = NO_TYPE;
  if (TEST_FLAG (SNAPPINFLAG, PCB))
    ans = SearchScreenGridSlop (Crosshair.X, Crosshair.Y,
                                POLYGONPOINT_TYPE, &ptr1, &ptr2, &ptr3);

  if (ans != NO_TYPE)
    {
      PointType *pnt = (PointType *)ptr3;
      check_snap_object (&snap_data, pnt->X, pnt->Y, true);
    }

  if (snap_data.x >= 0 && snap_data.y >= 0)
    {
      Crosshair.X = snap_data.x;
      Crosshair.Y = snap_data.y;
    }

  if (Settings.Mode == ARROW_MODE)
    {
      ans = SearchScreenGridSlop (Crosshair.X, Crosshair.Y,
                                  LINEPOINT_TYPE | ARCPOINT_TYPE,
                                  &ptr1, &ptr2, &ptr3);
      if (ans == NO_TYPE)
        hid_action("PointCursor");
      else if (!TEST_FLAG(SELECTEDFLAG, (LineType *)ptr2))
        hid_actionl("PointCursor","True", NULL);
    }

  if (Settings.Mode == LINE_MODE
      && Crosshair.AttachedLine.State != STATE_FIRST
      && TEST_FLAG (AUTODRCFLAG, PCB))
    EnforceLineDRC ();

  gui->set_crosshair (Crosshair.X, Crosshair.Y, HID_SC_DO_NOTHING);
}

/* ---------------------------------------------------------------------------
 * move crosshair relative (has to be switched off)
 */
void
MoveCrosshairRelative (Coord DeltaX, Coord DeltaY)
{
  FitCrosshairIntoGrid (Crosshair.X + DeltaX, Crosshair.Y + DeltaY);
}

/* ---------------------------------------------------------------------------
 * move crosshair absolute
 * return true if the crosshair was moved from its existing position
 */
bool
MoveCrosshairAbsolute (Coord X, Coord Y)
{
  Coord x, y, z;
  x = Crosshair.X;
  y = Crosshair.Y;
  FitCrosshairIntoGrid (X, Y);
  if (Crosshair.X != x || Crosshair.Y != y)
    {
      /* back up to old position to notify the GUI
       * (which might want to erase the old crosshair) */
      z = Crosshair.X;
      Crosshair.X = x;
      x = z;
      z = Crosshair.Y;
      Crosshair.Y = y;
      notify_crosshair_change (false); /* Our caller notifies when it has done */
      /* now move forward again */
      Crosshair.X = x;
      Crosshair.Y = z;
      return (true);
    }
  return (false);
}

/* ---------------------------------------------------------------------------
 * sets the valid range for the crosshair cursor
 */
void
SetCrosshairRange (Coord MinX, Coord MinY, Coord MaxX, Coord MaxY)
{
  Crosshair.MinX = MAX (0, MinX);
  Crosshair.MinY = MAX (0, MinY);
  Crosshair.MaxX = MIN (PCB->MaxWidth, MaxX);
  Crosshair.MaxY = MIN (PCB->MaxHeight, MaxY);

  /* force update of position */
  MoveCrosshairRelative (0, 0);
}

/* ---------------------------------------------------------------------------
 * initializes crosshair stuff
 * clears the struct, allocates to graphical contexts
 */
void
InitCrosshair (void)
{
  /* set initial shape */
  Crosshair.shape = Basic_Crosshair_Shape;

  /* set default limits */
  Crosshair.MinX = Crosshair.MinY = 0;
  Crosshair.MaxX = PCB->MaxWidth;
  Crosshair.MaxY = PCB->MaxHeight;

  /* clear the mark */
  Marked.status = false;
}

/* ---------------------------------------------------------------------------
 * exits crosshair routines, release GCs
 */
void
DestroyCrosshair (void)
{
  FreePolygonMemory (&Crosshair.AttachedPolygon);
}
