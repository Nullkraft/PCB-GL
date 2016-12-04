/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996 Thomas Nau
 *  Copyright (C) 2004 harry eaton
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <setjmp.h>
#include <stdlib.h>


#include "global.h"
#include "data.h"
#include "crosshair.h"
#include "find.h"
#include "line.h"
#include "misc.h"
#include "rtree.h"
#include "netclass.h"
#include "draw.h" /* For Redraw */

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

/* ---------------------------------------------------------------------------
 * Adjust the attached line to 45 degrees if necessary
 */
void
AdjustAttachedLine (void)
{
  AttachedLineType *line = &Crosshair.AttachedLine;

  printf ("AdjustAttachedLine\n");

  /* I need at least one point */
  if (line->State == STATE_FIRST)
    return;
  /* don't draw outline when ctrl key is pressed */
  if (Settings.Mode == LINE_MODE && gui->control_is_pressed ())
    {
      line->draw = false;
      return;
    }
  else
    line->draw = true;
  /* no 45 degree lines required */
  if (PCB->RatDraw || TEST_FLAG (ALLDIRECTIONFLAG, PCB))
    {
      line->Point2.X = Crosshair.X;
      line->Point2.Y = Crosshair.Y;
      return;
    }
  FortyFiveLine (line);
}

/* ---------------------------------------------------------------------------
 * makes the attached line fit into a 45 degree direction
 *
 * directions:
 *
 *           4
 *          5 3
 *         6   2
 *          7 1
 *           0
 */
void
FortyFiveLine (AttachedLineType *Line)
{
  Coord dx, dy, min, max;
  unsigned direction = 0;
  double m;

  /* first calculate direction of line */
  dx = Crosshair.X - Line->Point1.X;
  dy = Crosshair.Y - Line->Point1.Y;

  /* zero length line, don't draw anything */
  if (dx == 0 && dy == 0)
    return;

  if (dx == 0)
    direction = dy > 0 ? 0 : 4;
  else
    {
      m = (double)dy / (double)dx;
      direction = 2;
      if (m > TAN_30_DEGREE)
        direction = m > TAN_60_DEGREE ? 0 : 1;
      else if (m < -TAN_30_DEGREE)
        direction = m < -TAN_60_DEGREE ? 4 : 3;
    }
  if (dx < 0)
    direction += 4;

  dx = abs (dx);
  dy = abs (dy);
  min = MIN (dx, dy);
  max = MAX (dx, dy);

  /* now set up the second pair of coordinates */
  switch (direction)
    {
    case 0:
      Line->Point2.X = Line->Point1.X;
      Line->Point2.Y = Line->Point1.Y + max;
      break;

    case 4:
      Line->Point2.X = Line->Point1.X;
      Line->Point2.Y = Line->Point1.Y - max;
      break;

    case 2:
      Line->Point2.X = Line->Point1.X + max;
      Line->Point2.Y = Line->Point1.Y;
      break;

    case 6:
      Line->Point2.X = Line->Point1.X - max;
      Line->Point2.Y = Line->Point1.Y;
      break;

    case 1:
      Line->Point2.X = Line->Point1.X + min;
      Line->Point2.Y = Line->Point1.Y + min;
      break;

    case 3:
      Line->Point2.X = Line->Point1.X + min;
      Line->Point2.Y = Line->Point1.Y - min;
      break;

    case 5:
      Line->Point2.X = Line->Point1.X - min;
      Line->Point2.Y = Line->Point1.Y - min;
      break;

    case 7:
      Line->Point2.X = Line->Point1.X - min;
      Line->Point2.Y = Line->Point1.Y + min;
      break;
    }
}

/* ---------------------------------------------------------------------------
 *  adjusts the insert lines to make them 45 degrees as necessary
 */
