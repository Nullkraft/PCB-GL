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

/* FIXME: I THINK THIS IS THE SAME AS THE COMMON THINDRAW ROUTINE:
 *        OK, NO.. IT ISN'T, THAT ONE RENDERS THE CLIPPED POLYGON.
 *        SPEAKING OF WHICH.. I WONDER IF THIS ONE SHOULD TO!
 */
static void
outline_draw_pcb_polygon (DrawAPI *dapi, LayerType *layer, PolygonType *polygon)
{
  Cardinal i;
  for (i = 0; i < polygon->PointN; i++)
    {
      Cardinal next = next_contour_point (polygon, i);
      dapi->graphics->draw_line (dapi->gc,
                                 polygon->Points[   i].X, polygon->Points[   i].Y,
                                 polygon->Points[next].X, polygon->Points[next].Y);
    }
}

static void
outline_draw_pcb_line (DrawAPI *dapi, LayerType *layer, LineType *line)
{
  Coord dx, dy, ox, oy;
  Coord thick;
  double h;

  dx = line->Point2.X - line->Point1.X;
  dy = line->Point2.Y - line->Point1.Y;
  thick = line->Thickness;

  if (dx != 0 || dy != 0)
    h = 0.5 * thick / sqrt (SQUARE (dx) + SQUARE (dy));
  else
    h = 0.0;

  ox =   dy * h + 0.5 * SGN (dy);
  oy = -(dx * h + 0.5 * SGN (dx));
  dapi->graphics->draw_line (dapi->gc, line->Point1.X + ox, line->Point1.Y + oy,
                                       line->Point2.X + ox, line->Point2.Y + oy);

  if (abs (ox) >= pixel_slop || abs (oy) >= pixel_slop)
    {
      Angle angle = atan2 (dx, dy) * 57.295779;
      dapi->graphics->draw_line (dapi->gc, line->Point1.X - ox, line->Point1.Y - oy,
                                           line->Point2.X - ox, line->Point2.Y - oy);
      dapi->graphics->draw_arc (dapi->gc, line->Point1.X, line->Point1.Y,
                                          thick / 2, thick / 2, angle - 180, 180);
      dapi->graphics->draw_arc (dapi->gc, line->Point2.X, line->Point2.Y,
                                          thick / 2, thick / 2, angle, 180);
    }
}

static void
outline_draw_pcb_arc (DrawAPI *dapi, LayerType *layer, ArcType *arc)
{
  if (arc->Width > pixel_slop)
    {
      BoxType *bx = GetArcEnds (arc);
      dapi->graphics->draw_arc (dapi->gc, arc->X, arc->Y,
                                arc->Width + arc->Thickness, arc->Height + arc->Thickness,
                                arc->StartAngle, arc->Delta);

      dapi->graphics->draw_arc (dapi->gc, arc->X, arc->Y,
                                arc->Width - arc->Thickness, arc->Height - arc->Thickness,
                                arc->StartAngle, arc->Delta);

      dapi->graphics->draw_arc (dapi->gc, bx->X1, bx->Y1,
                                arc->Thickness, arc->Thickness,
                                arc->StartAngle, -180 * SGN (arc->Delta));

      dapi->graphics->draw_arc (dapi->gc, bx->X2, bx->Y2,
                                arc->Thickness, arc->Thickness,
                                arc->StartAngle + arc->Delta, 180 * SGN (arc->Delta));
    }
  else
    dapi->graphics->draw_arc (dapi->gc, arc->X, arc->Y,
                              arc->Width, arc->Height, arc->StartAngle, arc->Delta);
}

/* ---------------------------------------------------------------------------
 * draws the elements of a loaded circuit which is to be merged in
 */
