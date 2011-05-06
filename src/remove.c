/* $Id$ */

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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Contact addresses for paper mail and Email:
 *  Thomas Nau, Schlehenweg 15, 88471 Baustetten, Germany
 *  Thomas.Nau@rz.uni-ulm.de
 *
 */


/* functions used to remove vias, pins ...
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <setjmp.h>
#include <memory.h>

#include "global.h"

#include "data.h"
#include "draw.h"
#include "error.h"
#include "misc.h"
#include "move.h"
#include "mymem.h"
//#include "polygon.h"
#include "pour.h"
#include "rats.h"
#include "remove.h"
#include "rtree.h"
#include "search.h"
#include "select.h"
#include "set.h"
#include "undo.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

RCSID ("$Id$");


/* ---------------------------------------------------------------------------
 * some local prototypes
 */
static void *DestroyVia (PinTypePtr);
static void *DestroyRat (RatTypePtr);
static void *DestroyLine (LayerTypePtr, LineTypePtr);
static void *DestroyArc (LayerTypePtr, ArcTypePtr);
static void *DestroyText (LayerTypePtr, TextTypePtr);
static void *DestroyPour (LayerTypePtr, PourTypePtr);
static void *DestroyElement (ElementTypePtr);
static void *RemoveVia (PinTypePtr);
static void *RemoveRat (RatTypePtr);
static void *DestroyPourPoint (LayerTypePtr, PourTypePtr, PointTypePtr);
static void *RemovePourContour (LayerTypePtr, PourTypePtr, Cardinal);
static void *RemovePourPoint (LayerTypePtr, PourTypePtr, PointTypePtr);
static void *RemoveLinePoint (LayerTypePtr, LineTypePtr, PointTypePtr);

/* ---------------------------------------------------------------------------
 * some local types
 */
static ObjectFunctionType RemoveFunctions = {
  RemoveLine,
  RemoveText,
  NULL,
  RemovePour,
  RemoveVia,
  RemoveElement,
  NULL,
  NULL,
  NULL,
  RemoveLinePoint,
  RemovePourPoint,
  RemoveArc,
  RemoveRat
};
static ObjectFunctionType DestroyFunctions = {
  DestroyLine,
  DestroyText,
  NULL, //DestroyPolygon,
  DestroyPour,
  DestroyVia,
  DestroyElement,
  NULL,
  NULL,
  NULL,
  NULL,
  DestroyPourPoint,
  DestroyArc,
  DestroyRat
};
static DataTypePtr DestroyTarget;
static bool Bulk = false;

/* ---------------------------------------------------------------------------
 * remove PCB
 */
void
RemovePCB (PCBTypePtr Ptr)
{
  ClearUndoList (true);
  FreePCBMemory (Ptr);
  free (Ptr);
}

/* ---------------------------------------------------------------------------
 * destroys a via
 */
static void *
DestroyVia (PinTypePtr Via)
{
  r_delete_entry (DestroyTarget->via_tree, (BoxTypePtr) Via);
  free (Via->Name);

  DestroyTarget->Via = g_list_remove (DestroyTarget->Via, Via);
  DestroyTarget->ViaN --;

  g_slice_free (PinType, Via);

  return NULL;
}

/* ---------------------------------------------------------------------------
 * destroys a line from a layer 
 */
static void *
DestroyLine (LayerTypePtr Layer, LineTypePtr Line)
{
  r_delete_entry (Layer->line_tree, (BoxTypePtr) Line);
  free (Line->Number);

  Layer->Line = g_list_remove (Layer->Line, Line);
  Layer->LineN --;

  g_slice_free (LineType, Line);

  return NULL;
}

/* ---------------------------------------------------------------------------
 * destroys an arc from a layer 
 */
static void *
DestroyArc (LayerTypePtr Layer, ArcTypePtr Arc)
{
  r_delete_entry (Layer->arc_tree, (BoxTypePtr) Arc);

  Layer->Arc = g_list_remove (Layer->Arc, Arc);
  Layer->ArcN --;

  g_slice_free (ArcType, Arc);

  return NULL;
}

/* ---------------------------------------------------------------------------
 * destroys a pour from a layer
 */
static void *
DestroyPour (LayerTypePtr Layer, PourTypePtr Pour)
{
  r_delete_entry (Layer->pour_tree, (BoxTypePtr) Pour);
  FreePourMemory (Pour);

  Layer->Pour = g_list_remove (Layer->Pour, Pour);
  Layer->PourN --;

  g_slice_free (PourType, Pour);

  return NULL;
}

