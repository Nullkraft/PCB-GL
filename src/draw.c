/* $Id$ */

/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996, 2003, 2004 Thomas Nau
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


/* drawing routines
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "global.h"

/*#include "clip.h"*/
#include "compat.h"
#include "crosshair.h"
#include "data.h"
#include "draw.h"
#include "error.h"
#include "mymem.h"
#include "misc.h"
#include "rotate.h"
#include "rtree.h"
#include "search.h"
#include "select.h"
#include "print.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

#undef NDEBUG
#include <assert.h>

#ifndef MAXINT
#define MAXINT (((unsigned int)(~0))>>1)
#endif

RCSID ("$Id$");

#define	SMALL_SMALL_TEXT_SIZE	0
#define	SMALL_TEXT_SIZE			1
#define	NORMAL_TEXT_SIZE		2
#define	LARGE_TEXT_SIZE			3
#define	N_TEXT_SIZES			4


/* ---------------------------------------------------------------------------
 * some local identifiers
 */
static BoxType Block = {MAXINT, MAXINT, -MAXINT, -MAXINT};

/*---------------------------------------------------------------------------
 *  Adds the update rect to the update region
 */
static void
AddPart (void *b)
{
  BoxType *box = (BoxType *) b;

  Block.X1 = MIN (Block.X1, box->X1);
  Block.X2 = MAX (Block.X2, box->X2);
  Block.Y1 = MIN (Block.Y1, box->Y1);
  Block.Y2 = MAX (Block.Y2, box->Y2);
}

/*
 * initiate the actual redrawing of the updated area
 */
void
Draw (void)
{
  if (Block.X1 <= Block.X2 && Block.Y1 <= Block.Y2)
    gui->invalidate_lr (Block.X1, Block.X2, Block.Y1, Block.Y2);

  /* shrink the update block */
  Block.X1 = Block.Y1 =  MAXINT;
  Block.X2 = Block.Y2 = -MAXINT;
}

/* ---------------------------------------------------------------------- 
 * redraws all the data by the event handlers
 */
void
Redraw (void)
{
  gui->invalidate_all ();
}

static void
GatherPVName (PinTypePtr Ptr)
{
  BoxType box;
  bool vert = TEST_FLAG (EDGE2FLAG, Ptr);

  if (vert)
    {
      box.X1 = Ptr->X - Ptr->Thickness / 2 + Settings.PinoutTextOffsetY;
      box.Y1 = Ptr->Y - Ptr->DrillingHole / 2 - Settings.PinoutTextOffsetX;
    }
  else
    {
      box.X1 = Ptr->X + Ptr->DrillingHole / 2 + Settings.PinoutTextOffsetX;
      box.Y1 = Ptr->Y - Ptr->Thickness / 2 + Settings.PinoutTextOffsetY;
    }

  if (vert)
    {
      box.X2 = box.X1;
      box.Y2 = box.Y1;
    }
  else
    {
      box.X2 = box.X1;
      box.Y2 = box.Y1;
    }
  AddPart (&box);
}

static void
GatherPadName (PadTypePtr Pad)
{
  BoxType box;
  bool vert;

  /* should text be vertical ? */
  vert = (Pad->Point1.X == Pad->Point2.X);

  if (vert)
    {
      box.X1 = Pad->Point1.X                      - Pad->Thickness / 2;
      box.Y1 = MAX (Pad->Point1.Y, Pad->Point2.Y) + Pad->Thickness / 2;
      box.X1 += Settings.PinoutTextOffsetY;
      box.Y1 -= Settings.PinoutTextOffsetX;
      box.X2 = box.X1;
      box.Y2 = box.Y1;
    }
  else
    {
      box.X1 = MIN (Pad->Point1.X, Pad->Point2.X) - Pad->Thickness / 2;
      box.Y1 = Pad->Point1.Y                      - Pad->Thickness / 2;
      box.X1 += Settings.PinoutTextOffsetX;
      box.Y1 += Settings.PinoutTextOffsetY;
      box.X2 = box.X1;
      box.Y2 = box.Y1;
    }

  AddPart (&box);
  return;
}

/* ---------------------------------------------------------------------------
 * draw a via object
 */
void
DrawVia (PinTypePtr Via)
{
  AddPart (Via);
  if (!TEST_FLAG (HOLEFLAG, Via) && TEST_FLAG (DISPLAYNAMEFLAG, Via))
    DrawViaName (Via);
}

/* ---------------------------------------------------------------------------
 * draws the name of a via
 */
void
DrawViaName (PinTypePtr Via)
{
  GatherPVName (Via);
}

/* ---------------------------------------------------------------------------
 * draw a pin object
 */
void
DrawPin (PinTypePtr Pin)
{
  AddPart (Pin);
  if (!TEST_FLAG (HOLEFLAG, Pin) && TEST_FLAG (DISPLAYNAMEFLAG, Pin))
    DrawPinName (Pin);
}

/* ---------------------------------------------------------------------------
 * draws the name of a pin
 */
