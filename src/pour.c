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


/* special polygon editing routines
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <math.h>
#include <memory.h>
#include <setjmp.h>

#include "global.h"
#include "box.h"
#include "create.h"
#include "crosshair.h"
#include "data.h"
#include "draw.h"
#include "error.h"
#include "find.h"
#include "misc.h"
#include "move.h"
#include "polygon.h"
#include "pour.h"
#include "remove.h"
#include "rtree.h"
#include "search.h"
#include "set.h"
#include "thermal.h"
#include "undo.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

RCSID ("$Id$");

#define ROUND(x) ((long)(((x) >= 0 ? (x) + 0.5  : (x) - 0.5)))

#define UNSUBTRACT_BLOAT 10

/* ---------------------------------------------------------------------------
 * local prototypes
 */


/* --------------------------------------------------------------------------
 * remove redundant polygon points. Any point that lies on the straight
 * line between the points on either side of it is redundant.
 * returns true if any points are removed
 */
Boolean
RemoveExcessPourPoints (LayerTypePtr Layer, PourTypePtr Pour)
{
  PointTypePtr pt1, pt2, pt3;
  Cardinal n;
  LineType line;
  Boolean changed = False;

  if (Undoing ())
    return (False);
  /* there are always at least three points in a pour */
  pt1 = &Pour->Points[Pour->PointN - 1];
  pt2 = &Pour->Points[0];
  pt3 = &Pour->Points[1];
  for (n = 0; n < Pour->PointN; n++, pt1++, pt2++, pt3++)
    {
      /* wrap around pour */
      if (n == 1)
        pt1 = &Pour->Points[0];
      if (n == Pour->PointN - 1)
        pt3 = &Pour->Points[0];
      line.Point1 = *pt1;
      line.Point2 = *pt3;
      line.Thickness = 0;
      if (IsPointOnLine ((float) pt2->X, (float) pt2->Y, 0.0, &line))
        {
          RemoveObject (POURPOINT_TYPE, (void *) Layer, (void *) Pour,
                        (void *) pt2);
          changed = True;
        }
    }
  return (changed);
}

/* ---------------------------------------------------------------------------
 * returns the index of the polygon point which is the end
 * point of the segment with the lowest distance to the passed
 * coordinates
 */
Cardinal
GetLowestDistancePourPoint (PourTypePtr Pour, LocationType X,
                            LocationType Y)
{
  return GetLowestDistancePourPoint (Pour, X, Y);
}

/* ---------------------------------------------------------------------------
 * go back to the  previous point of the polygon
 */
void
GoToPreviousPourPoint (void)
{
  switch (Crosshair.AttachedPour.PointN)
    {
      /* do nothing if mode has just been entered */
    case 0:
      break;

      /* reset number of points and 'LINE_MODE' state */
    case 1:
      Crosshair.AttachedPour.PointN = 0;
      Crosshair.AttachedLine.State = STATE_FIRST;
      addedLines = 0;
      break;

      /* back-up one point */
    default:
      {
        PointTypePtr points = Crosshair.AttachedPour.Points;
        Cardinal n = Crosshair.AttachedPour.PointN - 2;

        Crosshair.AttachedPour.PointN--;
        Crosshair.AttachedLine.Point1.X = points[n].X;
        Crosshair.AttachedLine.Point1.Y = points[n].Y;
        break;
      }
    }
}

/* ---------------------------------------------------------------------------
 * close pour if possible
 */
void
ClosePour (void)
{
  Cardinal n = Crosshair.AttachedPour.PointN;

  /* check number of points */
  if (n >= 3)
    {
      /* if 45 degree lines are what we want do a quick check
       * if closing the polygon makes sense
       */
      if (!TEST_FLAG (ALLDIRECTIONFLAG, PCB))
        {
          BDimension dx, dy;

          dx = abs (Crosshair.AttachedPour.Points[n - 1].X -
                    Crosshair.AttachedPour.Points[0].X);
          dy = abs (Crosshair.AttachedPour.Points[n - 1].Y -
                    Crosshair.AttachedPour.Points[0].Y);
          if (!(dx == 0 || dy == 0 || dx == dy))
            {
              Message
                (_
                 ("Cannot close polygon because 45 degree lines are requested.\n"));
              return;
            }
        }
      CopyAttachedPourToLayer ();
      Draw ();
    }
  else
    Message (_("A polygon has to have at least 3 points\n"));
}

