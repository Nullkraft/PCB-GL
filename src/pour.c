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


/* special pour editing routines
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
  double mindistance = (double) MAX_COORD * MAX_COORD;
  PointTypePtr ptr1 = &Pour->Points[Pour->PointN - 1],
               ptr2 = &Pour->Points[0];
  Cardinal n, result = 0;

  /* we calculate the distance to each segment and choose the
   * shortest distance. If the closest approach between the
   * given point and the projected line (i.e. the segment extended)
   * is not on the segment, then the distance is the distance
   * to the segment end point.
   */

  for (n = 0; n < Pour->PointN; n++, ptr2++)
    {
      register double u, dx, dy;
      dx = ptr2->X - ptr1->X;
      dy = ptr2->Y - ptr1->Y;
      if (dx != 0.0 || dy != 0.0)
        {
          /* projected intersection is at P1 + u(P2 - P1) */
          u = ((X - ptr1->X) * dx + (Y - ptr1->Y) * dy) / (dx * dx + dy * dy);

          if (u < 0.0)
            {                   /* ptr1 is closest point */
              u = SQUARE (X - ptr1->X) + SQUARE (Y - ptr1->Y);
            }
          else if (u > 1.0)
            {                   /* ptr2 is closest point */
              u = SQUARE (X - ptr2->X) + SQUARE (Y - ptr2->Y);
            }
          else
            {                   /* projected intersection is closest point */
              u = SQUARE (X - ptr1->X * (1.0 - u) - u * ptr2->X) +
                SQUARE (Y - ptr1->Y * (1.0 - u) - u * ptr2->Y);
            }
          if (u < mindistance)
            {
              mindistance = u;
              result = n;
            }
        }
      ptr1 = ptr2;
    }
  return (result);
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
  InitPourClip (PCB->Data, CURRENT, pour);
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

/*---------------------------------------- END OF NICE GENTLE UI DRIVEN PIECES OF THE POUR CODE --------------*/

/*---------------------------------------- THIS CODE BELOW WILL MURDER SMALL ANIMALS THEN LAUGH --------------*/

/* ---------------------------------------------------------------------------
 * destroys a polygon from a pour
 */
static void *
DestroyPolygonInPour (PourTypePtr pour, PolygonTypePtr polygon)
{
  r_delete_entry (pour->polygon_tree, (BoxTypePtr) polygon);

  FreePolygonMemory (polygon);
  *polygon = pour->Polygons[ --pour->PolygonN ];
  r_substitute (pour->polygon_tree,
                (BoxType *) & pour->Polygons[ pour->PolygonN ],
                (BoxType *) polygon);
  memset (&pour->Polygons[ pour->PolygonN ], 0, sizeof (PolygonType));
  return (NULL);
}

static int
subtract_poly (POLYAREA * np1, POLYAREA **pg)
{
  POLYAREA *merged = NULL, *np = np1;
  int x;

  assert (np);
  assert (pg);
  assert (*pg);

  if (pg == NULL)
    {
      printf ("Hmm, got pg == NULL in subtract_poly\n");
      poly_Free (&np);
      return -1;
    }

  assert (poly_Valid (*pg));
  assert (poly_Valid (np));
  x = poly_Boolean_free (*pg, np, &merged, PBO_SUB);
  if (x != err_ok)
    {
      fprintf (stderr, "Error while clipping PBO_SUB: %d\n", x);
      poly_Free (&merged);
      *pg = NULL;
      return -1;
    }

  assert (!merged || poly_Valid (merged));

  *pg = merged;
  return 1;
}

  static int
unite_poly (POLYAREA * np, POLYAREA ** pg)
{
  POLYAREA *merged = NULL;
  int x;
  assert (np);
  assert (pg);
//  assert (*pg);
  x = poly_Boolean_free (*pg, np, &merged, PBO_UNITE);
  if (x != err_ok)
    {
      fprintf (stderr, "Error while clipping PBO_UNITE: %d\n", x);
      poly_Free (&merged);
      *pg = NULL;
      return 0;
    }
  assert (!merged || poly_Valid (merged));
  *pg = merged;
  return 1;
}