void
AdjustTwoLine (bool way)
{
  Coord dx, dy;
  AttachedLineType *line = &Crosshair.AttachedLine;

  if (Crosshair.AttachedLine.State == STATE_FIRST)
    return;
  /* don't draw outline when ctrl key is pressed */
  if (gui->control_is_pressed ())
    {
      line->draw = false;
      return;
    }
  else
    line->draw = true;
  if (TEST_FLAG (ALLDIRECTIONFLAG, PCB))
    {
      line->Point2.X = Crosshair.X;
      line->Point2.Y = Crosshair.Y;
      return;
    }
  /* swap the modes if shift is held down */
  if (gui->shift_is_pressed ())
    way = !way;
  dx = Crosshair.X - line->Point1.X;
  dy = Crosshair.Y - line->Point1.Y;
  if (!way)
    {
      if (abs (dx) > abs (dy))
	{
	  line->Point2.X = Crosshair.X - SGN (dx) * abs (dy);
	  line->Point2.Y = line->Point1.Y;
	}
      else
	{
	  line->Point2.X = line->Point1.X;
	  line->Point2.Y = Crosshair.Y - SGN (dy) * abs (dx);
	}
    }
  else
    {
      if (abs (dx) > abs (dy))
	{
	  line->Point2.X = line->Point1.X + SGN (dx) * abs (dy);
	  line->Point2.Y = Crosshair.Y;
	}
      else
	{
	  line->Point2.X = Crosshair.X;
	  line->Point2.Y = line->Point1.Y + SGN (dy) * abs (dx);;
	}
    }
}

struct drc_info
{
  LineType *line;
  bool bottom_side;
  bool top_side;
  jmp_buf env;
  ElementType *element;
  Cardinal group;
  LayerType *layer;
  char *drawn_line_netclass;
  Coord drawn_line_clearance;
  Coord max_clearance;
};

static int
drcVia_callback (const BoxType * b, void *cl)
{
  PinType *via = (PinType *) b;
  struct drc_info *i = (struct drc_info *) cl;
  char *netclass;
  Coord required_drc_clearance;
  Coord tmp;

  if (TEST_FLAG (FOUNDFLAG, via))
    return 1;

  tmp = i->line->Thickness;
  netclass = get_netclass_for_via (via);
  required_drc_clearance = get_clearance_between_netclasses (i->drawn_line_netclass, netclass);
  i->line->Thickness = Settings.LineThickness + 2 * required_drc_clearance;

  if (PinLineIntersect (via, i->line))
    {
      via->ExtraDrcClearance = required_drc_clearance - i->drawn_line_clearance;
      i->line->Thickness = tmp;
      if (TEST_FLAG (AUTODRCFLAG, PCB))
        longjmp (i->env, 1);
    }
  i->line->Thickness = tmp;
  return 1;
}

static int
drcPin_callback (const BoxType * b, void *cl)
{
  PinType *pin = (PinType *) b;
  struct drc_info *i = (struct drc_info *) cl;
  char *netclass;
  Coord required_drc_clearance;
  Coord tmp;

  if (TEST_FLAG (FOUNDFLAG, pin))
    return 1;

  tmp = i->line->Thickness;
  netclass = get_netclass_for_pin (pin);
  required_drc_clearance = get_clearance_between_netclasses (i->drawn_line_netclass, netclass);
  i->line->Thickness = Settings.LineThickness + 2 * required_drc_clearance;

  if (PinLineIntersect (pin, i->line))
    {
      pin->ExtraDrcClearance = required_drc_clearance - i->drawn_line_clearance;
      i->line->Thickness = tmp;
      if (TEST_FLAG (AUTODRCFLAG, PCB))
        longjmp (i->env, 1);
    }
  i->line->Thickness = tmp;
  return 1;
}

static int
drcPad_callback (const BoxType * b, void *cl)
{
  PadType *pad = (PadType *) b;
  struct drc_info *i = (struct drc_info *) cl;
  char *netclass;
  Coord required_drc_clearance;
  Coord tmp;

  if (TEST_FLAG (FOUNDFLAG, pad) || TEST_FLAG (ONSOLDERFLAG, pad) != i->bottom_side)
    return 1;

  tmp = i->line->Thickness;
  netclass = get_netclass_for_pad (pad);
  required_drc_clearance = get_clearance_between_netclasses (i->drawn_line_netclass, netclass);
  i->line->Thickness = Settings.LineThickness + 2 * required_drc_clearance;

  if (LinePadIntersect (i->line, pad))
    {
      pad->ExtraDrcClearance = required_drc_clearance - i->drawn_line_clearance;
      i->line->Thickness = tmp;
      if (TEST_FLAG (AUTODRCFLAG, PCB))
        longjmp (i->env, 1);
    }
  i->line->Thickness = tmp;
  return 1;
}