/* ---------------------------------------------------------------------------
 * removes a pour-point from a pour and destroys the data
 */
static void *
DestroyPourPoint (LayerTypePtr Layer,
                  PourTypePtr Pour, PointTypePtr Point)
{
  Cardinal point_idx;
  Cardinal i;
  Cardinal contour;
  Cardinal contour_start, contour_end, contour_points;

  point_idx = pour_point_idx (Pour, Point);
  contour = pour_point_contour (Pour, point_idx);
  contour_start = (contour == 0) ? 0 : Pour->HoleIndex[contour - 1];
  contour_end = (contour == Pour->HoleIndexN) ? Pour->PointN :
                                                Pour->HoleIndex[contour];
  contour_points = contour_end - contour_start;

  if (contour_points <= 3)
    return RemovePourContour (Layer, Pour, contour);

  r_delete_entry (Layer->pour_tree, (BoxType *) Pour);

  /* remove point from list, keep point order */
  for (i = point_idx; i < Pour->PointN - 1; i++)
    Pour->Points[i] = Pour->Points[i + 1];
  Pour->PointN--;

  /* Shift down indices of any holes */
  for (i = 0; i < Pour->HoleIndexN; i++)
    if (Pour->HoleIndex[i] > point_idx)
      Pour->HoleIndex[i]--;

  SetPourBoundingBox (Pour);
  r_insert_entry (Layer->pour_tree, (BoxType *) Pour, 0);
  InitPourClip (PCB->Data, Layer, Pour);
  return (Pour);
}

/* ---------------------------------------------------------------------------
 * destroys a text from a layer
 */
static void *
DestroyText (LayerTypePtr Layer, TextTypePtr Text)
{
  free (Text->TextString);
  r_delete_entry (Layer->text_tree, (BoxTypePtr) Text);

  Layer->Text = g_list_remove (Layer->Text, Text);
  Layer->TextN --;

  g_slice_free (TextType, Text);

  return NULL;
}

/* ---------------------------------------------------------------------------
 * destroys a element
 */
static void *
DestroyElement (ElementTypePtr Element)
{
  if (DestroyTarget->element_tree)
    r_delete_entry (DestroyTarget->element_tree, (BoxType *) Element);
  if (DestroyTarget->pin_tree)
    {
      PIN_LOOP (Element);
      {
	r_delete_entry (DestroyTarget->pin_tree, (BoxType *) pin);
      }
      END_LOOP;
    }
  if (DestroyTarget->pad_tree)
    {
      PAD_LOOP (Element);
      {
	r_delete_entry (DestroyTarget->pad_tree, (BoxType *) pad);
      }
      END_LOOP;
    }
  ELEMENTTEXT_LOOP (Element);
  {
    if (DestroyTarget->name_tree[n])
      r_delete_entry (DestroyTarget->name_tree[n], (BoxType *) text);
  }
  END_LOOP;
  FreeElementMemory (Element);

  DestroyTarget->Element = g_list_remove (DestroyTarget->Element, Element);
  DestroyTarget->ElementN --;

  g_slice_free (ElementType, Element);

  return NULL;
}

/* ---------------------------------------------------------------------------
 * destroys a rat
 */
static void *
DestroyRat (RatTypePtr Rat)
{
  if (DestroyTarget->rat_tree)
    r_delete_entry (DestroyTarget->rat_tree, &Rat->BoundingBox);

  DestroyTarget->Rat = g_list_remove (DestroyTarget->Rat, Rat);
  DestroyTarget->RatN --;

  g_slice_free (RatType, Rat);

  return NULL;
}


/* ---------------------------------------------------------------------------
 * removes a via
 */
static void *
RemoveVia (PinTypePtr Via)
{
  /* erase from screen and memory */
  if (PCB->ViaOn)
    {
      EraseVia (Via);
      if (!Bulk)
	Draw ();
    }
  MoveObjectToRemoveUndoList (VIA_TYPE, Via, Via, Via);
  return NULL;
}

/* ---------------------------------------------------------------------------
 * removes a rat
 */
static void *
RemoveRat (RatTypePtr Rat)
{
  /* erase from screen and memory */
  if (PCB->RatOn)
    {
      EraseRat (Rat);
      if (!Bulk)
	Draw ();
    }
  MoveObjectToRemoveUndoList (RATLINE_TYPE, Rat, Rat, Rat);
  return NULL;
}