static int
intersect_poly (POLYAREA * np, POLYAREA ** pg)
{
  POLYAREA *merged;
  int x;
  assert (np);
  assert (pg);
  assert (*pg);
  x = poly_Boolean_free (*pg, np, &merged, PBO_ISECT);
  if (x != err_ok)
    {
      fprintf (stderr, "Error while clipping PBO_ISECT: %d\n", x);
      poly_Free (&merged);
      *pg = NULL;
      return 0;
    }
  assert (!merged || poly_Valid (merged));
  *pg = merged;
  return 1;
}


static POLYAREA *
get_subtract_pin_poly (DataType * d, PinType * pin, LayerType * l, PourType *pour)
{
  POLYAREA *np;
  Cardinal i;

  if (pin->Clearance == 0)
    return NULL;

  i = GetLayerNumber (d, l);
  if (TEST_THERM (i, pin))
    np = ThermPoly ((PCBTypePtr) (d->pcb), pin, i);
  else
    np = PinPoly (pin, pin->Thickness, pin->Clearance);

  return np;
}

static POLYAREA *
get_subtract_line_poly (LineType *line, PourType *pour)
{
  if (!TEST_FLAG (CLEARLINEFLAG, line))
    return NULL;

  return LinePoly (line, line->Thickness + line->Clearance);
}

static POLYAREA *
get_subtract_arc_poly (ArcType * arc, PourType * pour)
{
  if (!TEST_FLAG (CLEARLINEFLAG, arc))
    return NULL;

  return ArcPoly (arc, arc->Thickness + arc->Clearance);
}

static POLYAREA *
get_subtract_text_poly (TextType * text, PourType * pour)
{
  const BoxType *b = &text->BoundingBox;

  if (!TEST_FLAG (CLEARLINEFLAG, text))
    return NULL;

  return RoundRect (b->X1 + PCB->Bloat, b->X2 - PCB->Bloat,
                    b->Y1 + PCB->Bloat, b->Y2 - PCB->Bloat, PCB->Bloat);
}

static POLYAREA *
get_subtract_pad_poly (PadType * pad, PourType * pour)
{
  POLYAREA *np;

  if (TEST_FLAG (SQUAREFLAG, pad))
    np = SquarePadPoly (pad, pad->Thickness + pad->Clearance);
  else
    np = LinePoly ((LineType *) pad, pad->Thickness + pad->Clearance);

  return np;
}

static POLYAREA *
get_subtract_polygon_poly (PolygonType * polygon, PourType * pour)
{
  POLYAREA *np;

  /* Don't subtract from ourselves! */
  if (polygon->ParentPour == pour || !TEST_FLAG (CLEARLINEFLAG, polygon))
    return NULL;

  poly_Copy0 (&np, polygon->Clipped);

  return np;
}

struct cpInfo
{
  const BoxType *other;
  DataType *data;
  LayerType *layer;
  PourType *pour;
  Boolean solder;
  POLYAREA *pg;
  BoxType *region;
  POLYAREA *accumulate;
  jmp_buf env;
};

static int
pin_sub_callback (const BoxType * b, void *cl)
{
  static int counter = 0;
  PinTypePtr pin = (PinTypePtr) b;
  struct cpInfo *info = (struct cpInfo *) cl;
  POLYAREA *np;

  /* don't subtract the object that was put back! */
  if (b == info->other)
    return 0;

  np = get_subtract_pin_poly (info->data, pin, info->layer, info->pour);
  if (np == NULL)
    return 0;

  unite_poly (np, &info->accumulate);
  counter ++;

  if (counter == 100)
    {
      counter = 0;
      if (subtract_poly (info->accumulate, &info->pg) < 0)
        longjmp (info->env, 1);
      info->accumulate = NULL;
    }

  return 1;
}

static int
arc_sub_callback (const BoxType * b, void *cl)
{
  ArcTypePtr arc = (ArcTypePtr) b;
  struct cpInfo *info = (struct cpInfo *) cl;
  POLYAREA *np;

  /* don't subtract the object that was put back! */
  if (b == info->other)
    return 0;
  if (!TEST_FLAG (CLEARLINEFLAG, arc))
    return 0;

  np = get_subtract_arc_poly (arc, info->pour);
  if (np == NULL)
    return 0;

  if (subtract_poly (np, &info->pg) < 0)
    longjmp (info->env, 1);
  return 1;
}