static int
drcLine_callback (const BoxType * b, void *cl)
{
  LineType *line = (LineType *) b;
  struct drc_info *i = (struct drc_info *) cl;
  char *netclass;
  Coord required_drc_clearance;
  Coord tmp;

  if (TEST_FLAG (FOUNDFLAG, line))
    return 1;

  tmp = i->line->Thickness;
  netclass = get_netclass_for_line (i->layer, line);
  required_drc_clearance = get_clearance_between_netclasses (i->drawn_line_netclass, netclass);
  i->line->Thickness = Settings.LineThickness + 2 * required_drc_clearance;

  if (LineLineIntersect (line, i->line))
    {
      line->ExtraDrcClearance = required_drc_clearance - i->drawn_line_clearance;
      i->line->Thickness = tmp;
      if (TEST_FLAG (AUTODRCFLAG, PCB))
        longjmp (i->env, 1);
    }
  i->line->Thickness = tmp;
  return 1;
}

static int
drcArc_callback (const BoxType * b, void *cl)
{
  ArcType *arc = (ArcType *) b;
  struct drc_info *i = (struct drc_info *) cl;
  char *netclass;
  Coord required_drc_clearance;
  Coord tmp;

  if (TEST_FLAG (FOUNDFLAG, arc))
    return 1;

  tmp = i->line->Thickness;
  netclass = get_netclass_for_arc (i->layer, arc);
  required_drc_clearance = get_clearance_between_netclasses (i->drawn_line_netclass, netclass);
  i->line->Thickness = Settings.LineThickness + 2 * required_drc_clearance;

  if (LineArcIntersect (i->line, arc))
    {
      arc->ExtraDrcClearance = required_drc_clearance - i->drawn_line_clearance;
      i->line->Thickness = tmp;
      if (TEST_FLAG (AUTODRCFLAG, PCB))
        longjmp (i->env, 1);
    }
  i->line->Thickness = tmp;
  return 1;
}

static bool
lines_hit_obstacle (struct drc_info *info, LineType *line1, LineType *line2)
{
  if (setjmp (info->env) == 0)
    {
      info->line = line1;
      r_search (PCB->Data->via_tree, &info->line->BoundingBox, NULL, drcVia_callback, info);
      r_search (PCB->Data->pin_tree, &info->line->BoundingBox, NULL, drcPin_callback, info);
      if (info->bottom_side || info->top_side)
        r_search (PCB->Data->pad_tree, &info->line->BoundingBox, NULL, drcPad_callback, info);
      if (line2 != NULL)
        {
          info->line = line2;
          r_search (PCB->Data->via_tree, &info->line->BoundingBox, NULL, drcVia_callback, info);
          r_search (PCB->Data->pin_tree, &info->line->BoundingBox, NULL, drcPin_callback, info);
          if (info->bottom_side || info->top_side)
            r_search (PCB->Data->pad_tree, &info->line->BoundingBox, NULL, drcPad_callback, info);
        }
      GROUP_LOOP (PCB->Data, info->group);
      {
        info->line = line1;
        info->layer = layer;
        r_search (layer->line_tree, &info->line->BoundingBox, NULL, drcLine_callback, info);
        r_search (layer->arc_tree,  &info->line->BoundingBox, NULL, drcArc_callback,  info);
        if (line2 != NULL)
          {
            info->line = line2;
            r_search (layer->line_tree, &info->line->BoundingBox, NULL, drcLine_callback, info);
            r_search (layer->arc_tree,  &info->line->BoundingBox, NULL, drcArc_callback,  info);
          }
      }
      END_LOOP;

      return false;
    }
  else
    {
      return true;
    }
}

static bool
line_hits_obstacle (struct drc_info *info, LineType *line)
{
  return lines_hit_obstacle (info, line, NULL);
}


/* drc_line() checks for intersectors against a line and
 * adjusts the end point until there is no intersection or
 * it winds up back at the start.

 * If way is false it checks straight start, 45 end lines,
 * otherwise it checks 45 start, straight end.
 *
 * It returns the straight-line length of the best answer, and
 * changes the position of the input point to the best answer.
 */