struct rlp_info
{
  jmp_buf env;
  LineTypePtr line;
  PointTypePtr point;
};
static int
remove_point (const BoxType * b, void *cl)
{
  LineType *line = (LineType *) b;
  struct rlp_info *info = (struct rlp_info *) cl;
  if (line == info->line)
    return 0;
  if ((line->Point1.X == info->point->X)
      && (line->Point1.Y == info->point->Y))
    {
      info->line = line;
      info->point = &line->Point1;
      longjmp (info->env, 1);
    }
  else
    if ((line->Point2.X == info->point->X)
	&& (line->Point2.Y == info->point->Y))
    {
      info->line = line;
      info->point = &line->Point2;
      longjmp (info->env, 1);
    }
  return 0;
}

/* ---------------------------------------------------------------------------
 * removes a line point, or a line if the selected point is the end
 */
static void *
RemoveLinePoint (LayerTypePtr Layer, LineTypePtr Line, PointTypePtr Point)
{
  PointType other;
  struct rlp_info info;
  if (&Line->Point1 == Point)
    other = Line->Point2;
  else
    other = Line->Point1;
  info.line = Line;
  info.point = Point;
  if (setjmp (info.env) == 0)
    {
      r_search (Layer->line_tree, (const BoxType *) Point, NULL, remove_point,
		&info);
      return RemoveLine (Layer, Line);
    }
  MoveObject (LINEPOINT_TYPE, Layer, info.line, info.point,
	      other.X - Point->X, other.Y - Point->Y);
  return (RemoveLine (Layer, Line));
}

/* ---------------------------------------------------------------------------
 * removes a line from a layer 
 */
void *
RemoveLine (LayerTypePtr Layer, LineTypePtr Line)
{
  /* erase from screen */
  if (Layer->On)
    {
      EraseLine (Line);
      if (!Bulk)
	Draw ();
    }
  MoveObjectToRemoveUndoList (LINE_TYPE, Layer, Line, Line);
  return NULL;
}

/* ---------------------------------------------------------------------------
 * removes an arc from a layer 
 */
void *
RemoveArc (LayerTypePtr Layer, ArcTypePtr Arc)
{
  /* erase from screen */
  if (Layer->On)
    {
      EraseArc (Arc);
      if (!Bulk)
	Draw ();
    }
  MoveObjectToRemoveUndoList (ARC_TYPE, Layer, Arc, Arc);
  return NULL;
}

/* ---------------------------------------------------------------------------
 * removes a text from a layer
 */
void *
RemoveText (LayerTypePtr Layer, TextTypePtr Text)
{
  /* erase from screen */
  if (Layer->On)
    {
      EraseText (Layer, Text);
      if (!Bulk)
	Draw ();
    }
  MoveObjectToRemoveUndoList (TEXT_TYPE, Layer, Text, Text);
  return NULL;
}

/* ---------------------------------------------------------------------------
 * removes a pour from a layer
 */
void *
RemovePour (LayerTypePtr Layer, PourTypePtr Pour)
{
  /* erase from screen */
  if (Layer->On)
    {
      ErasePour (Pour);
      if (!Bulk)
	Draw ();
    }
  MoveObjectToRemoveUndoList (POUR_TYPE, Layer, Pour, Pour);
  return NULL;
}

/* ---------------------------------------------------------------------------
 * removes a contour from a pour.
 * If removing the outer contour, it removes the whole pour.
 */
static void *
RemovePourContour (LayerTypePtr Layer,
                   PourTypePtr Pour,
                   Cardinal contour)
{
  Cardinal contour_start, contour_end, contour_points;
  Cardinal i;

  if (contour == 0)
    return RemovePour (Layer, Pour);

  if (Layer->On)
    {
      ErasePour (Pour);
      if (!Bulk)
        Draw ();
    }

  /* Copy the pour to the undo list */
  AddObjectToRemoveContourUndoList (POUR_TYPE, Layer, Pour);

  contour_start = (contour == 0) ? 0 : Pour->HoleIndex[contour - 1];
  contour_end = (contour == Pour->HoleIndexN) ? Pour->PointN :
                                                Pour->HoleIndex[contour];
  contour_points = contour_end - contour_start;

  /* remove points from list, keep point order */
  for (i = contour_start; i < Pour->PointN - contour_points; i++)
    Pour->Points[i] = Pour->Points[i + contour_points];
  Pour->PointN -= contour_points;

  /* remove hole from list and shift down remaining indices */
  for (i = contour; i < Pour->HoleIndexN; i++)
    Pour->HoleIndex[i - 1] = Pour->HoleIndex[i] - contour_points;
  Pour->HoleIndexN--;

  InitPourClip (PCB->Data, Layer, Pour);
  /* redraw pour if necessary */
  if (Layer->On)
    {
      DrawPour (Layer, Pour);
      if (!Bulk)
        Draw ();
    }
  return NULL;
}