static int
pad_sub_callback (const BoxType * b, void *cl)
{
  PadTypePtr pad = (PadTypePtr) b;
  struct cpInfo *info = (struct cpInfo *) cl;
  POLYAREA *np;

  /* don't subtract the object that was put back! */
  if (b == info->other)
    return 0;
  if (XOR (TEST_FLAG (ONSOLDERFLAG, pad), info->solder))
    return 0;

  np = get_subtract_pad_poly (pad, info->pour);
  if (np == NULL)
    return 0;

  if (subtract_poly (np, &info->pg) < 0)
    longjmp (info->env, 1);
  return 1;
}

static int
line_sub_callback (const BoxType * b, void *cl)
{
  static int counter = 0;
  LineTypePtr line = (LineTypePtr) b;
  struct cpInfo *info = (struct cpInfo *) cl;
  POLYAREA *np;

  /* don't subtract the object that was put back! */
  if (b == info->other)
    return 0;
  if (!TEST_FLAG (CLEARLINEFLAG, line))
    return 0;

  np = get_subtract_line_poly (line, info->pour);
  if (np == NULL)
    return 0;

  unite_poly (np, &info->accumulate);
  counter ++;

  if (counter == 100)
    {
      counter = 0;
      if (subtract_poly (info->accumulate, &info->pg) < 0)
        longjmp (info->env, 1);
      info->accumulate = NULL;
    }

  return 1;
}


static int
text_sub_callback (const BoxType * b, void *cl)
{
  TextTypePtr text = (TextTypePtr) b;
  struct cpInfo *info = (struct cpInfo *) cl;
  POLYAREA *np;

  /* don't subtract the object that was put back! */
  if (b == info->other)
    return 0;
  if (!TEST_FLAG (CLEARLINEFLAG, text))
    return 0;

  np = get_subtract_text_poly (text, info->pour);
  if (np == NULL)
    return 0;

  if (subtract_poly (np, &info->pg) < 0)
    longjmp (info->env, 1);
  return 1;
}

static int
poly_sub_callback (const BoxType * b, void *cl)
{
  PolygonTypePtr poly = (PolygonTypePtr) b;
  struct cpInfo *info = (struct cpInfo *) cl;
  POLYAREA *np;

  /* don't subtract the object that was put back! */
  if (b == info->other)
    return 0;
  if (!TEST_FLAG (CLEARLINEFLAG, poly))
    return 0;

  np = get_subtract_polygon_poly (poly, info->pour);
  if (np == NULL)
    return 0;

  if (subtract_poly (np, &info->pg) < 0)
    longjmp (info->env, 1);
  return 1;
}

static int
pour_sub_callback (const BoxType * b, void *cl)
{
  PourTypePtr pour = (PourTypePtr) b;
  struct cpInfo *info = (struct cpInfo *) cl;
  BoxType *region = info->region;

  return r_search (pour->polygon_tree, region, NULL, poly_sub_callback, info);

}

static int
Group (DataTypePtr Data, Cardinal layer)
{
  Cardinal i, j;
  for (i = 0; i < max_layer; i++)
    for (j = 0; j < ((PCBType *) (Data->pcb))->LayerGroups.Number[i]; j++)
      if (layer == ((PCBType *) (Data->pcb))->LayerGroups.Entries[i][j])
        return i;
  return i;
}