void
DrawPinName (PinTypePtr Pin)
{
  GatherPVName (Pin);
}

/* ---------------------------------------------------------------------------
 * draw a pad object
 */
void
DrawPad (PadTypePtr Pad)
{
  AddPart (Pad);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Pad))
    DrawPadName (Pad);
}

/* ---------------------------------------------------------------------------
 * draws the name of a pad
 */
void
DrawPadName (PadTypePtr Pad)
{
  GatherPadName (Pad);
}

/* ---------------------------------------------------------------------------
 * draws a line on a layer
 */
void
DrawLine (LayerTypePtr Layer, LineTypePtr Line)
{
  AddPart (Line);
}

/* ---------------------------------------------------------------------------
 * draws a ratline
 */
void
DrawRat (RatTypePtr Rat)
{
  if (Settings.RatThickness < 20)
    Rat->Thickness = pixel_slop * Settings.RatThickness;
  /* rats.c set VIAFLAG if this rat goes to a containing poly: draw a donut */
  if (TEST_FLAG(VIAFLAG, Rat))
    {
      Coord w = Rat->Thickness;

      BoxType b;

      b.X1 = Rat->Point1.X - w * 2 - w / 2;
      b.X2 = Rat->Point1.X + w * 2 + w / 2;
      b.Y1 = Rat->Point1.Y - w * 2 - w / 2;
      b.Y2 = Rat->Point1.Y + w * 2 + w / 2;
      AddPart (&b);
    }
  else
    DrawLine (NULL, (LineType *)Rat);
}

/* ---------------------------------------------------------------------------
 * draws an arc on a layer
 */
void
DrawArc (LayerTypePtr Layer, ArcTypePtr Arc)
{
  AddPart (Arc);
}

/* ---------------------------------------------------------------------------
 * draws a text on a layer
 */
void
DrawText (LayerTypePtr Layer, TextTypePtr Text)
{
  AddPart (Text);
}


/* ---------------------------------------------------------------------------
 * draws a polygon on a layer
 */
void
DrawPolygon (LayerTypePtr Layer, PolygonTypePtr Polygon)
{
  AddPart (Polygon);
}

/* ---------------------------------------------------------------------------
 * draws an element
 */
void
DrawElement (ElementTypePtr Element)
{
  DrawElementPackage (Element);
  DrawElementName (Element);
  DrawElementPinsAndPads (Element);
}

/* ---------------------------------------------------------------------------
 * draws the name of an element
 */
void
DrawElementName (ElementTypePtr Element)
{
  if (TEST_FLAG (HIDENAMEFLAG, Element))
    return;
  DrawText (NULL, &ELEMENT_TEXT (PCB, Element));
}

/* ---------------------------------------------------------------------------
 * draws the package of an element
 */
void
DrawElementPackage (ElementTypePtr Element)
{
  ELEMENTLINE_LOOP (Element);
  {
    DrawLine (NULL, line);
  }
  END_LOOP;
  ARC_LOOP (Element);
  {
    DrawArc (NULL, arc);
  }
  END_LOOP;
}

/* ---------------------------------------------------------------------------
 * draw pins of an element
 */
void
DrawElementPinsAndPads (ElementTypePtr Element)
{
  PAD_LOOP (Element);
  {
    if (FRONT (pad) || PCB->InvisibleObjectsOn)
      DrawPad (pad);
  }
  END_LOOP;
  PIN_LOOP (Element);
  {
    DrawPin (pin);
  }
  END_LOOP;
}

/* ---------------------------------------------------------------------------
 * erase a via
 */
void
EraseVia (PinTypePtr Via)
{
  AddPart (Via);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Via))
    EraseViaName (Via);
}

/* ---------------------------------------------------------------------------
 * erase a ratline
 */
void
EraseRat (RatTypePtr Rat)
{
  if (TEST_FLAG(VIAFLAG, Rat))
    {
      Coord w = Rat->Thickness;

      BoxType b;

      b.X1 = Rat->Point1.X - w * 2 - w / 2;
      b.X2 = Rat->Point1.X + w * 2 + w / 2;
      b.Y1 = Rat->Point1.Y - w * 2 - w / 2;
      b.Y2 = Rat->Point1.Y + w * 2 + w / 2;
      AddPart (&b);
    }
  else
    EraseLine ((LineType *)Rat);
}


/* ---------------------------------------------------------------------------
 * erase a via name
 */
void
EraseViaName (PinTypePtr Via)
{
  GatherPVName (Via);
}

/* ---------------------------------------------------------------------------
 * erase a pad object
 */
void
ErasePad (PadTypePtr Pad)
{
  AddPart (Pad);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Pad))
    ErasePadName (Pad);
}

/* ---------------------------------------------------------------------------
 * erase a pad name
 */
void
ErasePadName (PadTypePtr Pad)
{
  GatherPadName (Pad);
}

/* ---------------------------------------------------------------------------
 * erase a pin object
 */
void
ErasePin (PinTypePtr Pin)
{
  AddPart (Pin);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Pin))
    ErasePinName (Pin);
}