static double
drc_line (PointType *end)
{
  double f, s;
  Coord dx, dy, initial_length, last, length;
  LineType line1;
  struct drc_info info;

  info.group = GetLayerGroupNumberByNumber (INDEXOFCURRENT);
  info.bottom_side = (GetLayerGroupNumberBySide (BOTTOM_SIDE) == info.group);
  info.top_side = (GetLayerGroupNumberBySide (TOP_SIDE) == info.group);
  info.drawn_line_netclass = Crosshair.Netclass;
  info.drawn_line_clearance = PCB->Bloat; /* XXX: PICK THIS UP FROM MIN CLEARANCE IN line_netclass -> * */
  info.max_clearance = get_max_clearance_for_netclass (info.drawn_line_netclass);

  f = 1.0;
  s = 0.5;
  last = -1;

  line1.Flags = NoFlags ();
  line1.Thickness = Settings.LineThickness + 2 * info.max_clearance;
  line1.Clearance = 0;
  line1.Point1.X = Crosshair.AttachedLine.Point1.X;
  line1.Point1.Y = Crosshair.AttachedLine.Point1.Y;
  dx = end->X - line1.Point1.X;
  dy = end->Y - line1.Point1.Y;
  length = initial_length = hypot (dx, dy);

  while (length != last)
    {
      last = length;

      dx = (double)(end->X - line1.Point1.X) * f;
      dy = (double)(end->Y - line1.Point1.Y) * f;
      line1.Point2.X = line1.Point1.X + dx;
      line1.Point2.Y = line1.Point1.Y + dy;
      SetLineBoundingBox (&line1);

      if (line_hits_obstacle (&info, &line1))
        f -= s; /* bumped into something, back off */
      else
        f += s; /* no intersector! */

      f = MIN (f, 1.0); /* Avoid extending the line beyond the mouse pointer */

      s *= 0.5;

      length = f * initial_length;
    }

  end->X = line1.Point2.X;
  end->Y = line1.Point2.Y;
  return length;
}


/* drc_lines() checks for intersectors against two lines and
 * adjusts the end point until there is no intersection or
 * it winds up back at the start. If way is false it checks
 * straight start, 45 end lines, otherwise it checks 45 start,
 * straight end.
 *
 * It returns the straight-line length of the best answer, and
 * changes the position of the input point to the best answer.
 */