/* NB: For convenience, we're passing the defined POLYAREA in here */
static int
ClearPour (DataTypePtr Data, LayerTypePtr Layer, PourType * pour,
           POLYAREA **pg, const BoxType * here, BDimension expand)
{
  int r = 0;
  BoxType region;
  struct cpInfo info;
  Cardinal group;

  if (!TEST_FLAG (CLEARPOLYFLAG, pour)
      || GetLayerNumber (Data, Layer) >= max_layer)
    return 0;
  group = Group (Data, GetLayerNumber (Data, Layer));
  info.solder = (group == Group (Data, max_layer + SOLDER_LAYER));
  info.data = Data;
  info.other = here;
  info.layer = Layer;
  info.pour = pour;
  info.pg = *pg;
  if (here)
    region = clip_box (here, &pour->BoundingBox);
  else
    region = pour->BoundingBox;
  region = bloat_box (&region, expand);
  info.region = &region;

  if (setjmp (info.env) == 0)
    {
      r = 0;
      info.accumulate = NULL;
      if (info.solder || group == Group (Data, max_layer + COMPONENT_LAYER))
        r += r_search (Data->pad_tree, &region, NULL, pad_sub_callback, &info);
      GROUP_LOOP (Data, group);
      {
        r += r_search (layer->pour_tree, &region, NULL, pour_sub_callback, &info);
        r += r_search (layer->line_tree, &region, NULL, line_sub_callback, &info);
        subtract_poly (info.accumulate, &info.pg);
        info.accumulate = NULL;
        r += r_search (layer->arc_tree,  &region, NULL, arc_sub_callback,  &info);
        r += r_search (layer->text_tree, &region, NULL, text_sub_callback, &info);
      }
      END_LOOP;
      r += r_search (Data->via_tree, &region, NULL, pin_sub_callback, &info);
      r += r_search (Data->pin_tree, &region, NULL, pin_sub_callback, &info);
      subtract_poly (info.accumulate, &info.pg);
      info.accumulate = NULL;
    }

  *pg = info.pg;

  return r;
}

static int
check_polygon_island_cb (const BoxType * b, void *cl)
{
  PolygonTypePtr polygon = (PolygonTypePtr) b;
  struct cpInfo *info = (struct cpInfo *) cl;

  ASSIGN_FLAG (HOLEFLAG, IsPolygonAnIsland (info->layer, polygon), polygon);
  return 1;
}

static int
mark_islands (DataTypePtr Data, LayerTypePtr layer, PourTypePtr pour,
              int type, void *ptr1, void *ptr2)
{
  struct cpInfo info;
  BoxType region = ((PinTypePtr) ptr2)->BoundingBox;

  region.X1 -= UNSUBTRACT_BLOAT;
  region.Y1 -= UNSUBTRACT_BLOAT;
  region.X2 += UNSUBTRACT_BLOAT;
  region.Y2 += UNSUBTRACT_BLOAT;

  info.region = &region;
  info.layer = layer;

  return r_search (pour->polygon_tree, info.region, NULL,
                   check_polygon_island_cb, &info);
}