static void
outline_draw_pcb_element (DrawAPI *dapi, ElementType *Element)
{
  /* if no silkscreen, draw the bounding box */
  if (Element->ArcN == 0 && Element->LineN == 0)
    {
      dapi->graphics->draw_line (dapi->gc, Element->BoundingBox.X1, Element->BoundingBox.Y1,
                                           Element->BoundingBox.X1, Element->BoundingBox.Y2);
      dapi->graphics->draw_line (dapi->gc, Element->BoundingBox.X1, Element->BoundingBox.Y2,
                                           Element->BoundingBox.X2, Element->BoundingBox.Y2);
      dapi->graphics->draw_line (dapi->gc, Element->BoundingBox.X2, Element->BoundingBox.Y2,
                                           Element->BoundingBox.X2, Element->BoundingBox.Y1);
      dapi->graphics->draw_line (dapi->gc, Element->BoundingBox.X2, Element->BoundingBox.Y1,
                                           Element->BoundingBox.X1, Element->BoundingBox.Y1);
    }
  else
    {
      ELEMENTLINE_LOOP (Element);
      {
        dapi->draw_pcb_line (dapi, NULL, line);
      }
      END_LOOP;
      ARC_LOOP (Element);
      {
        dapi->draw_pcb_arc (dapi, NULL, arc);
      }
      END_LOOP;
    }

  PIN_LOOP (Element);
  {
    dapi->draw_pcb_pin (dapi, pin);
  }
  END_LOOP;

  /* pads */
  PAD_LOOP (Element);
  {
    if (PCB->InvisibleObjectsOn ||
        (TEST_FLAG (ONSOLDERFLAG, pad) != 0) == Settings.ShowSolderSide)
      {
        dapi->draw_pcb_pad (dapi, NULL, pad);
      }
  }
  END_LOOP;

  /* Element mark */
  dapi->graphics->draw_line (dapi->gc, Element->MarkX - EMARK_SIZE, Element->MarkY,
                                       Element->MarkX,              Element->MarkY - EMARK_SIZE);
  dapi->graphics->draw_line (dapi->gc, Element->MarkX + EMARK_SIZE, Element->MarkY,
                                       Element->MarkX,              Element->MarkY - EMARK_SIZE);
  dapi->graphics->draw_line (dapi->gc, Element->MarkX - EMARK_SIZE, Element->MarkY,
                                       Element->MarkX,              Element->MarkY + EMARK_SIZE);
  dapi->graphics->draw_line (dapi->gc, Element->MarkX + EMARK_SIZE, Element->MarkY,
                                       Element->MarkX,              Element->MarkY + EMARK_SIZE);
}

/* ---------------------------------------------------------------------------
 * draws all visible and attached objects of the pastebuffer
 */
static void
outline_draw_pcb_buffer (DrawAPI *dapi, BufferType *Buffer)
{
  Cardinal i;

  /* draw all visible layers */
  for (i = 0; i < max_copper_layer + 2; i++)
    if (PCB->Data->Layer[i].On)
      {
        LayerType *layer = &Buffer->Data->Layer[i];

        LINE_LOOP (layer);
        {
          dapi->draw_pcb_line (dapi, layer, line);
        }
        END_LOOP;
        ARC_LOOP (layer);
        {
          dapi->draw_pcb_arc (dapi, layer, arc);
        }
        END_LOOP;
        TEXT_LOOP (layer);
        {
          dapi->draw_pcb_text (dapi, layer, text);
        }
        END_LOOP;
        POLYGON_LOOP (layer);
        {
          dapi->draw_pcb_polygon (dapi, layer, polygon);
        }
        END_LOOP;
      }

  /* draw elements if visible */
  if (PCB->PinOn && PCB->ElementOn)
    {
      ELEMENT_LOOP (Buffer->Data);
      {
        if (FRONT (element) || PCB->InvisibleObjectsOn)
          dapi->draw_pcb_element (dapi, element);
      }
      END_LOOP;
    }

  /* and the vias */
  if (PCB->ViaOn)
    {
      VIA_LOOP (Buffer->Data);
      {
        dapi->draw_pcb_via (dapi, via);
      }
      END_LOOP;
    }
}

static void
outline_draw_pcb_pv (DrawAPI *dapi, PinType *pin)
{
  dapi->graphics->thindraw_pcb_pv (dapi->gc, dapi->gc, pin, true, false);
}

static void
outline_draw_pcb_pv_mask (DrawAPI *dapi, PinType *pin)
{
  dapi->graphics->thindraw_pcb_pv (dapi->gc, dapi->gc, pin, true, true);
}

static void
outline_draw_pcb_pad (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  dapi->graphics->thindraw_pcb_pad (dapi->gc, pad, false, false);
}

DrawAPI *outline_draw_new (HID_DRAW_API *graphics)
{
  DrawAPI *dapi;

  dapi = g_new0 (DrawAPI, 1);
  dapi->graphics = graphics;

  dapi->draw_pcb_pin          = outline_draw_pcb_pv;
  dapi->draw_pcb_pin_mask     = outline_draw_pcb_pv_mask;
  dapi->draw_pcb_pin_hole     = NULL;
  dapi->draw_pcb_via          = outline_draw_pcb_pv;
  dapi->draw_pcb_via_mask     = outline_draw_pcb_pv_mask;
  dapi->draw_pcb_via_hole     = NULL;
  dapi->draw_pcb_pad          = outline_draw_pcb_pad;
  dapi->draw_pcb_pad_mask     = NULL;
  dapi->draw_pcb_pad_paste    = NULL;
  dapi->draw_pcb_line         = outline_draw_pcb_line;
  dapi->draw_pcb_arc          = outline_draw_pcb_arc;
  dapi->draw_pcb_polygon      = outline_draw_pcb_polygon;

  dapi->draw_pcb_element      = outline_draw_pcb_element;
  dapi->draw_pcb_layer        = NULL;
  dapi->draw_pcb_layer_group  = NULL;
  dapi->draw_pcb_buffer       = outline_draw_pcb_buffer;
  dapi->set_draw_offset       = NULL;
  dapi->set_clip_box          = NULL;

  return dapi;
}