static double
drc_lines2 (PointType *end, bool way)
{
  double f, s;
  double f2, s2;
  double len, best;
  Coord dx, dy;
  Coord temp, last, length;
  Coord temp2, last2, length2;
  LineType line1, line2;
  bool two_lines, x_is_long, blocker;
  PointType ans;
  struct drc_info info;

  info.group = GetLayerGroupNumberByNumber (INDEXOFCURRENT);
  info.bottom_side = (GetLayerGroupNumberBySide (BOTTOM_SIDE) == info.group);
  info.top_side = (GetLayerGroupNumberBySide (TOP_SIDE) == info.group);
  info.drawn_line_netclass = Crosshair.Netclass;
  info.drawn_line_clearance = PCB->Bloat; /* XXX: PICK THIS UP FROM MIN CLEARANCE IN line_netclass -> * */
  info.max_clearance = get_max_clearance_for_netclass (info.drawn_line_netclass);

  f = 1.0;
  s = 0.5;
  last = -1;
  line1.Flags = line2.Flags = NoFlags ();
  line1.Thickness = Settings.LineThickness + 2 * info.max_clearance;
  line2.Thickness = line1.Thickness;
  line1.Clearance = line2.Clearance = 0;
  line1.Point1.X = Crosshair.AttachedLine.Point1.X;
  line1.Point1.Y = Crosshair.AttachedLine.Point1.Y;
  dx = end->X - line1.Point1.X;
  dy = end->Y - line1.Point1.Y;

  x_is_long = (abs (dx) > abs (dy));

  if (x_is_long)
    length = abs (dx);
  else
    length = abs (dy);

  temp = length;
  /* assume the worst */
  best = 0.0;
  ans.X = line1.Point1.X;
  ans.Y = line1.Point1.Y;
  while (length != last)
    {
      last = length;
      if (x_is_long)
        {
          dx = SGN (dx) * length;
          dy = end->Y - line1.Point1.Y;
          length2 = abs (dy);
        }
      else
        {
          dx = end->X - line1.Point1.X;
          dy = SGN (dy) * length;
          length2 = abs (dx);
        }
      temp2 = length2;
      f2 = 1.0;
      s2 = 0.5;
      last2 = -1;
      blocker = true;
      while (length2 != last2)
        {
          if (x_is_long)
            dy = SGN (dy) * length2;
          else
            dx = SGN (dx) * length2;
          two_lines = true;
          if (abs (dx) > abs (dy) && x_is_long)
            {
              line1.Point2.X = line1.Point1.X +
                (way ? SGN (dx) * abs (dy) : dx - SGN (dx) * abs (dy));
              line1.Point2.Y = line1.Point1.Y + (way ? dy : 0);
            }
          else if (abs (dy) >= abs (dx) && !x_is_long)
            {
              line1.Point2.X = line1.Point1.X + (way ? dx : 0);
              line1.Point2.Y = line1.Point1.Y +
                (way ? SGN (dy) * abs (dx) : dy - SGN (dy) * abs (dx));
            }
          else if (x_is_long)
            {
              /* we've changed which axis is long, so only do one line */
              line1.Point2.X = line1.Point1.X + dx;
              line1.Point2.Y = line1.Point1.Y + (way ? SGN (dy) * abs (dx) : 0);
              two_lines = false;
            }
          else
            {
              /* we've changed which axis is long, so only do one line */
              line1.Point2.X = line1.Point1.X + (way ? SGN (dx) * abs (dy) : 0);
              line1.Point2.Y = line1.Point1.Y + dy;
              two_lines = false;
            }
          line2.Point1.X = line1.Point2.X;
          line2.Point1.Y = line1.Point2.Y;
          if (two_lines)
            {
              line2.Point2.X = line1.Point1.X + dx;
              line2.Point2.Y = line1.Point1.Y + dy;
            }
          else
            {
              line2.Point2.Y = line1.Point2.Y;
              line2.Point2.X = line1.Point2.X;
            }
          SetLineBoundingBox (&line1);
          SetLineBoundingBox (&line2);
          last2 = length2;

          if (lines_hit_obstacle (&info, &line1, two_lines ? &line2 : NULL))
            {
              f2 -= s2; /* bumped into something, back off */
            }
          else
            {
              f2 += s2; /* no intersector! */
              blocker = false;
              len = hypot (line2.Point2.X - line1.Point1.X, line2.Point2.Y - line1.Point1.Y);
              if (len > best)
                {
                  best = len;
                  ans.X = line2.Point2.X;
                  ans.Y = line2.Point2.Y;
                }
            }

          s2 *= 0.5;
          length2 = MIN (f2 * temp2, temp2);
        }
      if (!blocker && (( x_is_long && line2.Point2.X - line1.Point1.X == dx) ||
                       (!x_is_long && line2.Point2.Y - line1.Point1.Y == dy)))
        f += s;
      else
        f -= s;
      s *= 0.5;
      length = MIN (f * temp, temp);
    }

  end->X = ans.X;
  end->Y = ans.Y;
  return best;
}

/* drc_lines() checks for intersectors against two lines and
 * adjusts the end point until there is no intersection or
 * it winds up back at the start. If way is false it checks
 * straight start, 45 end lines, otherwise it checks 45 start,
 * straight end.
 *
 * It returns the straight-line length of the best answer, and
 * changes the position of the input point to the best answer.
 */