static int
subtract_plow (DataTypePtr Data, LayerTypePtr layer, PourTypePtr pour,
              int type, void *ptr1, void *ptr2)
{
  POLYAREA *np = NULL, *pg = NULL, *start_pg, *tmp;
  int count, count_all, count_added;

  switch (type)
    {
    case PIN_TYPE:
    case VIA_TYPE:
      np = get_subtract_pin_poly (Data, (PinTypePtr) ptr2, layer, pour);
      break;
    case LINE_TYPE:
      np = get_subtract_line_poly ((LineTypePtr) ptr2, pour);
      break;
    case ARC_TYPE:
      np = get_subtract_arc_poly ((ArcTypePtr) ptr2, pour);
      break;
    case PAD_TYPE:
      np = get_subtract_pad_poly ((PadTypePtr) ptr2, pour);
      break;
    case POLYGON_TYPE:
      np = get_subtract_polygon_poly ((PolygonTypePtr) ptr2, pour);
      break;
    case POUR_TYPE:
#warning FIXME Later: Need to produce a function for this
      np = NULL;
      break;
    case TEXT_TYPE:
      np = get_subtract_text_poly ((TextTypePtr) ptr2, pour);
      break;
    }

  if (np == NULL)
    {
      printf ("Didn't get a POLYAREA to subtract, so bailing\n");
      return 0;
    }

  assert (poly_Valid (np));

  /* Make pg contain the polygons we're going to fiddle with */

  count = 0;
  POURPOLYGON_LOOP (pour);
  {
    /* Gather up children which are touched by np */
    if (isects (np, polygon, False))
      {
        count++;
        /* Steal their clipped contours, then delete them */
        /* Add contour to local list to fiddle about with */

        assert (poly_Valid (polygon->Clipped));
        if (polygon->Clipped == NULL)
          {
            printf ("Got polygon->clipped == NULL!\n");
            continue;
          }
        if (pg == NULL)
          {
            pg = polygon->Clipped;
            polygon->Clipped = NULL;
          }
        else
          {
            /* Link the _single_ polygon->Clipped into our circular pg list. */
            polygon->Clipped->f = pg;
            polygon->Clipped->b = pg->b;
            pg->b->f = polygon->Clipped;
            pg->b = polygon->Clipped;
            polygon->Clipped = NULL;
          }
        /* POURPOLYGON_LOOP iterates backwards, so it's OK
         * to delete the current element we're sitting on */
        DestroyPolygonInPour (pour, polygon);
      }
  }
  END_LOOP;
//  printf ("Subtract counted %i touching children, now removed\n", count);

  if (pg == NULL)
    {
      printf ("Hmm, got pg == NULL in subtract_plow\n");
      poly_Free (&np);
      return -1;
    }

  assert (poly_Valid (pg));

  /* Perform the subtract operation */

  /* NB: Old *pg is freed inside subtract_poly */
  subtract_poly (np, &pg);

  if (pg == NULL)
    {
      printf ("Poly killed to death by subtracting\n");
      return -1;
    }

#if 0
  count = 0;
  { POLYAREA *pg_start;
  pg_start = pg;
  do {
    count++;
  } while ((pg = pg->f) != pg_start);
  }
  printf ("After subtract, counted %i polygon pieces\n", count);
#endif

  count_all = count_added = 0;
  /* For each piece of the clipped up polygon, create a new child */
  start_pg = pg;
  do
    {
      PolygonType *poly;

      tmp = pg->f;
      pg->f = pg;
      pg->b = pg;

      count_all++;
//      if (pg->contours->area > PCB->IsleArea)
      if (1) // Breaks incremental updates otherwise
        {
          count_added++;
          poly = CreateNewPolygonInPour (pour, pour->Flags);
          poly->Clipped = pg;
          CLEAR_FLAG (SELECTEDFLAG, poly);

          SetPolygonBoundingBox (poly);

          if (pour->polygon_tree == NULL)
            pour->polygon_tree = r_create_tree (NULL, 0, 0);
          r_insert_entry (pour->polygon_tree, (BoxType *) poly, 0);
        }
      else
        {
          poly_Free (&pg);
        }
    }
  while ((pg = tmp) != start_pg);

  mark_islands (Data, layer, pour, type, ptr1, ptr2);

  return 0;
}

static POLYAREA *
get_unsubtract_pin_poly (PinType * pin, LayerType * l, PourType * pour)
{
  /* overlap a bit to prevent gaps from rounding errors */
  return BoxPolyBloated (&pin->BoundingBox, UNSUBTRACT_BLOAT);
}

static POLYAREA *
get_unsubtract_arc_poly (ArcType * arc, LayerType * l, PourType * pour)
{
  if (!TEST_FLAG (CLEARLINEFLAG, arc))
    return NULL;

  /* overlap a bit to prevent gaps from rounding errors */
  return BoxPolyBloated (&arc->BoundingBox, UNSUBTRACT_BLOAT);
}

static POLYAREA *
get_unsubtract_line_poly (LineType * line, LayerType * l, PourType * pour)
{
  if (!TEST_FLAG (CLEARLINEFLAG, line))
    return NULL;

  /* overlap a bit to prevent notches from rounding errors */
  return BoxPolyBloated (&line->BoundingBox, UNSUBTRACT_BLOAT);
}

static POLYAREA *
get_unsubtract_text_poly (TextType * text, LayerType * l, PourType * pour)
{
  if (!TEST_FLAG (CLEARLINEFLAG, text))
    return NULL;

  /* overlap a bit to prevent notches from rounding errors */
  return BoxPolyBloated (&text->BoundingBox, UNSUBTRACT_BLOAT);
}

static POLYAREA *
get_unsubtract_pad_poly (PadType * pad, LayerType * l, PourType * pour)
{
  /* overlap a bit to prevent notches from rounding errors */
  return BoxPolyBloated (&pad->BoundingBox, UNSUBTRACT_BLOAT);
}

static POLYAREA *
get_unsubtract_polygon_poly (PolygonType * poly, LayerType * l, PourType * pour)
{
  /* Don't subtract from ourselves, or if CLEARLINEFLAG isn't set */
  if (poly->ParentPour == pour || !TEST_FLAG (CLEARLINEFLAG, poly))
    return NULL;

  /* overlap a bit to prevent notches from rounding errors */
  return BoxPolyBloated (&poly->BoundingBox, UNSUBTRACT_BLOAT);
}