/* ---------------------------------------------------------------------------
 * removes a pour-point from a pour
 */
static void *
RemovePourPoint (LayerTypePtr Layer,
                 PourTypePtr Pour, PointTypePtr Point)
{
  Cardinal point_idx;
  Cardinal i;
  Cardinal contour;
  Cardinal contour_start, contour_end, contour_points;

  point_idx = pour_point_idx (Pour, Point);
  contour = pour_point_contour (Pour, point_idx);
  contour_start = (contour == 0) ? 0 : Pour->HoleIndex[contour - 1];
  contour_end = (contour == Pour->HoleIndexN) ? Pour->PointN :
                                                Pour->HoleIndex[contour];
  contour_points = contour_end - contour_start;

  if (contour_points <= 3)
    return RemovePourContour (Layer, Pour, contour);

  if (Layer->On)
    ErasePour (Pour);

  /* insert the pour-point into the undo list */
  AddObjectToRemovePointUndoList (POURPOINT_TYPE, Layer, Pour, point_idx);
  r_delete_entry (Layer->pour_tree, (BoxType *) Pour);

  /* remove point from list, keep point order */
  for (i = point_idx; i < Pour->PointN - 1; i++)
    Pour->Points[i] = Pour->Points[i + 1];
  Pour->PointN--;

  /* Shift down indices of any holes */
  for (i = 0; i < Pour->HoleIndexN; i++)
    if (Pour->HoleIndex[i] > point_idx)
      Pour->HoleIndex[i]--;

  SetPourBoundingBox (Pour);
  r_insert_entry (Layer->pour_tree, (BoxType *) Pour, 0);
  RemoveExcessPourPoints (Layer, Pour);
  InitPourClip (PCB->Data, Layer, Pour);

  /* redraw pour if necessary */
  if (Layer->On)
    {
      DrawPour (Layer, Pour);
      if (!Bulk)
	Draw ();
    }
  return NULL;
}

/* ---------------------------------------------------------------------------
 * removes an element
 */
void *
RemoveElement (ElementTypePtr Element)
{
  /* erase from screen */
  if ((PCB->ElementOn || PCB->PinOn) &&
      (FRONT (Element) || PCB->InvisibleObjectsOn))
    {
      EraseElement (Element);
      if (!Bulk)
	Draw ();
    }
  MoveObjectToRemoveUndoList (ELEMENT_TYPE, Element, Element, Element);
  return NULL;
}

/* ----------------------------------------------------------------------
 * removes all selected and visible objects
 * returns true if any objects have been removed
 */
bool
RemoveSelected (void)
{
  Bulk = true;
  if (SelectedOperation (&RemoveFunctions, false, ALL_TYPES))
    {
      IncrementUndoSerialNumber ();
      Draw ();
      Bulk = false;
      return (true);
    }
  Bulk = false;
  return (false);
}

/* ---------------------------------------------------------------------------
 * remove object as referred by pointers and type,
 * allocated memory is passed to the 'remove undo' list
 */
void *
RemoveObject (int Type, void *Ptr1, void *Ptr2, void *Ptr3)
{
  void *ptr = ObjectOperation (&RemoveFunctions, Type, Ptr1, Ptr2, Ptr3);
  return (ptr);
}

/* ---------------------------------------------------------------------------
 * DeleteRats - deletes rat lines only
 * can delete all rat lines, or only selected one
 */

bool
DeleteRats (bool selected)
{
  bool changed = false;
  Bulk = true;
  RAT_LOOP (PCB->Data);
  {
    if ((!selected) || TEST_FLAG (SELECTEDFLAG, line))
      {
	changed = true;
	RemoveRat (line);
      }
  }
  END_LOOP;
  Bulk = false;
  if (changed)
    {
      Draw ();
      IncrementUndoSerialNumber ();
    }
  return (changed);
}

/* ---------------------------------------------------------------------------
 * remove object as referred by pointers and type
 * allocated memory is destroyed assumed to already be erased
 */
void *
DestroyObject (DataTypePtr Target, int Type, void *Ptr1,
	       void *Ptr2, void *Ptr3)
{
  DestroyTarget = Target;
  return (ObjectOperation (&DestroyFunctions, Type, Ptr1, Ptr2, Ptr3));
}