static double
drc_lines (PointType *end, bool way)
{
  double f, s, f2, s2, len, best;
  Coord dx, dy, temp, last, length;
  Coord temp2, last2, length2;
  LineType line1, line2;
  struct drc_info info;
  bool two_lines, x_is_long, blocker;
  PointType ans;

  info.drawn_line_netclass = Crosshair.Netclass;
  info.drawn_line_clearance = PCB->Bloat; /* XXX: PICK THIS UP FROM MIN CLEARANCE IN line_netclass -> * */
  info.max_clearance = get_max_clearance_for_netclass (info.drawn_line_netclass);

  f = 1.0;
  s = 0.5;
  last = -1;
  line1.Flags = line2.Flags = NoFlags ();
  line1.Thickness = Settings.LineThickness + 2 * info.max_clearance;
  line2.Thickness = line1.Thickness;
  line1.Clearance = line2.Clearance = 0;
  line1.Point1.X = Crosshair.AttachedLine.Point1.X;
  line1.Point1.Y = Crosshair.AttachedLine.Point1.Y;
  dy = end->Y - line1.Point1.Y;
  dx = end->X - line1.Point1.X;
  if (abs (dx) > abs (dy))
    {
      x_is_long = true;
      length = abs (dx);
    }
  else
    {
      x_is_long = false;
      length = abs (dy);
    }

  info.group = GetLayerGroupNumberByNumber (INDEXOFCURRENT);
  info.bottom_side = (GetLayerGroupNumberBySide (BOTTOM_SIDE) == info.group);
  info.top_side = (GetLayerGroupNumberBySide (TOP_SIDE) == info.group);

  temp = length;
  /* assume the worst */
  best = 0.0;
  ans.X = line1.Point1.X;
  ans.Y = line1.Point1.Y;
  while (length != last)
    {
      last = length;
      if (x_is_long)
	{
	  dx = SGN (dx) * length;
	  dy = end->Y - line1.Point1.Y;
	  length2 = abs (dy);
	}
      else
	{
	  dy = SGN (dy) * length;
	  dx = end->X - line1.Point1.X;
	  length2 = abs (dx);
	}
      temp2 = length2;
      f2 = 1.0;
      s2 = 0.5;
      last2 = -1;
      blocker = true;
      while (length2 != last2)
	{
	  if (x_is_long)
	    dy = SGN (dy) * length2;
	  else
	    dx = SGN (dx) * length2;
	  two_lines = true;
	  if (abs (dx) > abs (dy) && x_is_long)
	    {
	      line1.Point2.X = line1.Point1.X +
		(way ? SGN (dx) * abs (dy) : dx - SGN (dx) * abs (dy));
	      line1.Point2.Y = line1.Point1.Y + (way ? dy : 0);
	    }
	  else if (abs (dy) >= abs (dx) && !x_is_long)
	    {
	      line1.Point2.X = line1.Point1.X + (way ? dx : 0);
	      line1.Point2.Y = line1.Point1.Y +
		(way ? SGN (dy) * abs (dx) : dy - SGN (dy) * abs (dx));
	    }
	  else if (x_is_long)
	    {
	      /* we've changed which axis is long, so only do one line */
	      line1.Point2.X = line1.Point1.X + dx;
	      line1.Point2.Y =
		line1.Point1.Y + (way ? SGN (dy) * abs (dx) : 0);
	      two_lines = false;
	    }
	  else
	    {
	      /* we've changed which axis is long, so only do one line */
	      line1.Point2.Y = line1.Point1.Y + dy;
	      line1.Point2.X =
		line1.Point1.X + (way ? SGN (dx) * abs (dy) : 0);
	      two_lines = false;
	    }
	  line2.Point1.X = line1.Point2.X;
	  line2.Point1.Y = line1.Point2.Y;
	  if (!two_lines)
	    {
	      line2.Point2.Y = line1.Point2.Y;
	      line2.Point2.X = line1.Point2.X;
	    }
	  else
	    {
	      line2.Point2.X = line1.Point1.X + dx;
	      line2.Point2.Y = line1.Point1.Y + dy;
	    }
	  SetLineBoundingBox (&line1);
	  SetLineBoundingBox (&line2);
	  last2 = length2;
          if (lines_hit_obstacle (&info, &line1, two_lines ? &line2 : NULL))
            {
              f2 -= s2; /* bumped into something, back off */
            }
          else
            {
	      f2 += s2; /* no intersector! */

	      blocker = false;
	      len = (line2.Point2.X - line1.Point1.X);
	      len *= len;
	      len += (double) (line2.Point2.Y - line1.Point1.Y) *
		(line2.Point2.Y - line1.Point1.Y);
	      if (len > best)
		{
		  best = len;
		  ans.X = line2.Point2.X;
		  ans.Y = line2.Point2.Y;
		}
#if 0
	      if (f2 > 1.0)
		f2 = 0.5;
#endif
	    }
	  s2 *= 0.5;
	  length2 = MIN (f2 * temp2, temp2);
	}
      if (!blocker && (( x_is_long && line2.Point2.X - line1.Point1.X == dx) ||
                       (!x_is_long && line2.Point2.Y - line1.Point1.Y == dy)))
	f += s;
      else
	f -= s;
      s *= 0.5;
      length = MIN (f * temp, temp);
    }

  end->X = ans.X;
  end->Y = ans.Y;
  return best;
}