static POLYAREA *
original_pour_poly (PourType * p)
{
  PLINE *contour = NULL;
  POLYAREA *np = NULL;
  Vector v;

  /* first make initial polygon contour */
  POURPOINT_LOOP (p);
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
      POURPOINT_LOOP (p);
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
  return np;
}

static int
add_plow (DataTypePtr Data, LayerTypePtr layer, PourTypePtr pour,
          int type, void *ptr1, void *ptr2)
{
  POLYAREA *np = NULL, *pg = NULL, *tmp, *start_pg;
  POLYAREA *orig_poly;
  int count, count_all, count_added;

  switch (type)
    {
    case PIN_TYPE:
    case VIA_TYPE:
      np = get_unsubtract_pin_poly ((PinTypePtr) ptr2, layer, pour);
      break;
    case LINE_TYPE:
      np = get_unsubtract_line_poly ((LineTypePtr) ptr2, layer, pour);
      break;
    case ARC_TYPE:
      np = get_unsubtract_arc_poly ((ArcTypePtr) ptr2, layer, pour);
      break;
    case PAD_TYPE:
      np = get_unsubtract_pad_poly ((PadTypePtr) ptr2, layer, pour);
      break;
    case POLYGON_TYPE:
      np = get_unsubtract_polygon_poly ((PolygonTypePtr) ptr2, layer, pour);
      break;
    case POUR_TYPE:
#warning FIXME Later: Need to produce a function for this
      np = NULL;
      break;
    case TEXT_TYPE:
      np = get_unsubtract_text_poly ((TextTypePtr) ptr2, layer, pour);
      break;
    }

  if (np == NULL)
    {
      printf ("Didn't get a POLYAREA to add, so bailing\n");
      return 0;
    }

  orig_poly = original_pour_poly (pour);
  /* NB: orig_poly and old *pg are freed inside intersect_poly() */
  intersect_poly (orig_poly, &np);

  if (np == NULL)
    {
      printf ("POLYAREA to add got clipped away, so bailing\n");
      return 0;
    }

  assert (poly_Valid (np));

  /* Make pg contain the polygons we're going to fiddle with */

  count = 0;
  POURPOLYGON_LOOP (pour);
  {
    /* Gather up children which are touched by np */
    if (isects (np, polygon, False))
      {
        count++;
        /* Steal their clipped contours, then delete them */
        /* Add contour to local list to fiddle about with */

        assert (poly_Valid (polygon->Clipped));
        if (polygon->Clipped == NULL)
          {
            printf ("Got polygon->clipped == NULL!\n");
            continue;
          }
        if (pg == NULL)
          {
            pg = polygon->Clipped;
            polygon->Clipped = NULL;
          }
        else
          {
            /* Link the _single_ polygon->Clipped into our circular pg list. */
            polygon->Clipped->f = pg;
            polygon->Clipped->b = pg->b;
            pg->b->f = polygon->Clipped;
            pg->b = polygon->Clipped;
            polygon->Clipped = NULL;
          }
        /* POURPOLYGON_LOOP iterates backwards, so it's OK
         * to delete the current element we're sitting on */
        DestroyPolygonInPour (pour, polygon);
      }
  }
  END_LOOP;
//  printf ("Unsubtract counted %i touching children, now removed\n", count);

  if (pg == NULL)
    {
      printf ("Hmm, got pg == NULL in add_plow\n");
//      poly_Free (&np);
//      return -1;
    }

//  assert (poly_Valid (pg));

  /* Perform the union operation */
  /* NB: np and old *pg are freed inside union_poly() */
  unite_poly (np, &pg);


#if 0
  count = 0;
  { POLYAREA *pg_start;
  pg_start = pg;
  do {
    count++;
  } while ((pg = pg->f) != pg_start);
  }
  printf ("After unsubtract, counted %i polygon pieces\n", count);
#endif

  ClearPour (PCB->Data, layer, pour, &pg, (const BoxType *) ptr2, 2 * UNSUBTRACT_BLOAT);

  if (pg == NULL)
    {
      printf ("Poly killed to death somehow\n");
      return -1;
    }

  /* For each piece of the clipped up polygon, create a new child */
  count_all = count_added = 0;
  start_pg = pg;
  do
    {
      PolygonType *poly;

      tmp = pg->f;
      pg->f = pg;
      pg->b = pg;
      count_all++;
//      if (pg->contours->area > PCB->IsleArea)
      if (1) // Breaks incremental updates otherwise
        {
          count_added++;
          poly = CreateNewPolygonInPour (pour, pour->Flags);
          poly->Clipped = pg;
          CLEAR_FLAG (SELECTEDFLAG, poly);

          SetPolygonBoundingBox (poly);

          if (pour->polygon_tree == NULL)
            pour->polygon_tree = r_create_tree (NULL, 0, 0);
          r_insert_entry (pour->polygon_tree, (BoxType *) poly, 0);
        }
      else
        {
          printf ("Too small\n");
          poly_Free (&pg);
        }
    }
  while ((pg = tmp) != start_pg);

  mark_islands (Data, layer, pour, type, ptr1, ptr2);

//  printf ("ClearPour counted %i polygon pieces, and added the biggest %i\n", count_all, count_added);

  return 0;
}