/* ---------------------------------------------------------------------------
 * erase a pin name
 */
void
ErasePinName (PinTypePtr Pin)
{
  GatherPVName (Pin);
}

/* ---------------------------------------------------------------------------
 * erases a line on a layer
 */
void
EraseLine (LineTypePtr Line)
{
  AddPart (Line);
}

/* ---------------------------------------------------------------------------
 * erases an arc on a layer
 */
void
EraseArc (ArcTypePtr Arc)
{
  if (!Arc->Thickness)
    return;
  AddPart (Arc);
}

/* ---------------------------------------------------------------------------
 * erases a text on a layer
 */
void
EraseText (LayerTypePtr Layer, TextTypePtr Text)
{
  AddPart (Text);
}

/* ---------------------------------------------------------------------------
 * erases a polygon on a layer
 */
void
ErasePolygon (PolygonTypePtr Polygon)
{
  AddPart (Polygon);
}

/* ---------------------------------------------------------------------------
 * erases an element
 */
void
EraseElement (ElementTypePtr Element)
{
  ELEMENTLINE_LOOP (Element);
  {
    EraseLine (line);
  }
  END_LOOP;
  ARC_LOOP (Element);
  {
    EraseArc (arc);
  }
  END_LOOP;
  EraseElementName (Element);
  EraseElementPinsAndPads (Element);
}

/* ---------------------------------------------------------------------------
 * erases all pins and pads of an element
 */
void
EraseElementPinsAndPads (ElementTypePtr Element)
{
  PIN_LOOP (Element);
  {
    ErasePin (pin);
  }
  END_LOOP;
  PAD_LOOP (Element);
  {
    ErasePad (pad);
  }
  END_LOOP;
}

/* ---------------------------------------------------------------------------
 * erases the name of an element
 */
void
EraseElementName (ElementTypePtr Element)
{
  if (TEST_FLAG (HIDENAMEFLAG, Element))
    return;
  DrawText (NULL, &ELEMENT_TEXT (PCB, Element));
}


void
EraseObject (int type, void *lptr, void *ptr)
{
  switch (type)
    {
    case VIA_TYPE:
    case PIN_TYPE:
      ErasePin ((PinTypePtr) ptr);
      break;
    case TEXT_TYPE:
    case ELEMENTNAME_TYPE:
      EraseText ((LayerTypePtr)lptr, (TextTypePtr) ptr);
      break;
    case POLYGON_TYPE:
      ErasePolygon ((PolygonTypePtr) ptr);
      break;
    case ELEMENT_TYPE:
      EraseElement ((ElementTypePtr) ptr);
      break;
    case LINE_TYPE:
    case ELEMENTLINE_TYPE:
    case RATLINE_TYPE:
      EraseLine ((LineTypePtr) ptr);
      break;
    case PAD_TYPE:
      ErasePad ((PadTypePtr) ptr);
      break;
    case ARC_TYPE:
    case ELEMENTARC_TYPE:
      EraseArc ((ArcTypePtr) ptr);
      break;
    default:
      Message ("hace: Internal ERROR, trying to erase an unknown type\n");
    }
}



void
DrawObject (int type, void *ptr1, void *ptr2)
{
  switch (type)
    {
    case VIA_TYPE:
      if (PCB->ViaOn)
	DrawVia ((PinTypePtr) ptr2);
      break;
    case LINE_TYPE:
      if (((LayerTypePtr) ptr1)->On)
	DrawLine ((LayerTypePtr) ptr1, (LineTypePtr) ptr2);
      break;
    case ARC_TYPE:
      if (((LayerTypePtr) ptr1)->On)
	DrawArc ((LayerTypePtr) ptr1, (ArcTypePtr) ptr2);
      break;
    case TEXT_TYPE:
      if (((LayerTypePtr) ptr1)->On)
	DrawText ((LayerTypePtr) ptr1, (TextTypePtr) ptr2);
      break;
    case POLYGON_TYPE:
      if (((LayerTypePtr) ptr1)->On)
	DrawPolygon ((LayerTypePtr) ptr1, (PolygonTypePtr) ptr2);
      break;
    case ELEMENT_TYPE:
      if (PCB->ElementOn &&
	  (FRONT ((ElementTypePtr) ptr2) || PCB->InvisibleObjectsOn))
	DrawElement ((ElementTypePtr) ptr2);
      break;
    case RATLINE_TYPE:
      if (PCB->RatOn)
	DrawRat ((RatTypePtr) ptr2);
      break;
    case PIN_TYPE:
      if (PCB->PinOn)
	DrawPin ((PinTypePtr) ptr2);
      break;
    case PAD_TYPE:
      if (PCB->PinOn)
	DrawPad ((PadTypePtr) ptr2);
      break;
    case ELEMENTNAME_TYPE:
      if (PCB->ElementOn &&
	  (FRONT ((ElementTypePtr) ptr2) || PCB->InvisibleObjectsOn))
	DrawElementName ((ElementTypePtr) ptr1);
      break;
    }
}