void
EnforceLineDRC (void)
{
  PointType rs;
#if 1
  PointType r45;
  bool shift;
  double r1, r2;
#endif

  /* Silence a bogus compiler warning by storing this in a variable */
  int layer_idx = INDEXOFCURRENT;

  if (!TEST_FLAG (AUTODRCFLAG, PCB) && !TEST_FLAG (SHOWDRCFLAG, PCB))
    return;

  if ( gui->mod1_is_pressed() || gui->control_is_pressed () || PCB->RatDraw
      || layer_idx >= max_copper_layer)
    return;

  /* Reset ExtraDrcClearance on all objects */
  GROUP_LOOP (PCB->Data, GetLayerGroupNumberByNumber (INDEXOFCURRENT));
    {
      LINE_LOOP (layer);
        {
          line->ExtraDrcClearance = 0;
        }
      END_LOOP;
      ARC_LOOP (layer);
        {
          arc->ExtraDrcClearance = 0;
        }
      END_LOOP;
      POLYGON_LOOP (layer);
        {
          polygon->ExtraDrcClearance = 0;
        }
      END_LOOP;
      TEXT_LOOP (layer);
        {
          text->ExtraDrcClearance = 0;
        }
      END_LOOP;
    }
  END_LOOP;
  ELEMENT_LOOP (PCB->Data);
    {
      PIN_LOOP (element);
        {
          pin->ExtraDrcClearance = 0;
        }
      END_LOOP;
      PAD_LOOP (element);
        {
          pad->ExtraDrcClearance = 0;
        }
      END_LOOP;
    }
  END_LOOP;
  VIA_LOOP (PCB->Data);
    {
      via->ExtraDrcClearance = 0;
    }
  END_LOOP;

  rs.X = Crosshair.X;
  rs.Y = Crosshair.Y;
#if 1
  r45.X = Crosshair.X;
  r45.Y = Crosshair.Y;
#endif

  if (!TEST_FLAG (AUTODRCFLAG, PCB))
    {
      /* Just run drc_lines to update clearances, without accepting any of its adjustment, when AUTODRCFLAG is not set */
      if (TEST_FLAG (ALLDIRECTIONFLAG, PCB))
        drc_line (&rs);
      else
        drc_lines (&rs, (PCB->Clipping == 2) != gui->shift_is_pressed ());
      return;
    }

  if (TEST_FLAG (ALLDIRECTIONFLAG, PCB))
    {
      drc_line (&rs);
      Crosshair.X = rs.X;
      Crosshair.Y = rs.Y;
    }
  else
    {
#if 0 /* Auto switch starting angle */
      /* first try starting straight */
      r1 = drc_lines (&rs, false);
      /* then try starting at 45 */
      r2 = drc_lines (&r45, true);

      shift = gui->shift_is_pressed ();
      if (XOR (r1 > r2, shift))
        {
          if (PCB->Clipping)
            PCB->Clipping = shift ? 2 : 1;
          Crosshair.X = rs.X;
          Crosshair.Y = rs.Y;
        }
      else
        {
          if (PCB->Clipping)
            PCB->Clipping = shift ? 1 : 2;
          Crosshair.X = r45.X;
          Crosshair.Y = r45.Y;
        }
#else /* Fixed starting angle */
//      drc_lines (&rs, (PCB->Clipping == 1) != gui->shift_is_pressed ());
      drc_lines (&rs, false);
      Crosshair.X = rs.X;
      Crosshair.Y = rs.Y;
#endif
    }
}