/* ---------------------------------------------------------------------------------------------------------- */

int
InitPourClip (DataTypePtr Data, LayerTypePtr layer, PourType * pour)
{
  POLYAREA *pg, *tmp, *start_pg;
  int count_all, count_added;

  /* Free any children we might have */
  if (pour->PolygonN)
    {
      POURPOLYGON_LOOP (pour);
      {
        /* POURPOLYGON_LOOP iterates backwards, so it's OK
         * to delete the current element we're sitting on */
        DestroyPolygonInPour (pour, polygon);
      }
      END_LOOP;
    }

  pg = original_pour_poly (pour);
  if (!pg)
    {
      printf ("Clipping returned NULL - can that be good?\n");
      return 0;
    }
//  assert (poly_Valid (clipped));
  if (TEST_FLAG (CLEARPOLYFLAG, pour))
    {
      /* Clip the pour against anything we can find in this layer */
      ClearPour (Data, layer, pour, &pg, NULL, UNSUBTRACT_BLOAT);
    }

  if (pg == NULL)
    {
      printf ("Got pg == NULL for some reason\n");
      return 0;
    }

  count_all = count_added = 0;
  /* For each piece of the clipped up polygon, create a new child */
  start_pg = pg;
  do
    {
      PolygonType *poly;

      tmp = pg->f;
      pg->f = pg;
      pg->b = pg;

      count_all++;
//      if (pg->contours->area > PCB->IsleArea)
      if (1) // Breaks incremental updates otherwise
        {
          count_added++;
          poly = CreateNewPolygonInPour (pour, pour->Flags);
          poly->Clipped = pg;
          CLEAR_FLAG (SELECTEDFLAG, poly);

          SetPolygonBoundingBox (poly);

          if (pour->polygon_tree == NULL)
            pour->polygon_tree = r_create_tree (NULL, 0, 0);
          r_insert_entry (pour->polygon_tree, (BoxType *) poly, 0);
        }
      else
        {
          poly_Free (&pg);
        }
    }
  while ((pg = tmp) != start_pg);

  POURPOLYGON_LOOP (pour);
  {
    ASSIGN_FLAG (HOLEFLAG, IsPolygonAnIsland (layer, polygon), polygon);
  }
  END_LOOP;

  return 1;
}

struct plow_info
{
  int type;
  void *ptr1, *ptr2;
  LayerTypePtr layer;
  DataTypePtr data;
  int (*callback) (DataTypePtr, LayerTypePtr,
                   PourTypePtr, int, void *, void *);
};

static int
plow_callback (const BoxType * b, void *cl)
{
  struct plow_info *plow = (struct plow_info *) cl;
  PourTypePtr pour = (PourTypePtr) b;

  if (TEST_FLAG (CLEARPOLYFLAG, pour))
    return plow->callback (plow->data, plow->layer, pour,
                           plow->type, plow->ptr1, plow->ptr2);
  return 0;
}