/* ---------------------------------------------------------------------------
 * moves the data of the attached (new) polygon to the current layer
 */
void
CopyAttachedPourToLayer (void)
{
  PourTypePtr pour;
  int saveID;

  /* move data to layer and clear attached struct */
  pour = CreateNewPour (CURRENT, NoFlags ());
  saveID = pour->ID;
  *pour = Crosshair.AttachedPour;
  pour->ID = saveID;
  SET_FLAG (CLEARPOLYFLAG, pour);
  if (TEST_FLAG (NEWFULLPOLYFLAG, PCB))
    SET_FLAG (FULLPOLYFLAG, pour);
  memset (&Crosshair.AttachedPour, 0, sizeof (PourType));
  SetPourBoundingBox (pour);
  if (!CURRENT->pour_tree)
    CURRENT->pour_tree = r_create_tree (NULL, 0, 0);
  r_insert_entry (CURRENT->pour_tree, (BoxType *) pour, 0);
//  InitClip (PCB->Data, CURRENT, pour);
//  DrawPolygon (CURRENT, polygon, 0);
  DrawPour (CURRENT, pour, 0);
  SetChangedFlag (True);

  /* reset state of attached line */
  Crosshair.AttachedLine.State = STATE_FIRST;
  addedLines = 0;

  /* add to undo list */
  AddObjectToCreateUndoList (POUR_TYPE, CURRENT, pour, pour);
  IncrementUndoSerialNumber ();
}

static POLYAREA *
original_pour (PourType * p)
{
  PLINE *contour = NULL;
  POLYAREA *np = NULL;
  Vector v;

  /* first make initial polygon contour */
  POLYGONPOINT_LOOP (p);
  {
    v[0] = point->X;
    v[1] = point->Y;
    if (contour == NULL)
      {
        if ((contour = poly_NewContour (v)) == NULL)
          return NULL;
      }
    else
      poly_InclVertex (contour->head.prev, poly_CreateNode (v));
  }
  END_LOOP;
  if (contour == NULL)
    {
      printf ("How did that escape - did the loop iterate zero times??\n");
      POLYGONPOINT_LOOP (p);
        {
          printf ("Hello\n");
        }
      END_LOOP;
      return NULL;
    }
  poly_PreContour (contour, TRUE);
  /* make sure it is a positive contour */
  if ((contour->Flags.orient) != PLF_DIR)
    poly_InvContour (contour);
  assert ((contour->Flags.orient) == PLF_DIR);
  if ((np = poly_Create ()) == NULL)
    return NULL;
  poly_InclContour (np, contour);
  assert (poly_Valid (np));
#warning FIXME Later
//  return biggest (np);
  return np;
}

int
InitPourClip (DataTypePtr Data, LayerTypePtr layer, PourType * p)
{
  POLYAREA *clipped, *pg;

  printf ("InitPourClip\n");
  /* Free any children? */
  if (p->PolygonN)
    {
      printf ("We already had children. Killing them now.\n");
      /* TODO: Free existing children, remove them from whatever r_tree etc.. */
    }

  clipped = original_pour (p);
  if (!clipped)
    {
      printf ("Clipping returned NULL - can that be good?\n");
      return 0;
    }
  assert (poly_Valid (clipped));
  if (TEST_FLAG (CLEARPOLYFLAG, p))
    {
      /* Clip the pour against anything we can find in this layer */
      /* TODO: Clear up API so the resulting areas are in "clipped" */
      // e.g.: clearPour (Data, layer, p, NULL, 0);
    }
  pg = clipped;
  do
    {
      /* TODO: For each piece of the clipped up polygon, create a new child */
    }
  while ((pg = pg->f) != clipped);

  poly_Free (&clipped);
  return 1;
}

void
RestoreToPour (DataType * Data, int type, void *ptr1, void *ptr2)
{
  printf ("FIXME Later\n");
}

void
ClearFromPour (DataType * Data, int type, void *ptr1, void *ptr2)
{
  printf ("FIXME Later\n");
}

