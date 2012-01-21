/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996 Thomas Nau
 *  Copyright (C) 2012 PCB Contributors (See ChangeLog for details)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory.h>
#include <math.h>

#include "global.h"
#include "crosshair.h"
#include "data.h"
#include "draw.h"
#include "line.h"
#include "misc.h"
#include "polygon.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif


static void
thindraw_pcb_polygon (PolygonType *polygon)
{
  Cardinal i;
  for (i = 0; i < polygon->PointN; i++)
    {
      Cardinal next = next_contour_point (polygon, i);
      gui->draw_line (Crosshair.GC,
                      polygon->Points[   i].X, polygon->Points[   i].Y,
                      polygon->Points[next].X, polygon->Points[next].Y);
    }
}

static void
thindraw_pcb_line (LineType *line)
{
  Coord dx, dy, ox, oy;
  Coord thick;
  double h;

  dx = line->Point2.X - line->Pointt1.X;
  dx = line->Point2.Y - line->Pointt1.Y;
  thick = line->Thickness;

  if (dx != 0 || dy != 0)
    h = 0.5 * thick / sqrt (SQUARE (dx) + SQUARE (dy));
  else
    h = 0.0;

  ox =   dy * h + 0.5 * SGN (dy);
  oy = -(dx * h + 0.5 * SGN (dx));
  gui->draw_line (Crosshair.GC, line->Point1.X + ox, line->Point1.Y + oy,
                                line->Point2.X + ox, line->Point2.Y + oy);

  if (abs (ox) >= pixel_slop || abs (oy) >= pixel_slop)
    {
      Angle angle = atan2 (dx, dy) * 57.295779;
      gui->draw_line (Crosshair.GC, line->Point1.X - ox, line->Point1.Y - oy,
                                    line->Point2.X - ox, line->Point2.Y - oy);
      gui->draw_arc (Crosshair.GC, line->Point1.X, line->Point1.Y,
                                   thick / 2, thick / 2, angle - 180, 180);
      gui->draw_arc (Crosshair.GC, line->Point2.X, line->Point2.Y,
                                   thick / 2, thick / 2, angle, 180);
    }
}

static void
thindraw_pcb_arc (ArcType *arc)
{
  if (wid > pixel_slop)
    {
      BoxType *bx = GetArcEnds (arc);
      gui->draw_arc (Crosshair.GC, arc->X, arc->Y,
                    arc->Width + arc->Thickness, arc->Height + arc->Thickness,
                     arc->StartAngle, arc->Delta);

      gui->draw_arc (Crosshair.GC, arc->X, arc->Y,
                     arc->Width - arc->Thickness, arc->Height - arc->Thickness,
                     arc->StartAngle, arc->Delta);

      gui->draw_arc (Crosshair.GC, bx->X1, bx->Y1,
                     arc->Thickness, arc->Thickness,
                     arc->StartAngle, -180 * SGN (arc->Delta));

      gui->draw_arc (Crosshair.GC, bx->X2, bx->Y2,
                     arc->Thickness, arc->Thickness,
                     arc->StartAngle + arc->Delta, 180 * SGN (arc->Delta));
    }
  else
    gui->draw_arc (Crosshair.GC, arc->X, arc->Y, arc->Width, arc->Height, arc->StartAngle, arc->Delta);
}

/* ---------------------------------------------------------------------------
 * draws the elements of a loaded circuit which is to be merged in
 */
static void
thindraw_pcb_element (ElementType *Element)
{
  /* if no silkscreen, draw the bounding box */
  if (Element->ArcN == 0 && Element->LineN == 0)
    {
      gui->draw_line (Crosshair.GC, Element->BoundingBox.X1, Element->BoundingBox.Y1,
                                    Element->BoundingBox.X1, Element->BoundingBox.Y2);
      gui->draw_line (Crosshair.GC, Element->BoundingBox.X1, Element->BoundingBox.Y2,
                                    Element->BoundingBox.X2, Element->BoundingBox.Y2);
      gui->draw_line (Crosshair.GC, Element->BoundingBox.X2, Element->BoundingBox.Y2,
                                    Element->BoundingBox.X2, Element->BoundingBox.Y1);
      gui->draw_line (Crosshair.GC, Element->BoundingBox.X2, Element->BoundingBox.Y1,
                                    Element->BoundingBox.X1, Element->BoundingBox.Y1);
    }
  else
    {
      ELEMENTLINE_LOOP (Element);
      {
        thindraw_pcb_line (line);
      }
      END_LOOP;
      ARC_LOOP (Element);
      {
        thindraw_pcb_arc (arc);
      }
      END_LOOP;
    }

  PIN_LOOP (Element);
  {
    thindraw_moved_pv (pin, DX, DY);
  }
  END_LOOP;

  /* pads */
  PAD_LOOP (Element);
  {
    if (PCB->InvisibleObjectsOn ||
        (TEST_FLAG (ONSOLDERFLAG, pad) != 0) == Settings.ShowSolderSide)
      {
        thindraw_pcb_pad (Crosshair.GC, pad, false, false);
      }
  }
  END_LOOP;

  /* Element mark */
  gui->draw_line (Crosshair.GC, Element->MarkX - EMARK_SIZE, Element->MarkY,
                                Element->MarkX,              Element->MarkY - EMARK_SIZE);
  gui->draw_line (Crosshair.GC, Element->MarkX + EMARK_SIZE, Element->MarkY,
                                Element->MarkX,              Element->MarkY - EMARK_SIZE);
  gui->draw_line (Crosshair.GC, Element->MarkX - EMARK_SIZE, Element->MarkY,
                                Element->MarkX,              Element->MarkY + EMARK_SIZE);
  gui->draw_line (Crosshair.GC, Element->MarkX + EMARK_SIZE, Element->MarkY,
                                Element->MarkX,              Element->MarkY + EMARK_SIZE);
}

/* ---------------------------------------------------------------------------
 * draws all visible and attached objects of the pastebuffer
 */
static void
thindraw_pcb_buffer (BufferType *Buffer)
{
  Cardinal i;

  /* draw all visible layers */
  for (i = 0; i < max_copper_layer + 2; i++)
    if (PCB->Data->Layer[i].On)
      {
        LayerType *layer = &Buffer->Data->Layer[i];

        LINE_LOOP (layer);
        {
          thindraw_pcb_line (line);
        }
        END_LOOP;
        ARC_LOOP (layer);
        {
          thindraw_pcb_arc (arc);
        }
        END_LOOP;
        TEXT_LOOP (layer);
        {
          thindraw_pcb_text (text);
        }
        END_LOOP;
        POLYGON_LOOP (layer);
        {
          thindraw_pcb_polygon (polygon);
        }
        END_LOOP;
      }

  /* draw elements if visible */
  if (PCB->PinOn && PCB->ElementOn)
    {
      ELEMENT_LOOP (Buffer->Data);
      {
        if (FRONT (element) || PCB->InvisibleObjectsOn)
          thindraw_pcb_element (element);
      }
      END_LOOP;
    }

  /* and the vias */
  if (PCB->ViaOn)
    {
      VIA_LOOP (Buffer->Data);
      {
        thindraw_pcb_pv (via);
      }
      END_LOOP;
    }
}