int
PlowPours (DataType * Data, int type, void *ptr1, void *ptr2,
           int (*call_back) (DataTypePtr data, LayerTypePtr lay,
                             PourTypePtr poly, int type,
                             void *ptr1, void *ptr2),
           int ignore_clearflags)
{
  BoxType sb = ((PinTypePtr) ptr2)->BoundingBox;
  int r = 0;
  struct plow_info info;

  info.type = type;
  info.ptr1 = ptr1;
  info.ptr2 = ptr2;
  info.data = Data;
  info.callback = call_back;
  switch (type)
    {
    case PIN_TYPE:
    case VIA_TYPE:
      if (type == PIN_TYPE || ptr1 == ptr2 || ptr1 == NULL)
        {
          LAYER_LOOP (Data, max_layer);
          {
            info.layer = layer;
            r += r_search (layer->pour_tree, &sb, NULL, plow_callback, &info);
          }
          END_LOOP;
        }
      else
        {
          int layer_no = GetLayerNumber (Data, ((LayerTypePtr) ptr1));
          int group_no = GetLayerGroupNumberByNumber (layer_no);
          GROUP_LOOP (Data, group_no);
          {
            info.layer = layer;
            r += r_search (layer->pour_tree, &sb, NULL, plow_callback, &info);
          }
          END_LOOP;
        }
      break;
    case LINE_TYPE:
    case ARC_TYPE:
    case TEXT_TYPE:
    case POLYGON_TYPE:
    case POUR_TYPE:
      /* the cast works equally well for lines and arcs */
      if (!ignore_clearflags &&
          !TEST_FLAG (CLEARLINEFLAG, (LineTypePtr) ptr2))
        return 0;
      /* silk doesn't plow */
      if (GetLayerNumber (Data, ptr1) >= max_layer)
        return 0;
      GROUP_LOOP (Data, GetLayerGroupNumberByNumber (GetLayerNumber (Data,
                                                                     ((LayerTypePtr) ptr1))));
      {
        info.layer = layer;
        r += r_search (layer->pour_tree, &sb, NULL, plow_callback, &info);
      }
      END_LOOP;
      break;
    case PAD_TYPE:
      {
        Cardinal group = TEST_FLAG (ONSOLDERFLAG,
                                    (PadType *) ptr2) ? SOLDER_LAYER :
          COMPONENT_LAYER;
        group = GetLayerGroupNumberByNumber (max_layer + group);
        GROUP_LOOP (Data, group);
        {
          info.layer = layer;
          r += r_search (layer->pour_tree, &sb, NULL, plow_callback, &info);
        }
        END_LOOP;
      }
      break;

    case ELEMENT_TYPE:
      {
        PIN_LOOP ((ElementType *) ptr1);
        {
          PlowPours (Data, PIN_TYPE, ptr1, pin, call_back, ignore_clearflags);
        }
        END_LOOP;
        PAD_LOOP ((ElementType *) ptr1);
        {
          PlowPours (Data, PAD_TYPE, ptr1, pad, call_back, ignore_clearflags);
        }
        END_LOOP;
      }
      break;
    }
  return r;
}

void
RestoreToPours (DataType * Data, int type, void *ptr1, void *ptr2)
{
  if (!Data->ClipPours)
    return;
  if (type == POUR_TYPE)
    {
#warning FIXME Later: Why do we need to do this?
//      printf ("Calling InitPourClip from RestoreToPour\n");
      InitPourClip (PCB->Data, (LayerTypePtr) ptr1, (PourTypePtr) ptr2);
    }
  PlowPours (Data, type, ptr1, ptr2, add_plow, False);
}

void
ClearFromPours (DataType * Data, int type, void *ptr1, void *ptr2)
{
  if (!Data->ClipPours)
    return;
  if (type == POUR_TYPE)
    {
#warning FIXME Later: Why do we need to do this?
//      printf ("Calling InitPourClip from ClearFromPour\n");
      InitPourClip (PCB->Data, (LayerTypePtr) ptr1, (PourTypePtr) ptr2);
    }
  PlowPours (Data, type, ptr1, ptr2, subtract_plow, False);
}

#warning FIXME Later: We could perhaps reduce un-necessary computation by using this function
void
MarkPourIslands (DataType * Data, int type, void *ptr1, void *ptr2)
{
  if (!Data->ClipPours)
    return;
  PlowPours (Data, type, ptr1, ptr2, mark_islands, True);
}
