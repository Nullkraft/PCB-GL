/*
       polygon clipping functions. harry eaton implemented the algorithm
       described in "A Closed Set of Algorithms for Performing Set
       Operations on Polygonal Regions in the Plane" which the original
       code did not do. I also modified it for integer coordinates
       and faster computation. The license for this modified copy was
       switched to the GPL per term (3) of the original LGPL license.
       Copyright (C) 2006 harry eaton
 
   based on:
       poly_Boolean: a polygon clip library
       Copyright (C) 1997  Alexey Nikitin, Michael Leonov
       (also the authors of the paper describing the actual algorithm)
       leonov@propro.iis.nsk.su

   in turn based on:
       nclip: a polygon clip library
       Copyright (C) 1993  Klamer Schutte
 
       This program is free software; you can redistribute it and/or
       modify it under the terms of the GNU General Public
       License as published by the Free Software Foundation; either
       version 2 of the License, or (at your option) any later version.
 
       This program is distributed in the hope that it will be useful,
       but WITHOUT ANY WARRANTY; without even the implied warranty of
       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
       General Public License for more details.
 
       You should have received a copy of the GNU General Public
       License along with this program; if not, write to the Free
       Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
      polygon1.c
      (C) 1997 Alexey Nikitin, Michael Leonov
      (C) 1993 Klamer Schutte

      all cases where original (Klamer Schutte) code is present
      are marked
*/

#include	<assert.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<setjmp.h>
#include	<math.h>
#include	<string.h>

#include "global.h"
#include "rtree.h"
#include "heap.h"
#include "pcb-printf.h"
#include "misc.h"
#include "circle_line_intersect.h"
#include "circle_circle_intersect.h"

#define ROUND(a) (long)((a) > 0 ? ((a) + 0.5) : ((a) - 0.5))

#define EPSILON (1E-6)
#define IsZero(a, b) (fabs((a) - (b)) < EPSILON)

/*********************************************************************/
/*              L o n g   V e c t o r   S t u f f                    */
/*********************************************************************/

#define Vcopy(a,b) {(a)[0]=(b)[0];(a)[1]=(b)[1];}
int vect_equal (Vector v1, Vector v2);
void vect_init (Vector v, double x, double y);
void vect_sub (Vector res, Vector v2, Vector v3);

void vect_min (Vector res, Vector v2, Vector v3);
void vect_max (Vector res, Vector v2, Vector v3);

double vect_dist2 (Vector v1, Vector v2);
double vect_det2 (Vector v1, Vector v2);
double vect_len2 (Vector v1);

int vect_inters2 (Vector p1, Vector p2, double s1, double s2,
                  Vector q1, Vector q2, double t1, double t2,
                  Vector i1, Vector i2,
                  double *i1s, double *i1t, double *i2s, double *i2t);

static void poly_InclVertex_int (VNODE * after, VNODE * node);

/* note that a vertex v's Flags.status represents the edge defined by
 * v to v->next (i.e. the edge is forward of v)
 */

/* Some macros which will hopefully aid readability of the code which
 * traverses edges and vertices..
 */
#define VERTEX_FORWARD_EDGE(v) ((v))
#define VERTEX_BACKWARD_EDGE(v) ((v)->prev)
#define EDGE_FORWARD_VERTEX(e) ((e)->next)
#define EDGE_BACKWARD_VERTEX(e) ((e))
#define NEXT_VERTEX(v) ((v)->next)
#define PREV_VERTEX(v) ((v)->prev)
#define NEXT_EDGE(e) ((e)->next)
#define PREV_EDGE(e) ((e)->prev)
#define VERTEX_SIDE_DIR_EDGE(v,s) (((s) == 'P') ? VERTEX_BACKWARD_EDGE (v) : VERTEX_FORWARD_EDGE (v)) /* Move backwards for 'P' side, forwards for 'N' */
#define EDGE_SIDE_DIR_VERTEX(e,s) (((s) == 'P') ? EDGE_BACKWARD_VERTEX (e) : EDGE_FORWARD_VERTEX (e)) /* Move backwards for 'P' side, forwards for 'N' */
#define VERTEX_DIRECTION_EDGE(v,d) (((d) == FORW) ? VERTEX_FORWARD_EDGE (v) : VERTEX_BACKWARD_EDGE (v)) /* Move backwards for BACKW, forwards for FORW */
#define EDGE_DIRECTION_VERTEX(e,d) (((d) == FORW) ? EDGE_FORWARD_VERTEX (e) : EDGE_BACKWARD_VERTEX (e)) /* Move backwards for BACKW, forwards for FORW */

#define ISECTED 3
#define UNKNWN  0
#define INSIDE  1
#define OUTSIDE 2
#define SHARED 3
#define SHARED2 4

#define TOUCHES 99

#define EDGE_LABEL(e)  ((e)->Flags.status)
#define LABEL_EDGE(e,l) ((e)->Flags.status = (l))

#define error(code)  longjmp(*(e), code)

//#define DEBUG_INTERSECT
#undef DEBUG_LABEL
#define DEBUG_ALL_LABELS
//#define DEBUG_JUMP
//#define DEBUG_GATHER
#undef DEBUG_ANGLE
#define DEBUG
#ifdef DEBUG
#define DEBUGP(...) pcb_fprintf(stderr, ## __VA_ARGS__)
#else
#define DEBUGP(...)
#endif

/* ///////////////////////////////////////////////////////////////////////////// * /
/ *  2-Dimentional stuff
/ * ///////////////////////////////////////////////////////////////////////////// */

#define Vsub2(r,a,b)	{(r)[0] = (a)[0] - (b)[0]; (r)[1] = (a)[1] - (b)[1];}
#define Vadd2(r,a,b)	{(r)[0] = (a)[0] + (b)[0]; (r)[1] = (a)[1] + (b)[1];}
#define Vsca2(r,a,s)	{(r)[0] = (a)[0] * (s); (r)[1] = (a)[1] * (s);}
#define Vcpy2(r,a)		{(r)[0] = (a)[0]; (r)[1] = (a)[1];}
#define Vequ2(a,b)		((a)[0] == (b)[0] && (a)[1] == (b)[1])
#define Vadds(r,a,b,s)	{(r)[0] = ((a)[0] + (b)[0]) * (s); (r)[1] = ((a)[1] + (b)[1]) * (s);}
#define Vswp2(a,b) { long t; \
	t = (a)[0], (a)[0] = (b)[0], (b)[0] = t; \
	t = (a)[1], (a)[1] = (b)[1], (b)[1] = t; \
}

static POLYPARENTAGE no_parentage = {
  .immaculate_conception = true,
  .action = PBO_NONE,
  .a = NULL,
  .b = NULL
};

static char *theState (VNODE * v);

/* static */ void
pline_dump (VNODE * v)
{
  VNODE *s, *n;

  s = v;
  do
    {
      n = NEXT_VERTEX(v);
      pcb_fprintf (stderr, "Line [%$#mn %$#mn %$#mn %$#mn 10 10 \"%s\"] # %s, radius %mn\n",
	       v->point[0], v->point[1],
	       n->point[0], n->point[1], theState (v),
	       VERTEX_FORWARD_EDGE (v)->is_round ? "Round" : "Line",
	       VERTEX_FORWARD_EDGE (v)->is_round ? v->radius : 0);
    }
  while ((v = NEXT_VERTEX(v)) != s);
}

/*static */void
poly_dump (POLYAREA * p)
{
  POLYAREA *f = p;
  PLINE *pl;

  do
    {
      pl = p->contours;
      do
        {
          pline_dump (&pl->head);
          fprintf (stderr, "NEXT PLINE\n");
        }
      while ((pl = pl->next) != NULL);
      fprintf (stderr, "NEXT POLY\n");
    }
  while ((p = p->f) != f);
}

static VNODE *
poly_CreateNodeFull (Vector v, bool is_round, double cx, double cy, double radius)
//poly_CreateNodeFull (Vector v, bool is_round, Coord cx, Coord cy, Coord radius)
{
  VNODE *res;
  Coord *c;

  assert (v);
  res = g_slice_new0 (VNODE);
  if (res == NULL)
    return NULL;

  c = res->point;
  *c++ = *v++;
  *c = *v;

  res->orig_point0[0] = res->point[0];
  res->orig_point0[1] = res->point[1];
  res->p0 = 0.0;
  res->p1 = 1.0;

  res->is_round = is_round;
  res->cx = cx;
  res->cy = cy;
  res->radius = radius;

  return res;
}

VNODE *
poly_CreateNode (Vector v)
{
  return poly_CreateNodeFull (v, false, 0, 0, 0);
}

VNODE *
//poly_CreateNodeArcApproximation (Vector v, Coord cx, Coord cy, Coord radius)
poly_CreateNodeArcApproximation (Vector v, double cx, double cy, double radius)
{
//  return poly_CreateNodeFull (v, false /*true*/, cx, cy, radius);
  return poly_CreateNodeFull (v, true, cx, cy, radius);
}

/***************************************************************/
/* routines for processing intersections */

/*
node_add
 (C) 1993 Klamer Schutte
 (C) 1997 Alexey Nikitin, Michael Leonov
 (C) 2006 harry eaton

 returns a bit field in new_point that indicates where the
 point was.
 1 means a new node was created and inserted
 4 means the intersection was not on the dest point
*/
/* dest is considered an edge */
static VNODE *
node_add_single (VNODE * dest, Vector po)
{
  VNODE *p;

  pcb_printf ("    New node is %f from previous, %f from next",
              vect_dist2 (po, dest->point),
              vect_dist2 (po, dest->next->point));

#warning NOT SURE WHAT A SENSIBLE EPSILON IN INTEGER NANOMETERS IS - INCORRECT NUMBERS ONE WAY OR THE OTHER CAUSE BREAKAGE
//#define DIST_EPSILON 3.
#define DIST_EPSILON 1.
  if (vect_dist2 (po, EDGE_BACKWARD_VERTEX (dest)->point) < DIST_EPSILON)
    {
      printf (" - (TOUCHING)\n");
      return EDGE_BACKWARD_VERTEX (dest);
    }
  if (vect_dist2 (po, EDGE_FORWARD_VERTEX (dest)->point) < DIST_EPSILON)
    {
      printf (" - (TOUCHING)\n");
      return EDGE_FORWARD_VERTEX (dest);
    }
#undef DIST_EPSILON

  printf (" - (CREATING NEW NODE AND EDGE)\n");

  p = poly_CreateNodeFull (po, dest->is_round, dest->cx, dest->cy, dest->radius);
  pcb_printf (" %p (%mm, %mm)\n", p, po[0], po[1]);
  if (p == NULL)
    return NULL;

  p->orig_point0[0] = dest->orig_point0[0];
  p->orig_point0[1] = dest->orig_point0[1];
  p->orig_point1[0] = dest->orig_point1[0];
  p->orig_point1[1] = dest->orig_point1[1];
  /* NB: p->p0 and p->p1 remain incorrect (defaulting to 0.0 and 1.0 until the node is fully inserted) */

  p->cvc_prev = p->cvc_next = NULL;
  p->Flags.status = UNKNWN;
  return p;
}				/* node_add */

#define ISECT_BAD_PARAM (-1)
#define ISECT_NO_MEMORY (-2)

/*
 * Determine whether the angle swept by a-b to a-c is
 * counter clockwise, clockwise or zero
 */
static int compare_ccw_cw (Vector a, Vector b, Vector c)
{
  double cross;
  Vector ab;
  Vector ac;

  Vsub2 (ab, b, a);
  Vsub2 (ac, c, a);

  cross = (double) ab[0] * ac[1] - (double) ac[0] * ab[1];
  if (cross > 0.0)
    return 1;
  else if (cross < 0.0)
    return -1;
  else
    return 0;
}

/*
new_descriptor
  (C) 2006 harry eaton
*/
static CVCList *
new_descriptor (VNODE * a, char poly, char side)
{
  CVCList *l = (CVCList *) malloc (sizeof (CVCList));
  Vector v;
  register double ang, dx, dy;

  if (!l)
    return NULL;
  l->head = NULL;
  l->parent = a;
  l->poly = poly;
  l->side = side;
  l->next = l->prev = l;
  if (VERTEX_SIDE_DIR_EDGE (a, side)->is_round == false)  /* not round */
    { /* Line-segment case */

#if 0
      if (side == 'P')		/* previous */
        vect_sub (v, PREV_VERTEX (a)->point, a->point);
      else				/* next */
        vect_sub (v, NEXT_VERTEX (a)->point, a->point);
#else
      /* XXX: Not entirely sure about these */
      if (side == 'P')		/* previous */
        vect_sub (v, a->prev->orig_point0, a->prev->orig_point1);
      else				/* next */
        vect_sub (v, a->orig_point1, a->orig_point0);
#endif

      /* Uses slope/(slope+1) in quadrant 1 as a proxy for the angle.
       * It still has the same monotonic sort result
       * and is far less expensive to compute than the real angle.
       */
      if (vect_equal (v, vect_zero))
        {
          printf ("OH DEAR.. THIS ISN'T GOING TO END WELL\n");
          *(char *)0 = 0;
          if (side == 'P')
            {
              if (PREV_VERTEX (a)->cvc_prev == (CVCList *) - 1)
                PREV_VERTEX (a)->cvc_prev = PREV_VERTEX (a)->cvc_next = NULL;
              poly_ExclVertex (PREV_VERTEX (a));
              vect_sub (v, PREV_VERTEX (a)->point, a->point);
#warning DOES THIS LEAK A VERTEX?
            }
          else
            {
              if (NEXT_VERTEX (a)->cvc_prev == (CVCList *) - 1)
                NEXT_VERTEX (a)->cvc_prev = NEXT_VERTEX (a)->cvc_next = NULL;
              poly_ExclVertex (NEXT_VERTEX (a));
              vect_sub (v, NEXT_VERTEX (a)->point, a->point);
#warning DOES THIS LEAK A VERTEX?
            }
        }

      l->curvature = 0.0;

    }
  else
    { /* Arc segment case */

      Vector center;
      double radial_dx;

      /* Compute tangent to arc by rotating radial line between
       * the relevant endpoint and its center through 90 degrees
       * CCW. When operating in the "P" side, we then flip this
       * vector. (Alternatively, the radial line is moved through
       * 90 degrees CW)
       */

      center[0] = VERTEX_SIDE_DIR_EDGE (a, side)->cx;
      center[1] = VERTEX_SIDE_DIR_EDGE (a, side)->cy;
      l->curvature = (double)-compare_ccw_cw (a->point, center, EDGE_SIDE_DIR_VERTEX (VERTEX_SIDE_DIR_EDGE (a, side), side)->point) / VERTEX_SIDE_DIR_EDGE (a, side)->radius;
#if 0
      if (side == 'P')		/* previous */
        {
          l->curvature = (double)+compare_ccw_cw (EDGE_BACKWARD_VERTEX (VERTEX_BACKWARD_EDGE (a))->point, center, a->point) / VERTEX_BACKWARD_EDGE (a)->radius;
          // OR: ?
          // l->curvature = (double)-compare_ccw_cw (a->point, center, a->prev->point) / a->prev->radius;
        }
      else				/* next */
        {
          l->curvature = (double)-compare_ccw_cw (a->point, center, EDGE_FORWARD_VERTEX (VERTEX_FORWARD_EDGE (a))->point) / VERTEX_FORWARD_EDGE (a)->radius;
        }
#endif

      /* First of all, make v the radial line */
      vect_sub (v, center, a->point);

      if (vect_equal (v, vect_zero))
        {
          printf ("OH DEAR.. THIS ISN'T GOING TO END WELL - ARC CASE\n");
#warning NO CORRECTIVE ACTION FOR THIS CASE AT THE MOMENT, JUST FORCE A CRASH
          *(char *)0 = 0;
        }

      /* Now rotate it through 90 to give the curve tangent */
      radial_dx = v[0];
      v[0] = -v[1];
      v[1] = radial_dx;

#if 0
      /* Flip the vector for the previous case */
      if (side == 'P')		/* previous */
        {
          v[0] = -v[0];
          v[1] = -v[1];
        }
#endif
      /* Flip the vector for the previous case */
      if ((side == 'P' && compare_ccw_cw (a->prev->point, center, a      ->point) ==  1) ||
          (side != 'P' && compare_ccw_cw (a      ->point, center, a->next->point) == -1))
        {
          v[0] = -v[0];
          v[1] = -v[1];
        }
    }
  assert (!vect_equal (v, vect_zero));
  dx = fabs ((double) v[0]);
  dy = fabs ((double) v[1]);
  ang = dy / (dy + dx);
  /* now move to the actual quadrant */
  if (v[0] < 0 && v[1] >= 0)
    ang = 2.0 - ang;		/* 2nd quadrant */
  else if (v[0] < 0 && v[1] < 0)
    ang += 2.0;			/* 3rd quadrant */
  else if (v[0] >= 0 && v[1] < 0)
    ang = 4.0 - ang;		/* 4th quadrant */
  l->angle = ang;
  assert (ang >= 0.0 && ang <= 4.0);
#ifdef DEBUG_ANGLE
  DEBUGP ("node on %c at %#mD assigned angle %g on side %c\n", poly,
	  a->point[0], a->point[1], ang, side);
#endif
  return l;
}

/*
 * Compare the edge angles (and curvatures) to determine
 * the ordering of two edges around a vertex.
 *
 * Returns <0 (ie -1) for a < b
 * Returns =0         for a = b
 * Returns >1 (ie +1) for a > b
 */
static int compare_cvc_nodes (CVCList *a, CVCList *b)
{
  if (a->angle < b->angle)
    return -1;
  else if (a->angle > b->angle)
    return 1;
  else if (a->curvature < b->curvature)
    return -1;
  else if (a->curvature > b->curvature)
    return 1;
  else
    return 0;
}

/*
insert_descriptor
  (C) 2006 harry eaton

   argument a is a cross-vertex node.
   argument poly is the polygon it comes from ('A' or 'B')
   argument side is the side this descriptor goes on ('P' for previous
   'N' for next.
   argument start is the head of the list of cvclists
*/
static CVCList *
insert_descriptor (VNODE * a, char poly, char side, CVCList * start)
{
  CVCList *l, *newone, *big, *small;

  if (!(newone = new_descriptor (a, poly, side)))
    return NULL;
  /* search for the CVCList for this point */
  if (!start)
    {
      start = newone;		/* return is also new, so we know where start is */
      start->head = newone;	/* circular list */
      return newone;
    }
  else
    {
      l = start;
      do
	{
	  assert (l->head);
	  if (l->parent->point[0] == a->point[0]
	      && l->parent->point[1] == a->point[1])
	    {			/* this CVCList is at our point */
	      start = l;
	      newone->head = l->head;
	      break;
	    }
          // Next two lines equivelant to:   if (l->head == start)    ???
	  if (l->head->parent->point[0] == start->parent->point[0]
	      && l->head->parent->point[1] == start->parent->point[1])
	    {
	      /* this seems to be a new point */
	      /* link this cvclist to the list of all cvclists */
	      for (; l->head != newone; l = l->next)
		l->head = newone;
	      newone->head = start;
	      return newone;
	    }
	  l = l->head;
	}
      while (1);
    }
  assert (start);
  l = big = small = start;
  do
    {
      if (compare_cvc_nodes (l->next, l) < 0)
	{
	  small = l->next;
	  big = l;
	}
      else if (compare_cvc_nodes (newone, l) >= 0 &&
               compare_cvc_nodes (newone, l->next) <= 0)
	{
	  /* insert new cvc if it lies between existing points */
	  newone->prev = l;
	  newone->next = l->next;
	  l->next = l->next->prev = newone;
	  return newone;
	}
    }
  while ((l = l->next) != start);
  /* didn't find it between points, it must go on an end */

#if 0
  /* XXX: DUH.. BOTH OF THESE CODE-PATHS BELOW ARE EQUIVELANT.. INSERT AFTER big, or BEFORE small.
   *      The list rolls around, so big->next == small.
   */
  if (big->angle <= newone->angle)
    {
      newone->prev = big;
      newone->next = big->next;
      big->next = big->next->prev = newone;
      return newone;
    }
  assert (small->angle >= newone->angle);
#endif
  newone->next = small;
  newone->prev = small->prev;
  small->prev = small->prev->next = newone;
  return newone;
}

/*
node_add_point
 (C) 1993 Klamer Schutte
 (C) 1997 Alexey Nikitin, Michael Leonov

 return 1 if new node in b, 2 if new node in a and 3 if new node in both
*/
/* a is considered an edge */

static VNODE *
node_add_single_point (VNODE * a, Vector p)
{
  VNODE *a_backward_vertex, *a_forward_vertex, *new_node;

  a_backward_vertex = EDGE_BACKWARD_VERTEX (a);
  a_forward_vertex = EDGE_FORWARD_VERTEX (a);

  new_node = node_add_single (a, p);
  assert (new_node != NULL);

  new_node->cvc_prev = new_node->cvc_next = (CVCList *) - 1;

  if (new_node == a_backward_vertex || new_node == a_forward_vertex)
    return NULL;

  return new_node;
}				/* node_add_point */


/* \brief Compare the curvature / geometries of two edges
 *
 * \return true if the geometry of the two edges match
 */
static bool
compare_edge_arcs_equal (VNODE *a, VNODE *b)
{
  return a->is_round == b->is_round &&
         a->radius   == b->radius   &&
         a->cx       == b->cx       &&
         a->cy       == b->cy;
}

/*
edge_label
 (C) 2006 harry eaton
 (C) 2016 Peter Clifton
*/
/* pn is considered an edge */
static unsigned int
edge_label (VNODE * pn)
{
  CVCList *first_l, *l;
  char this_poly;
  int region;
  bool shared_edge_case = false;

  /* search counter-clockwise in the cross vertex connectivity (CVC) list
   *
   * check for shared edges (that could be prev or next in the list since the angles are equal)
   * and check if this edge (pn -> pn->next) is found between the other poly's entry and exit
   */

  /* Start with l pointing to the CVCNode corresponding to this edge leaving its from vertex */
  assert (pn);
  l = EDGE_BACKWARD_VERTEX (pn)->cvc_next;

  assert (l);
  this_poly = l->poly;

  /* Shared edges can be sorted in either order, so need to check l->prev as well */
  if (compare_cvc_nodes (l, l->prev) == 0)
    {
      shared_edge_case = true;
      l = l->prev;
    }
  else
    {
      if (compare_cvc_nodes (l, l->next) == 0)
        shared_edge_case = true;

      /* Both the shared with next, or general non-shared cases need l = l->next */
      l = l->next;
    }

  if (shared_edge_case)
    {
      /* Should be the shared edge case.. but we will make a few checks to be sure! */

      /* If this fires, we found a hairline edge pair within our own polygon, as no edge
       * from the same polygon should compare identically in the CVCList
       */
      assert (l->poly != this_poly);

      /* If this fires, we found two geometrically distinct edges which for some reason compare as equal in our cvc_list.
       * Shared edges should be geometrically identical (but may be in opposite directions).
       */
      assert (EDGE_SIDE_DIR_VERTEX (VERTEX_SIDE_DIR_EDGE (l->parent, l->side), l->side)->point[0] == EDGE_FORWARD_VERTEX (pn)->point[0] &&
              EDGE_SIDE_DIR_VERTEX (VERTEX_SIDE_DIR_EDGE (l->parent, l->side), l->side)->point[1] == EDGE_FORWARD_VERTEX (pn)->point[1] &&
              compare_edge_arcs_equal (VERTEX_SIDE_DIR_EDGE (l->parent, l->side), pn));

      /* SHARED is the same direction case,
       * SHARED2 is the opposite direction case.
       */
      region = (l->side == 'P') ? SHARED2 : SHARED;
      pn->shared = VERTEX_SIDE_DIR_EDGE (l->parent, l->side);
    }
  else
    {
      first_l = l;
      /* Skip edges unil we find one from the next polygon */
      while ((l->poly == this_poly) && (l != first_l->prev))
        {
          /* Check for hairline pairs of edges in the CVCList, they may be sorted in incorrect order,
           * and would thus mislead as to whether we are inside or outside a given contour. It is a
           * bug if such edges are present, so test for it here where we may detect it. We compare
           * l->prev and l, as we know both are still in this_poly.. l->next may not be.
           */
          assert (compare_cvc_nodes (l->prev, l) != 0);

          l = l->next;
        }

      /* If this fires, we must have wrapped around the entire CVCList wihthout finding any edges from
       * the other polygon.
       */
      assert (l->poly != this_poly);

      /* Check the other polygon edge we landed on in the CVCList is not a hairline edge pair
       * from the same polygon. If so, they may be sorted in incorrect order and would thus
       * mislead as to whether we are inside or outside that contour. It is a bug if such edges
       * are present.
       */
      assert (l->poly != l->next->poly || compare_cvc_nodes (l, l->next) != 0);

      region = (l->side == 'P') ? INSIDE : OUTSIDE;
    }

  assert (EDGE_LABEL (pn) == UNKNWN || EDGE_LABEL (pn) == region);
  LABEL_EDGE (pn, region);
  if (region == SHARED || region == SHARED2)
    return UNKNWN;
  return region;
}				/* edge_label */

/*
 add_descriptors
 (C) 2006 harry eaton
*/
static CVCList *
add_descriptors (PLINE * pl, char poly, CVCList * list)
{
  VNODE *node = &pl->head; /* node is considered a vertex */

  do
    {
      if (node->cvc_prev)
	{
	  assert (node->cvc_prev == (CVCList *) - 1
		  && node->cvc_next == (CVCList *) - 1);
	  list = node->cvc_prev = insert_descriptor (node, poly, 'P', list);
	  if (!node->cvc_prev)
	    return NULL;
	  list = node->cvc_next = insert_descriptor (node, poly, 'N', list);
	  if (!node->cvc_next)
	    return NULL;
	}
    }
  while ((node = NEXT_VERTEX(node)) != &pl->head);
  return list;
}

static bool
cntrbox_check (PLINE * c, Vector p)
{
  return (p[0]     < c->xmin ||
          p[0] + 1 > c->xmax ||
          p[1]     < c->ymin ||
          p[1] + 1 > c->ymax);
}

static inline void
cntrbox_adjust (PLINE * c, Vector p)
{
  c->xmin = min (c->xmin, p[0]);
  c->xmax = max (c->xmax, p[0] + 1);
  c->ymin = min (c->ymin, p[1]);
  c->ymax = max (c->ymax, p[1] + 1);
}

/* some structures for handling segment intersections using the rtrees */

typedef struct seg
{
  BoxType box;
  VNODE *v;
  PLINE *p;
  int intersected;
} seg;

typedef struct _insert_node_task insert_node_task;

struct _insert_node_task
{
  insert_node_task *next;
  seg * node_seg;
  VNODE *new_node;
  double param;
};

typedef struct info
{
  double m, b;
  rtree_t *tree;
  VNODE *v;
  struct seg *s;
  jmp_buf *env, sego, *touch;
//  int need_restart;
  insert_node_task *node_insert_list;
} info;

typedef struct contour_info
{
  PLINE *pa;
  jmp_buf restart;
  jmp_buf *getout;
//  int need_restart;
  insert_node_task *node_insert_list;
} contour_info;


/*
 * adjust_tree()
 * (C) 2006 harry eaton
 * This replaces the segment in the tree with the two new segments after
 * a vertex has been added
 */
static int
adjust_tree (rtree_t * tree, struct seg *s)
{
  struct seg *q;

  q = (seg *)malloc (sizeof (struct seg));
  if (!q)
    return 1;
  q->intersected = 0;
  q->v = s->v;
  q->p = s->p;
  q->box.X1 = min (EDGE_BACKWARD_VERTEX (q->v)->point[0], EDGE_FORWARD_VERTEX (q->v)->point[0]);
  q->box.X2 = max (EDGE_BACKWARD_VERTEX (q->v)->point[0], EDGE_FORWARD_VERTEX (q->v)->point[0]) + 1;
  q->box.Y1 = min (EDGE_BACKWARD_VERTEX (q->v)->point[1], EDGE_FORWARD_VERTEX (q->v)->point[1]);
  q->box.Y2 = max (EDGE_BACKWARD_VERTEX (q->v)->point[1], EDGE_FORWARD_VERTEX (q->v)->point[1]) + 1;

  if (q->v->is_round)
    {
      Angle start_angle;
      Angle end_angle;
      Angle delta_angle;
      BoxType arc_bound;

      start_angle = atan2 ((EDGE_BACKWARD_VERTEX (q->v)->point[1] - q->v->cy), -(EDGE_BACKWARD_VERTEX (q->v)->point[0] - q->v->cx)) / M180;
      end_angle   = atan2 (( EDGE_FORWARD_VERTEX (q->v)->point[1] - q->v->cy), -( EDGE_FORWARD_VERTEX (q->v)->point[0] - q->v->cx)) / M180;

#warning delta angle calculation looks rather suspect - wont work for arcs > 180 degrees span
      delta_angle = end_angle - start_angle;

      if (delta_angle > 180.) delta_angle -= 360.;
      if (delta_angle < -180.) delta_angle += 360.;

      arc_bound = calc_thin_arc_bounds (q->v->cx, q->v->cy, q->v->radius, q->v->radius, start_angle, delta_angle);

      MAKEMIN (q->box.X1, arc_bound.X1);
      MAKEMIN (q->box.Y1, arc_bound.Y1);
      MAKEMAX (q->box.X2, arc_bound.X2);
      MAKEMAX (q->box.Y2, arc_bound.Y2);
    }

  r_insert_entry (tree, (const BoxType *) q, 1);
  q = (seg *)malloc (sizeof (struct seg));
  if (!q)
    return 1;
  q->intersected = 0;
  q->v = NEXT_EDGE (s->v);
  q->p = s->p;
  q->box.X1 = min (EDGE_BACKWARD_VERTEX (q->v)->point[0], EDGE_FORWARD_VERTEX (q->v)->point[0]);
  q->box.X2 = max (EDGE_BACKWARD_VERTEX (q->v)->point[0], EDGE_FORWARD_VERTEX (q->v)->point[0]) + 1;
  q->box.Y1 = min (EDGE_BACKWARD_VERTEX (q->v)->point[1], EDGE_FORWARD_VERTEX (q->v)->point[1]);
  q->box.Y2 = max (EDGE_BACKWARD_VERTEX (q->v)->point[1], EDGE_FORWARD_VERTEX (q->v)->point[1]) + 1;

  if (q->v->is_round)
    {
      Angle start_angle;
      Angle end_angle;
      Angle delta_angle;
      BoxType arc_bound;

      start_angle = atan2 ((EDGE_BACKWARD_VERTEX (q->v)->point[1] - q->v->cy), -(EDGE_BACKWARD_VERTEX (q->v)->point[0] - q->v->cx)) / M180;
      end_angle   = atan2 (( EDGE_FORWARD_VERTEX (q->v)->point[1] - q->v->cy), -( EDGE_FORWARD_VERTEX (q->v)->point[0] - q->v->cx)) / M180;

#warning delta angle calculation looks rather suspect - wont work for arcs > 180 degrees span
      delta_angle = end_angle - start_angle;

      if (delta_angle > 180.) delta_angle -= 360.;
      if (delta_angle < -180.) delta_angle += 360.;

      arc_bound = calc_thin_arc_bounds (q->v->cx, q->v->cy, q->v->radius, q->v->radius, start_angle, delta_angle);

      MAKEMIN (q->box.X1, arc_bound.X1);
      MAKEMIN (q->box.Y1, arc_bound.Y1);
      MAKEMAX (q->box.X2, arc_bound.X2);
      MAKEMAX (q->box.Y2, arc_bound.Y2);
    }

  r_insert_entry (tree, (const BoxType *) q, 1);
  r_delete_entry (tree, (const BoxType *) s);
  return 0;
}

#if 0  /* TURNS OUT DISABLING THIS ACTUALLY (VERY MARGINALLY) SPEEDS THINGS UP! */
/*
 * seg_in_region()
 * (C) 2006, harry eaton
 * This prunes the search for boxes that don't intersect the segment.
 */
static int
seg_in_region (const BoxType * b, void *cl)
{
  struct info *i = (struct info *) cl;
  double y1, y2;
  /* for zero slope the search is aligned on the axis so it is already pruned */
  if (i->m == 0.)
    return 1;
  y1 = i->m * b->X1 + i->b;
  y2 = i->m * b->X2 + i->b;
  if (min (y1, y2) >= b->Y2)
    return 0;
  if (max (y1, y2) < b->Y1)
    return 0;
  return 1;			/* might intersect */
}
#endif

/* Prepend a deferred node-insersion task to a list */
static insert_node_task *
prepend_insert_node_task (insert_node_task *list, seg *seg, VNODE *new_node, double param)
{
  insert_node_task *task = (insert_node_task *)malloc (sizeof (*task));
  task->node_seg = seg;
  task->new_node = new_node;
  task->next = list;
  task->param = param;
  return task;
}

static bool
insert_vertex_in_seg (struct info *i, struct seg *s, Vector v, double param)
{
  VNODE *new_node = node_add_single_point (s->v, v);
  if (new_node == NULL)
    return false;

#ifdef DEBUG_INTERSECT
  DEBUGP ("new intersection on segment \"i\" at %#mD\n", v[0], v[1]);
#endif
  i->node_insert_list = prepend_insert_node_task (i->node_insert_list, s, new_node, param);
  s->intersected = 1;
  return true;
}

static int
seg_in_seg_line_line (struct info *i, struct seg *s1, struct seg *s2)
{
  Vector v1, v2;
  int cnt;
  double s1_i1;
  double s2_i1;
  double s1_i2;
  double s2_i2;

  assert (!s1->v->is_round);
  assert (!s2->v->is_round);

  printf ("Intersecting\n");
  pcb_printf ("  Line   %p (%mm, %mm)-(%mm, %mm)\n",
              s1->v,
              EDGE_BACKWARD_VERTEX (s1->v)->point[0],
              EDGE_BACKWARD_VERTEX (s1->v)->point[1],
              EDGE_FORWARD_VERTEX (s1->v)->point[0],
              EDGE_FORWARD_VERTEX (s1->v)->point[1]);
  pcb_printf ("  Original: (%mn, %mn)-(%mn, %mn) p0=%f, p1=%f\n",
              s1->v->orig_point0[0], s1->v->orig_point0[1], s1->v->orig_point1[0], s1->v->orig_point1[1], s1->v->p0, s1->v->p1);
  pcb_printf ("  Line   %p (%mm, %mm)-(%mm, %mm)\n",
              s2->v,
              EDGE_BACKWARD_VERTEX (s2->v)->point[0],
              EDGE_BACKWARD_VERTEX (s2->v)->point[1],
              EDGE_FORWARD_VERTEX (s2->v)->point[0],
              EDGE_FORWARD_VERTEX (s2->v)->point[1]);
  pcb_printf ("  Original : (%mn, %mn)-(%mn, %mn) p0=%f, p1=%f\n",
              s2->v->orig_point0[0], s2->v->orig_point0[1], s2->v->orig_point1[0], s2->v->orig_point1[1], s2->v->p0, s2->v->p1);

  /* Intersect the two original lines */
  cnt = vect_inters2 (s1->v->orig_point0, s1->v->orig_point1, s1->v->p0, s1->v->p1,
                      s2->v->orig_point0, s2->v->orig_point1, s2->v->p0, s2->v->p1,
                      v1, v2, &s1_i1, &s2_i1, &s1_i2, &s2_i2);

  printf ("Coefficients (on original lines) %f, %f, %f, %f - intersection count is %i\n", s1_i1, s2_i1, s1_i2, s2_i2, cnt);

  if (cnt == 0)
    {
      printf ("\n");
      return 0;
    }

  /* ... */
  if (cnt == 2)
    {
      /* XXX: cnt == 2 intersections are parallel lines. The coefficients should represent
       *      the endpoints of each line. Rejecting these here is not actually correct, as
       *      there may still be an intersection (depending on the segment bounds).
       */
//      if (s1->v->p0 - EPSILON > s1_i2 || s1_i2 > s1->v->p1 + EPSILON)
      if (s1->v->p0 > s1_i2 || s1_i2 > s1->v->p1)
        {
//          printf ("  Second intersection is off the first line bounds\n");
          printf ("BUG!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! (In the way we handle this... - might still be valid intersection) <<<<<<<<<<<\n");
          cnt--;
        }
//      else if (s2->v->p0 - EPSILON > s2_i2 || s2_i2 > s2->v->p1 + EPSILON)
      else if (s2->v->p0 > s2_i2 || s2_i2 > s2->v->p1)
        {
//          printf ("  Second intersection is off the second line bounds\n");
          printf ("BUG!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! (In the way we handle this... - might still be valid intersection) <<<<<<<<<<<\n");
          cnt--;
        }
    }

//  if (s1->v->p0 - EPSILON > s1_i1 || s1_i1 > s1->v->p1 + EPSILON)
  if (s1->v->p0 > s1_i1 || s1_i1 > s1->v->p1)
    {
//      printf ("  First intersection is off the first line bounds\n");
      if (cnt == 2)
        printf ("BUG2!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! (In the way we handle this... - might still be valid intersection) <<<<<<<<<<<\n");
      cnt--;
      /* Copy second intersection details to first */
      Vcopy (v1, v2);
      s1_i1 = s1_i2;
      s2_i1 = s2_i2;
    }
//  else if (s2->v->p0 - EPSILON > s2_i1 || s2_i1 > s2->v->p1 + EPSILON)
  else if (s2->v->p0 > s2_i1 || s2_i1 > s2->v->p1)
    {
//      printf ("  First intersection is off the second line bounds\n");
      if (cnt == 2)
        printf ("BUG2!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! (In the way we handle this... - might still be valid intersection) <<<<<<<<<<<\n");
      cnt--;
      /* Copy second intersection details to first */
      Vcopy (v1, v2);
      s1_i1 = s1_i2;
      s2_i1 = s2_i2;
    }

  if (cnt == 0)
    {
      printf ("\n");
      return 0;
    }

  if (i->touch)  /* if checking touches one find and we're done */
    longjmp (*i->touch, TOUCHES);

  /* Mark the contour PLINEs as intersected */
  s1->p->Flags.status = ISECTED;
  s2->p->Flags.status = ISECTED;

  for (; cnt; cnt--)
    {
      bool done_insert_on_s1 = insert_vertex_in_seg (i, s1, cnt > 1 ? v2 : v1, cnt > 1 ? s1_i2 : s1_i1); /* Was s2 */
      bool done_insert_on_s2 = insert_vertex_in_seg (i, s2, cnt > 1 ? v2 : v1, cnt > 1 ? s2_i2 : s2_i1); /* Was s1 */

      printf ("\n");

      /* Skip any remaining r_search hits against segment i, as any futher
       * intersections will be rejected until the next pass anyway.
       */
      if ((done_insert_on_s1 && s1 == i->s) ||
          (done_insert_on_s2 && s2 == i->s))
        longjmp (*i->env, 1);

      /* If we inserted on s (but not i), skip return now, as we can't continue with
       * the for-loop iteration if we modified geoemtry
       */
      if (done_insert_on_s1 || done_insert_on_s2)
        return 0;
//        longjmp (*i->env, 1);
    }

  return 0;
}

static int
seg_in_seg_arc_line (struct info *i, struct seg *s1, struct seg *s2)
{
  double m1, m2;
  Vector v1, v2;
  int cnt;
  Angle start_angle;
  Angle end_angle;
  Angle delta_angle;

  assert (s1->v->is_round);
  assert (!s2->v->is_round);

  cnt = circle_line_intersect ((double)s1->v->cx, (double)s1->v->cy, (double)s1->v->radius,
                               (double)s2->v->orig_point0[0], (double)s2->v->orig_point0[1],
                               (double)s2->v->orig_point1[0], (double)s2->v->orig_point1[1],
                               &m1, &m2);
  start_angle = atan2 ((EDGE_BACKWARD_VERTEX (s1->v)->point[1] - s1->v->cy), -(EDGE_BACKWARD_VERTEX (s1->v)->point[0] - s1->v->cx)) / M180;
  end_angle   = atan2 (( EDGE_FORWARD_VERTEX (s1->v)->point[1] - s1->v->cy), -( EDGE_FORWARD_VERTEX (s1->v)->point[0] - s1->v->cx)) / M180;

#warning delta angle calculation looks rather suspect - wont work for arcs > 180 degrees span
  delta_angle = end_angle - start_angle;

  if (delta_angle > 180.) delta_angle -= 360.;
  if (delta_angle < -180.) delta_angle += 360.;

#if 0
  if (delta_angle > 180. - EPSILON && delta_angle < 180 + EPSILON)
    {
      printf ("HMM\n");
      cnt = 0;
      return 0;
    }
#endif

  printf ("Intersecting\n");

  pcb_printf ("  circle %p (%mn, %mn) R=%mn, start_angle=%f, end_angle=%f, delta_angle=%f\n",
              s1->v, s1->v->cx, s1->v->cy, s1->v->radius, start_angle, end_angle, delta_angle);

  pcb_printf ("  Line   %p (%mm, %mm)-(%mm, %mm)",
              s2->v,
              EDGE_BACKWARD_VERTEX (s2->v)->point[0],
              EDGE_BACKWARD_VERTEX (s2->v)->point[1],
              EDGE_FORWARD_VERTEX (s2->v)->point[0],
              EDGE_FORWARD_VERTEX (s2->v)->point[1]);
  pcb_printf ("  Original : (%mn, %mn)-(%mn, %mn) p0=%f, p1=%f\n",
              s2->v->orig_point0[0], s2->v->orig_point0[1], s2->v->orig_point1[0], s2->v->orig_point1[1], s2->v->p0, s2->v->p1);

  printf ("  Intersect count is %i\n", cnt);

  if (cnt == 0)
    {
      printf ("Trying with a perturbation\n");
      cnt = circle_line_intersect ((double)s1->v->cx, (double)s1->v->cy, (double)s1->v->radius,
                                   (double)s2->v->orig_point0[0], (double)s2->v->orig_point0[1] + 1,
                                   (double)s2->v->orig_point1[0], (double)s2->v->orig_point1[1] + 1,
                                   &m1, &m2);
      printf ("  Intersect count is %i\n", cnt);

      if (cnt == 0)
        return 0;

      printf ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! <<<<<<\n");
    }

  if (cnt == 2)
    {
      //if (m2 < 0. - EPSILON || m2 > 1. + EPSILON)
      //if (s2->v->p0 - EPSILON > m2 || m2 > s2->v->p1 + EPSILON)
      if (s2->v->p0 > m2 || m2 > s2->v->p1)
        {
          printf ("  Second intersection is out of line bounds, m2 = %f\n", m2);
          cnt--;
        }
      else
        /* Process whether the m2 intersection lies on the arc */
        {
          Angle m2_angle;
          Angle m2_delta;

          Vcopy (v2, s2->v->orig_point0);
          v2[0] += m2 * (s2->v->orig_point1[0] - s2->v->orig_point0[0]);
          v2[1] += m2 * (s2->v->orig_point1[1] - s2->v->orig_point0[1]);

          m2_angle = atan2 ((v2[1] - s1->v->cy), -(v2[0] - s1->v->cx)) / M180;
          m2_delta = m2_angle - start_angle;

          if (m2_delta > 180.) m2_delta -= 360.;
          if (m2_delta < -180.) m2_delta += 360.;

          pcb_printf ("  Second intersection is at (%mm, %mm), angle %f\n", v2[0], v2[1], m2_angle);
              printf ("    m2_angle=%f m2_delta=%f, delta_angle=%f\n",
                      m2_angle, m2_delta, delta_angle);

//          if ((delta_angle > 0. && ( m2_delta < 0. - EPSILON ||  m2_delta >  delta_angle + EPSILON)) ||
//              (delta_angle < 0. && (-m2_delta < 0. - EPSILON || -m2_delta > -delta_angle + EPSILON)))
          if ((delta_angle > 0. && ( m2_delta < 0. ||  m2_delta >  delta_angle)) ||
              (delta_angle <= 0. && (-m2_delta < 0. || -m2_delta > -delta_angle)))
            {
              printf ("    Excluding second intersection, as not on arc, m2_angle=%f m2_delta=%f, delta_angle=%f\n",
                      m2_angle, m2_delta, delta_angle);
              cnt --;
            }
        }
    }

  //if (m1 < 0. - EPSILON || m1 > 1. + EPSILON)
  //if (s2->v->p0 - EPSILON > m1 || m1 > s2->v->p1 + EPSILON)
  if (s2->v->p0 > m1 || m1 > s2->v->p1)
    {
      printf ("  First intersection is out of line bounds, m1 = %f\n", m1);
      cnt--;
      m1 = m2;
      Vcopy (v1, v2);
    }
  else
    {
      Angle m1_angle;
      Angle m1_delta;

#if 0
      Vcopy (v1, s2->v->point);
      v1[0] += m1 * (EDGE_FORWARD_VERTEX (s2->v)->point[0] - EDGE_BACKWARD_VERTEX (s2->v)->point[0]);
      v1[1] += m1 * (EDGE_FORWARD_VERTEX (s2->v)->point[1] - EDGE_BACKWARD_VERTEX (s2->v)->point[1]);
#else
      Vcopy (v1, s2->v->orig_point0);
      v1[0] += m1 * (s2->v->orig_point1[0] - s2->v->orig_point0[0]);
      v1[1] += m1 * (s2->v->orig_point1[1] - s2->v->orig_point0[1]);
#endif

      m1_angle = atan2 ((v1[1] - s1->v->cy), -(v1[0] - s1->v->cx)) / M180;
      m1_delta = m1_angle - start_angle;

      if (m1_delta > 180.) m1_delta -= 360.;
      if (m1_delta < -180.) m1_delta += 360.;

      pcb_printf ("  First intersection is at (%mm, %mm), angle %f\n", v1[0], v1[1], m1_angle);
          printf ("    m1_angle=%f m1_delta=%f, delta_angle=%f\n",
                  m1_angle, m1_delta, delta_angle);

//      if ((delta_angle > 0. && ( m1_delta < 0. - EPSILON ||  m1_delta >  delta_angle + EPSILON)) ||
//          (delta_angle < 0. && (-m1_delta < 0. - EPSILON || -m1_delta > -delta_angle + EPSILON)))
      if ((delta_angle > 0. && ( m1_delta < 0. ||  m1_delta >  delta_angle)) ||
          (delta_angle <= 0. && (-m1_delta < 0. || -m1_delta > -delta_angle)))
        {
          printf ("    Excluding first intersection, as not on arc, m1_angle=%f m1_delta=%f, delta_angle=%f\n",
                  m1_angle, m1_delta, delta_angle);
          cnt --;
          m1 = m2;
          Vcopy (v1, v2);
        }
    }

  /* Process whether the m1 intersection lies on the arc */

  if (cnt == 0)
    {
      printf ("\n");
      return 0;
    }

  if (i->touch)  /* if checking touches one find and we're done */
    longjmp (*i->touch, TOUCHES);

  /* Mark the contour PLINEs as intersected */
  s1->p->Flags.status = ISECTED;
  s2->p->Flags.status = ISECTED;

  for (; cnt; cnt--)
    {
      bool done_insert_on_s1 = insert_vertex_in_seg (i, s1, cnt > 1 ? v2 : v1, -1.0 /*cnt > 1 ? 0.0 : 0.0*/); /* XXX */
      bool done_insert_on_s2 = insert_vertex_in_seg (i, s2, cnt > 1 ? v2 : v1, cnt > 1 ? m2 : m1);

      printf ("\n");

      /* Skip any remaining r_search hits against segment i, as any futher
       * intersections will be rejected until the next pass anyway.
       */
      if ((done_insert_on_s1 && s1 == i->s) ||
          (done_insert_on_s2 && s2 == i->s))
        longjmp (*i->env, 1);

      /* If we inserted on s (but not i), skip return now, as we can't continue with
       * the for-loop iteration if we modified geoemtry
       */
      if (done_insert_on_s1 || done_insert_on_s2)
        return 0;
//        longjmp (*i->env, 1);
    }

  return 0;
}

static int
seg_in_seg_arc_arc (struct info *i, struct seg *s1, struct seg *s2)
{
  Vector v1, v2;
  int cnt;
  double xi1, yi1;
  double xi2, yi2;
  Angle start_angle1;
  Angle end_angle1;
  Angle delta_angle1;
  Angle start_angle2;
  Angle end_angle2;
  Angle delta_angle2;

  assert (s1->v->is_round);
  assert (s2->v->is_round);

//  printf ("Querying arc-arc intersection\n");

#if 1
  /* COP OUT */
  if (s1->v->cx     == s2->v->cx &&
      s1->v->cy     == s2->v->cy &&
      s1->v->radius == s2->v->radius)
    {
      printf ("Cop-out for co-circular arcs for now - they will be better handled by their line approximations. <<<<<<<<<<\n");
      return seg_in_seg_line_line (i, s1, s2);
    }
#endif

  cnt = circle_circle_intersect ((double)s1->v->cx, (double)s1->v->cy, (double)s1->v->radius,
                                 (double)s2->v->cx, (double)s2->v->cy, (double)s2->v->radius,
                                 &xi1, &yi1, &xi2, &yi2);

  v1[0] = xi1;
  v1[1] = yi1;
  v2[0] = xi2;
  v2[1] = yi2;

  start_angle1 = atan2 ((s1->v      ->point[1] - s1->v->cy), -(s1->v      ->point[0] - s1->v->cx)) / M180;
  end_angle1   = atan2 ((s1->v->next->point[1] - s1->v->cy), -(s1->v->next->point[0] - s1->v->cx)) / M180;

  start_angle2 = atan2 ((s2->v      ->point[1] - s2->v->cy), -(s2->v      ->point[0] - s2->v->cx)) / M180;
  end_angle2   = atan2 ((s2->v->next->point[1] - s2->v->cy), -(s2->v->next->point[0] - s2->v->cx)) / M180;

  #warning delta angle calculation looks rather suspect - wont work for arcs > 180 degrees span
  delta_angle1 = end_angle1 - start_angle1;
  delta_angle2 = end_angle2 - start_angle2;

  if (delta_angle1 > 180.) delta_angle1 -= 360.;
  if (delta_angle1 < -180.) delta_angle1 += 360.;

  if (delta_angle2 > 180.) delta_angle2 -= 360.;
  if (delta_angle2 < -180.) delta_angle2 += 360.;

  printf ("Intersecting\n");

  pcb_printf ("  circle %p (%mm, %mm) R=%mm, start_angle=%f, end_angle=%f, delta_angle=%f\n",
              s1->v, s1->v->cx, s1->v->cy, s1->v->radius, start_angle1, end_angle1, delta_angle1);

  pcb_printf ("  circle %p (%mm, %mm) R=%mm, start_angle=%f, end_angle=%f, delta_angle=%f\n",
              s2->v, s2->v->cx, s2->v->cy, s2->v->radius, start_angle2, end_angle2, delta_angle2);

  printf ("  Intersect count is %i\n", cnt);

  if (cnt == 0)
    return 0;

  if (cnt == 2)
    /* Process whether the second intersection lies on the second arc */
    {
      Angle s2i2_angle = atan2 ((v2[1] - s2->v->cy), -(v2[0] - s2->v->cx)) / M180;
      Angle s2i2_delta = s2i2_angle - start_angle2;

      if (s2i2_delta >  180.) s2i2_delta -= 360.;
      if (s2i2_delta < -180.) s2i2_delta += 360.;

      pcb_printf ("  Second intersection is at (%mm, %mm), angle %f\n", v2[0], v2[1], s2i2_angle);
          printf ("    s2i2_angle=%f s2i2_delta=%f, delta_angle2=%f\n",
                  s2i2_angle, s2i2_delta, delta_angle2);

      if ((delta_angle2 >  0. && ( s2i2_delta < 0. ||  s2i2_delta >  delta_angle2)) ||
          (delta_angle2 <= 0. && (-s2i2_delta < 0. || -s2i2_delta > -delta_angle2)))
        {
          printf ("    Excluding second intersection, as not on arc, s2i2_angle=%f s2i2_delta=%f, delta_angle=%f\n",
                  s2i2_angle, s2i2_delta, delta_angle2);
          cnt --;
        }
      else
        /* Process whether the second intersection lies on the first arc */
        {
          Angle s1i2_angle = atan2 ((v2[1] - s1->v->cy), -(v2[0] - s1->v->cx)) / M180;
          Angle s1i2_delta = s1i2_angle - start_angle1;

          if (s1i2_delta >  180.) s1i2_delta -= 360.;
          if (s1i2_delta < -180.) s1i2_delta += 360.;

          pcb_printf ("  Second intersection is at (%mm, %mm), angle %f\n", v2[0], v2[1], s1i2_angle);
              printf ("    s1i2_angle=%f s1i2_delta=%f, delta_angle1=%f\n",
                      s1i2_angle, s1i2_delta, delta_angle1);

          if ((delta_angle1 >  0. && ( s1i2_delta < 0. ||  s1i2_delta >  delta_angle1)) ||
              (delta_angle1 <= 0. && (-s1i2_delta < 0. || -s1i2_delta > -delta_angle1)))
            {
              printf ("    Excluding second intersection, as not on arc, s1i2_angle=%f s1i2_delta=%f, delta_angle=%f\n",
                      s1i2_angle, s1i2_delta, delta_angle1);
              cnt --;
            }
        }
    }

  if (1)
    /* process whether the first intersection lies on the second arc */
    {
      Angle s2i1_angle = atan2 ((v1[1] - s2->v->cy), -(v1[0] - s2->v->cx)) / M180;
      Angle s2i1_delta = s2i1_angle - start_angle2;

      if (s2i1_delta >  180.) s2i1_delta -= 360.;
      if (s2i1_delta < -180.) s2i1_delta += 360.;

      pcb_printf ("  First intersection is at (%mm, %mm), angle %f\n", v1[0], v1[1], s2i1_angle);
          printf ("    s2i1_angle=%f s2i1_delta=%f, delta_angle2=%f\n",
                  s2i1_angle, s2i1_delta, delta_angle2);

      if ((delta_angle2 >  0. && ( s2i1_delta < 0. ||  s2i1_delta >  delta_angle2)) ||
          (delta_angle2 <= 0. && (-s2i1_delta < 0. || -s2i1_delta > -delta_angle2)))
        {
          printf ("    Excluding first intersection, as not on arc, s2i1_angle=%f s2i1_delta=%f, delta_angle=%f\n",
                  s2i1_angle, s2i1_delta, delta_angle2);
          cnt --;
          Vcopy (v1, v2);
        }
      else
        /* Process whether the first intersection lies on the first arc */
        {
          Angle s1i1_angle = atan2 ((v1[1] - s1->v->cy), -(v1[0] - s1->v->cx)) / M180;
          Angle s1i1_delta = s1i1_angle - start_angle1;

          if (s1i1_delta >  180.) s1i1_delta -= 360.;
          if (s1i1_delta < -180.) s1i1_delta += 360.;

          pcb_printf ("  First intersection is at (%mm, %mm), angle %f\n", v1[0], v1[1], s1i1_angle);
              printf ("    s1i1_angle=%f s1i1_delta=%f, delta_angle1=%f\n",
                      s1i1_angle, s1i1_delta, delta_angle1);

          if ((delta_angle1 >  0. && ( s1i1_delta < 0. ||  s1i1_delta >  delta_angle1)) ||
              (delta_angle1 <= 0. && (-s1i1_delta < 0. || -s1i1_delta > -delta_angle1)))
            {
              printf ("    Excluding first intersection, as not on arc, s1i1_angle=%f s1i1_delta=%f, delta_angle=%f\n",
                      s1i1_angle, s1i1_delta, delta_angle1);
              cnt --;
              Vcopy (v1, v2);
            }
        }
    }

  if (cnt == 0)
    {
      printf ("\n");
      return 0;
    }

  if (i->touch)  /* if checking touches one find and we're done */
    longjmp (*i->touch, TOUCHES);

  /* Mark the contour PLINEs as intersected */
  s1->p->Flags.status = ISECTED;
  s2->p->Flags.status = ISECTED;

  for (; cnt; cnt--)
    {
      bool done_insert_on_s1 = insert_vertex_in_seg (i, s1, cnt > 1 ? v2 : v1, -1.0 /*cnt > 1 ? 0.0 : 0.0*/); /* XXX */
      bool done_insert_on_s2 = insert_vertex_in_seg (i, s2, cnt > 1 ? v2 : v1, -1.0 /*cnt > 1 ? 0.0 : 0.0*/); /* XXX */

      printf ("\n");

      /* Skip any remaining r_search hits against segment i, as any futher
       * intersections will be rejected until the next pass anyway.
       */
      if ((done_insert_on_s1 && s1 == i->s) ||
          (done_insert_on_s2 && s2 == i->s))
        longjmp (*i->env, 1);

      /* If we inserted on s (but not i), skip return now, as we can't continue with
       * the for-loop iteration if we modified geoemtry
       */
      if (done_insert_on_s1 || done_insert_on_s2)
        return 0;
//        longjmp (*i->env, 1);
    }

  return 0;
}

/*
 * seg_in_seg()
 * (C) 2006 harry eaton
 * This routine checks if the segment in the tree intersect the search segment.
 * If it does, the plines are marked as intersected and the point is marked for
 * the cvclist. If the point is not already a vertex, a new vertex is inserted
 * and the search for intersections starts over at the beginning.
 * That is potentially a significant time penalty, but it does solve the snap rounding
 * problem. There are efficient algorithms for finding intersections with snap
 * rounding, but I don't have time to implement them right now.
 */
static int
seg_in_seg (const BoxType * b, void *cl)
{
  struct info *i = (struct info *) cl;
  struct seg *s1 = (struct seg *) b;
  struct seg *s2 = i->s;

  /* When new nodes are added at the end of a pass due to an intersection
   * the segments may be altered. If either segment we're looking at has
   * already been intersected this pass, skip it until the next pass.
   */
  if (s1->intersected || s2->intersected)
    return 0;

  if (s1->v->is_round && s2->v->is_round)
    return seg_in_seg_arc_arc (i, s1, s2);
  else if (s1->v->is_round)
    return seg_in_seg_arc_line (i, s1, s2);
  else if (s2->v->is_round)
    return seg_in_seg_arc_line (i, s2, s1);
  else
    return seg_in_seg_line_line (i, s1, s2);
}

static void *
make_edge_tree (PLINE * pb)
{
  struct seg *s;
  VNODE *bv; /* bv is considred an edge */
  rtree_t *ans = r_create_tree (NULL, 0, 0);
  bv = &pb->head;
  do
    {
      s = (seg *)malloc (sizeof (struct seg));
      s->intersected = 0;

      s->box.X1 = MIN (EDGE_BACKWARD_VERTEX (bv)->point[0], EDGE_FORWARD_VERTEX (bv)->point[0]);
      s->box.X2 = MAX (EDGE_BACKWARD_VERTEX (bv)->point[0], EDGE_FORWARD_VERTEX (bv)->point[0]) + 1;
      s->box.Y1 = MIN (EDGE_BACKWARD_VERTEX (bv)->point[1], EDGE_FORWARD_VERTEX (bv)->point[1]);
      s->box.Y2 = MAX (EDGE_BACKWARD_VERTEX (bv)->point[1], EDGE_FORWARD_VERTEX (bv)->point[1]) + 1;

      if (bv->is_round)
        {
          Angle start_angle;
          Angle end_angle;
          Angle delta_angle;
          BoxType arc_bound;

          start_angle = atan2 ((EDGE_BACKWARD_VERTEX (bv)->point[1] - bv->cy), -(EDGE_BACKWARD_VERTEX (bv)->point[0] - bv->cx)) / M180;
          end_angle   = atan2 (( EDGE_FORWARD_VERTEX (bv)->point[1] - bv->cy), -( EDGE_FORWARD_VERTEX (bv)->point[0] - bv->cx)) / M180;

#warning delta angle calculation looks rather suspect - wont work for arcs > 180 degrees span
          delta_angle = end_angle - start_angle;

          if (delta_angle > 180.) delta_angle -= 360.;
          if (delta_angle < -180.) delta_angle += 360.;

          arc_bound = calc_thin_arc_bounds (bv->cx, bv->cy, bv->radius, bv->radius, start_angle, delta_angle);

          MAKEMIN (s->box.X1, arc_bound.X1);
          MAKEMIN (s->box.Y1, arc_bound.Y1);
          MAKEMAX (s->box.X2, arc_bound.X2);
          MAKEMAX (s->box.Y2, arc_bound.Y2);
        }

      s->v = bv;
      s->p = pb;
      r_insert_entry (ans, (const BoxType *) s, 1);
    }
  while ((bv = NEXT_EDGE (bv)) != &pb->head);
  return (void *) ans;
}

static int
get_seg (const BoxType * b, void *cl)
{
  struct info *i = (struct info *) cl;
  struct seg *s = (struct seg *) b;
  if (i->v == s->v)
    {
      i->s = s;
      longjmp (i->sego, 1);
    }
  return 0;
}

/*
 * intersect() (and helpers)
 * (C) 2006, harry eaton
 * This uses an rtree to find A-B intersections. Whenever a new vertex is
 * added, the search for intersections is re-started because the rounding
 * could alter the topology otherwise. 
 * This should use a faster algorithm for snap rounding intersection finding.
 * The best algorthim is probably found in:
 *
 * "Improved output-sensitive snap rounding," John Hershberger, Proceedings
 * of the 22nd annual symposium on Computational geomerty, 2006, pp 357-366.
 * http://doi.acm.org/10.1145/1137856.1137909
 *
 * Algorithms described by de Berg, or Goodrich or Halperin, or Hobby would
 * probably work as well.
 *
 */

static int
contour_bounds_touch (const BoxType * b, void *cl)
{
  contour_info *c_info = (contour_info *) cl;
  PLINE *pa = c_info->pa;
  PLINE *pb = (PLINE *) b;
  PLINE *rtree_over;
  PLINE *looping_over;
  VNODE *av; /* node iterators */ /* av is considered an edge */
  struct info info;
  BoxType box;
  jmp_buf restart;

  /* Have seg_in_seg return to our desired location if it touches */
  info.env = &restart;
  info.touch = c_info->getout;
//  info.need_restart = 0;
  info.node_insert_list = c_info->node_insert_list;

  /* Pick which contour has the fewer points, and do the loop
   * over that. The r_tree makes hit-testing against a contour
   * faster, so we want to do that on the bigger contour.
   */
  if (pa->Count < pb->Count)
    {
      rtree_over = pb;
      looping_over = pa;
    }
  else
    {
      rtree_over = pa;
      looping_over = pb;
    }

  av = &looping_over->head;
  do				/* Loop over the edges in the smaller contour */
    {
      /* XXX: Use original slope rather than that of the intersected segments? */
      /* check this edge for any insertions */
      double dx;
      info.v = av;
#if 0
      /* compute the slant for region trimming */
      dx = EDGE_FORWARD_VERTEX (av)->point[0] - EDGE_BACKWARD_VERTEX (av)->point[0];
      if (dx == 0)
	info.m = 0;
      else
	{
	  info.m = (EDGE_FORWARD_VERTEX (av)->point[1] - EDGE_BACKWARD_VERTEX (av)->point[1]) / dx;
	  info.b = EDGE_BACKWARD_VERTEX (av)->point[1] - info.m * EDGE_BACKWARD_VERTEX (av)->point[0];
	}
#else
      /* compute the slant for region trimming */
      dx = av->orig_point1[0] - av->orig_point0[0];
      if (dx == 0)
	info.m = 0;
      else
	{
	  info.m = (av->orig_point1[1] - av->orig_point0[1]) / dx;
	  info.b = av->orig_point0[1] - info.m * av->orig_point0[0];
	}
#endif
      box.X2 = (box.X1 = EDGE_BACKWARD_VERTEX (av)->point[0]) + 1;
      box.Y2 = (box.Y1 = EDGE_BACKWARD_VERTEX (av)->point[1]) + 1;

      /* fill in the segment in info corresponding to this node */
      if (setjmp (info.sego) == 0)
	{
	  r_search (looping_over->tree, &box, NULL, get_seg, &info);
	  assert (0);
	}

      /* If we're going to have another pass anyway, skip this */
      if (info.s->intersected && info.node_insert_list != NULL)
	continue;

      if (setjmp (restart))
	continue;

      /* NB: If this actually hits anything, we are teleported back to the beginning */
      info.tree = rtree_over->tree;
      if (info.tree)
	if (UNLIKELY (r_search (info.tree, &info.s->box,
				NULL /* seg_in_region */, seg_in_seg, &info)))
	  assert (0); /* XXX: Memory allocation failure */
    }
  while ((av = NEXT_EDGE (av)) != &looping_over->head);

  c_info->node_insert_list = info.node_insert_list;
//  if (info.need_restart)
//    c_info->need_restart = 1;
  return 0;
}

static int
intersect_impl (jmp_buf * jb, POLYAREA * b, POLYAREA * a, int add)
{
  POLYAREA *t;
  PLINE *pa;
  contour_info c_info;
  int need_restart = 0;
  insert_node_task *task;
//  c_info.need_restart = 0;
  c_info.node_insert_list = NULL;

  /* Search the r-tree of the object with most contours
   * We loop over the contours of "a". Swap if necessary.
   */
  if (a->contour_tree->size > b->contour_tree->size)
    {
      t = b;
      b = a;
      a = t;
    }

  for (pa = a->contours; pa; pa = pa->next)	/* Loop over the contours of POLYAREA "a" */
    {
      BoxType sb;
      jmp_buf out;
      int retval;

      c_info.getout = NULL;
      c_info.pa = pa;

      if (!add)
	{
	  retval = setjmp (out);
	  if (retval)
	    {
	      /* The intersection test short-circuited back here,
	       * we need to clean up, then longjmp to jb */
	      longjmp (*jb, retval);
	    }
	  c_info.getout = &out;
	}

      sb.X1 = pa->xmin;
      sb.Y1 = pa->ymin;
      sb.X2 = pa->xmax + 1;
      sb.Y2 = pa->ymax + 1;

      r_search (b->contour_tree, &sb, NULL, contour_bounds_touch, &c_info);
//      if (c_info.need_restart)
//	need_restart = 1;
    }

  /* Process any deferred node insersions */
  task = c_info.node_insert_list;
  while (task != NULL)
    {
      insert_node_task *next = task->next;

      /* Do insersion */
      PREV_VERTEX (task->new_node) = EDGE_BACKWARD_VERTEX (task->node_seg->v);
      NEXT_VERTEX (task->new_node) = EDGE_FORWARD_VERTEX (task->node_seg->v);
      PREV_VERTEX (EDGE_FORWARD_VERTEX (task->node_seg->v)) = task->new_node;
      EDGE_FORWARD_VERTEX (task->node_seg->v) = task->new_node;
      task->node_seg->p->Count++;

      if (task->param >= 0.0 && task->param <= 1.0)
        {
          /* Nicely behaved linear segment addition, with parameter */
          task->new_node->p1 = task->new_node->prev->p1;
          task->new_node->prev->p1 = task->param;
          task->new_node->p0 = task->param;
        }
      else
        {
          /* Used for arc-segment insersions, where we redeclare the original coordinates of the pieces */
          if (task->new_node->prev->p1 != 1.0)
            printf ("task->new_node->prev->p1 != 1.0\n");
          task->new_node->prev->p1 = 1.0; /* NB: Should already be 1.0 - might be worth asserting? */
          Vcopy (task->new_node->prev->orig_point1, task->new_node->point);

          task->new_node->p0 = 0.0;
          task->new_node->p1 = 1.0;
          Vcopy (task->new_node->orig_point0, task->new_node->point);
          Vcopy (task->new_node->orig_point1, task->new_node->next->point);
        }

#warning NEED AN UPDATE FOR ROUND CONTOURS HERE?
      if (cntrbox_check (task->node_seg->p, task->new_node->point)) /* XXX: DOES THIS WORK / MATTER FOR ARC SEGMENT INSERTIONS? */
        {
          /* First delete the contour from the contour r-tree, as its bounds
           * may be adjusted whilst inserting nodes
           */
          r_delete_entry (b->contour_tree, (const BoxType *) task->node_seg->p);
          cntrbox_adjust (task->node_seg->p, task->new_node->point); /* XXX: DOES THIS WORK / MATTER FOR ARC SEGMENT INSERTIONS? */
          r_insert_entry (b->contour_tree, (const BoxType *) task->node_seg->p, 0);
        }

      if (adjust_tree (task->node_seg->p->tree, task->node_seg))
	assert (0); /* XXX: Memory allocation failure */

      need_restart = 1; /* Any new nodes could intersect */

      free (task);
      task = next;
    }

  return need_restart;
}

static int
intersect (jmp_buf * jb, POLYAREA * b, POLYAREA * a, int add)
{
  int call_count = 1;
  while (intersect_impl (jb, b, a, add))
    call_count++;
  return 0;
}

static void
M_POLYAREA_intersect (jmp_buf * e, POLYAREA * afst, POLYAREA * bfst, int add)
{
  POLYAREA *a = afst, *b = bfst;
  PLINE *curcA, *curcB;
  CVCList *the_list = NULL;

  if (a == NULL || b == NULL)
    error (err_bad_parm);
  do
    {
      do
	{
	  if (a->contours->xmax >= b->contours->xmin &&
	      a->contours->ymax >= b->contours->ymin &&
	      a->contours->xmin <= b->contours->xmax &&
	      a->contours->ymin <= b->contours->ymax)
	    {
	      if (UNLIKELY (intersect (e, a, b, add)))
		error (err_no_memory);
	    }
	}
      while (add && (a = a->f) != afst);
      for (curcB = b->contours; curcB != NULL; curcB = curcB->next)
	if (curcB->Flags.status == ISECTED)
	  {
	    the_list = add_descriptors (curcB, 'B', the_list);
	    if (UNLIKELY (the_list == NULL))
	      error (err_no_memory);
	  }
    }
  while (add && (b = b->f) != bfst);
  do
    {
      for (curcA = a->contours; curcA != NULL; curcA = curcA->next)
	if (curcA->Flags.status == ISECTED)
	  {
	    the_list = add_descriptors (curcA, 'A', the_list);
	    if (UNLIKELY (the_list == NULL))
	      error (err_no_memory);
	  }
    }
  while (add && (a = a->f) != afst);
}				/* M_POLYAREA_intersect */

static inline int
cntrbox_inside (PLINE * c1, PLINE * c2)
{
  assert (c1 != NULL && c2 != NULL);
  return ((c1->xmin >= c2->xmin) &&
	  (c1->ymin >= c2->ymin) &&
	  (c1->xmax <= c2->xmax) && (c1->ymax <= c2->ymax));
}

/*****************************************************************/
/* Routines for making labels */

static int
count_contours_i_am_inside (const BoxType * b, void *cl)
{
  PLINE *me = (PLINE *) cl;
  PLINE *check = (PLINE *) b;

  if (poly_ContourInContour (check, me))
    return 1;
  return 0;
}

/* cntr_in_M_POLYAREA
returns poly is inside outfst ? TRUE : FALSE */
static int
cntr_in_M_POLYAREA (PLINE * poly, POLYAREA * outfst, BOOLp test)
{
  POLYAREA *outer = outfst;
  heap_t *heap;

  assert (poly != NULL);
  assert (outer != NULL);

  heap = heap_create ();
  do
    {
      if (cntrbox_inside (poly, outer->contours))
	heap_insert (heap, outer->contours->area, (void *) outer);
    }
  /* if checking touching, use only the first polygon */
  while (!test && (outer = outer->f) != outfst);
  /* we need only check the smallest poly container
   * but we must loop in case the box containter is not
   * the poly container */
  do
    {
      if (heap_is_empty (heap))
	break;
      outer = (POLYAREA *) heap_remove_smallest (heap);

      switch (r_search
	      (outer->contour_tree, (BoxType *) poly, NULL,
	       count_contours_i_am_inside, poly))
	{
	case 0:		/* Didn't find anything in this piece, Keep looking */
	  break;
	case 1:		/* Found we are inside this piece, and not any of its holes */
	  heap_destroy (&heap);
	  return TRUE;
	case 2:		/* Found inside a hole in the smallest polygon so far. No need to check the other polygons */
	  heap_destroy (&heap);
	  return FALSE;
	default:
	  printf ("Something strange here\n");
	  break;
	}
    }
  while (1);
  heap_destroy (&heap);
  return FALSE;
}				/* cntr_in_M_POLYAREA */

static char *
theState (VNODE * e)
{
  static char u[] = "UNKNOWN";
  static char i[] = "INSIDE";
  static char o[] = "OUTSIDE";
  static char s[] = "SHARED";
  static char s2[] = "SHARED2";

  switch (EDGE_LABEL (e))
    {
    case INSIDE:
      return i;
    case OUTSIDE:
      return o;
    case SHARED:
      return s;
    case SHARED2:
      return s2;
    default:
      return u;
    }
}

#ifdef DEBUG
#ifdef DEBUG_ALL_LABELS
static void
print_labels (PLINE * a)
{
  VNODE *e = &a->head;

  do
    {
      DEBUGP ("(%mn, %mn)->(%mn, %mn) labeled %s  is_round=%i radius=%f cvc_next=%p cvc_prev=%p\n",
              EDGE_BACKWARD_VERTEX (e)->point[0], EDGE_BACKWARD_VERTEX (e)->point[1],
               EDGE_FORWARD_VERTEX (e)->point[0],  EDGE_FORWARD_VERTEX (e)->point[1], theState (e),
              e->is_round, e->radius, EDGE_BACKWARD_VERTEX (e)->cvc_next, EDGE_BACKWARD_VERTEX (e)->cvc_prev);
    }
  while ((e = NEXT_EDGE (e)) != &a->head);
}
#endif
#endif

/*
label_contour
 (C) 2006 harry eaton
 (C) 1993 Klamer Schutte
 (C) 1997 Alexey Nikitin, Michael Leonov
*/

static BOOLp
label_contour (PLINE * a)
{
  VNODE *cure = &a->head; /* cur is considered an edge */
  VNODE *first_labelled = NULL;
  int label = UNKNWN;

  do
    {
      if (EDGE_BACKWARD_VERTEX (cure)->cvc_next)	/* examine cross vertex */
	{
	  label = edge_label (cure);
	  if (first_labelled == NULL)
	    first_labelled = cure;
	  continue;
	}

      if (first_labelled == NULL)
	continue;

      /* This labels nodes which aren't cross-connected */
      assert (label == INSIDE || label == OUTSIDE);
      LABEL_EDGE (cure, label);
    }
  while ((cure = NEXT_EDGE (cure)) != first_labelled);
#ifdef DEBUG_ALL_LABELS
  print_labels (a);
  DEBUGP ("\n\n");
#endif
  return FALSE;
}				/* label_contour */

static BOOLp
cntr_label_POLYAREA (PLINE * poly, POLYAREA * ppl, BOOLp test)
{
  assert (ppl != NULL && ppl->contours != NULL);
  if (poly->Flags.status == ISECTED)
    {
      label_contour (poly);	/* should never get here when BOOLp is true */
    }
  else if (cntr_in_M_POLYAREA (poly, ppl, test))
    {
      if (test)
	return TRUE;
      poly->Flags.status = INSIDE;
    }
  else
    {
      if (test)
	return false;
      poly->Flags.status = OUTSIDE;
    }
  return FALSE;
}				/* cntr_label_POLYAREA */

static BOOLp
M_POLYAREA_label_separated (PLINE * afst, POLYAREA * b, BOOLp touch)
{
  PLINE *curc = afst;

  for (curc = afst; curc != NULL; curc = curc->next)
    {
      if (cntr_label_POLYAREA (curc, b, touch) && touch)
	return TRUE;
    }
  return FALSE;
}

static BOOLp
M_POLYAREA_label (POLYAREA * afst, POLYAREA * b, BOOLp touch)
{
  POLYAREA *a = afst;
  PLINE *curc;

  assert (a != NULL);
  do
    {
      for (curc = a->contours; curc != NULL; curc = curc->next)
	if (cntr_label_POLYAREA (curc, b, touch))
	  {
	    if (touch)
	      return TRUE;
	  }
    }
  while (!touch && (a = a->f) != afst);
  return FALSE;
}

/****************************************************************/

/* routines for temporary storing resulting contours */
static void
InsCntr (jmp_buf * e, PLINE * c, POLYAREA ** dst)
{
  POLYAREA *newp;

  newp = poly_Create ();
  if (newp == NULL)
    error(err_no_memory);

  if (*dst == NULL)
    {
      newp->f = newp->b = newp;
      *dst = newp;
    }
  else
    {
      newp->f = *dst;
      newp->b = (*dst)->b;
      newp->f->b = newp->b->f = newp;
    }

  c->next = NULL;
  newp->contours = c;
  r_insert_entry (newp->contour_tree, (BoxType *) c, 0);
}				/* InsCntr */

static void
PutContour (jmp_buf * e, PLINE * cntr, POLYAREA ** contours, PLINE ** holes,
	    POLYAREA * owner, POLYAREA * parent, PLINE * parent_contour)
{
  assert (cntr != NULL);
  assert (cntr->Count > 2);
  cntr->next = NULL;

  if (cntr->Flags.orient == PLF_DIR)
    {
      if (owner != NULL)
	r_delete_entry (owner->contour_tree, (BoxType *) cntr);
      InsCntr (e, cntr, contours);
    }
  /* put hole into temporary list */
  else
    {
      /* if we know this belongs inside the parent, put it there now */
      if (parent_contour)
	{
	  cntr->next = parent_contour->next;
	  parent_contour->next = cntr;
	  if (owner != parent)
	    {
	      if (owner != NULL)
		r_delete_entry (owner->contour_tree, (BoxType *) cntr);
	      r_insert_entry (parent->contour_tree, (BoxType *) cntr, 0);
	    }
	}
      else
	{
	  cntr->next = *holes;
	  *holes = cntr;	/* let cntr be 1st hole in list */
	  /* We don't insert the holes into an r-tree,
	   * they just form a linked list */
	  if (owner != NULL)
	    r_delete_entry (owner->contour_tree, (BoxType *) cntr);
	}
    }
}				/* PutContour */

static inline void
remove_contour (POLYAREA * piece, PLINE * prev_contour, PLINE * contour,
		int remove_rtree_entry)
{
  if (piece->contours == contour)
    piece->contours = contour->next;
  else if (prev_contour != NULL)
    {
      assert (prev_contour->next == contour);
      prev_contour->next = contour->next;
    }

  contour->next = NULL;

  if (remove_rtree_entry)
    r_delete_entry (piece->contour_tree, (BoxType *) contour);
}

struct polyarea_info
{
  BoxType BoundingBox;
  POLYAREA *pa;
};

static int
heap_it (const BoxType * b, void *cl)
{
  heap_t *heap = (heap_t *) cl;
  struct polyarea_info *pa_info = (struct polyarea_info *) b;
  PLINE *p = pa_info->pa->contours;
  if (p->Count == 0)
    return 0;			/* how did this happen? */
  heap_insert (heap, p->area, pa_info);
  return 1;
}

struct find_inside_info
{
  jmp_buf jb;
  PLINE *want_inside;
  PLINE *result;
};

static int
find_inside (const BoxType * b, void *cl)
{
  struct find_inside_info *info = (struct find_inside_info *) cl;
  PLINE *check = (PLINE *) b;
  /* Do test on check to see if it inside info->want_inside */
  /* If it is: */
  if (check->Flags.orient == PLF_DIR)
    {
      return 0;
    }
  if (poly_ContourInContour (info->want_inside, check))
    {
      info->result = check;
      longjmp (info->jb, 1);
    }
  return 0;
}

static void
InsertHoles (jmp_buf * e, POLYAREA * dest, PLINE ** src)
{
  POLYAREA *curc;
  PLINE *curh, *container;
  heap_t *heap;
  rtree_t *tree;
  int i;
  int num_polyareas = 0;
  struct polyarea_info *all_pa_info, *pa_info;

  if (*src == NULL)
    return;			/* empty hole list */
  if (dest == NULL)
    error (err_bad_parm);	/* empty contour list */

  /* Count dest polyareas */
  curc = dest;
  do
    {
      num_polyareas++;
    }
  while ((curc = curc->f) != dest);

  /* make a polyarea info table */
  /* make an rtree of polyarea info table */
  all_pa_info = (struct polyarea_info *) malloc (sizeof (struct polyarea_info) * num_polyareas);
  tree = r_create_tree (NULL, 0, 0);
  i = 0;
  curc = dest;
  do
    {
      all_pa_info[i].BoundingBox.X1 = curc->contours->xmin;
      all_pa_info[i].BoundingBox.Y1 = curc->contours->ymin;
      all_pa_info[i].BoundingBox.X2 = curc->contours->xmax;
      all_pa_info[i].BoundingBox.Y2 = curc->contours->ymax;
      all_pa_info[i].pa = curc;
      r_insert_entry (tree, (const BoxType *) &all_pa_info[i], 0);
      i++;
    }
  while ((curc = curc->f) != dest);

  /* loop through the holes and put them where they belong */
  while ((curh = *src) != NULL)
    {
      *src = curh->next;

      container = NULL;
      /* build a heap of all of the polys that the hole is inside its bounding box */
      heap = heap_create ();
      r_search (tree, (BoxType *) curh, NULL, heap_it, heap);
      if (heap_is_empty (heap))
	{
#ifndef NDEBUG
#ifdef DEBUG
	  poly_dump (dest);
#endif
#endif
	  poly_DelContour (&curh);
	  error (err_bad_parm);
	}
      /* Now search the heap for the container. If there was only one item
       * in the heap, assume it is the container without the expense of
       * proving it.
       */
      pa_info = (struct polyarea_info *) heap_remove_smallest (heap);
      if (heap_is_empty (heap))
	{			/* only one possibility it must be the right one */
	  assert (poly_ContourInContour (pa_info->pa->contours, curh));
	  container = pa_info->pa->contours;
	}
      else
	{
	  do
	    {
	      if (poly_ContourInContour (pa_info->pa->contours, curh))
		{
		  container = pa_info->pa->contours;
		  break;
		}
	      if (heap_is_empty (heap))
		break;
	      pa_info = (struct polyarea_info *) heap_remove_smallest (heap);
	    }
	  while (1);
	}
      heap_destroy (&heap);
      if (container == NULL)
	{
	  /* bad input polygons were given */
#ifndef NDEBUG
#ifdef DEBUG
	  poly_dump (dest);
#endif
#endif
	  curh->next = NULL;
	  poly_DelContour (&curh);
	  error (err_bad_parm);
	}
      else
	{
	  /* Need to check if this new hole means we need to kick out any old ones for reprocessing */
	  while (1)
	    {
	      struct find_inside_info info;
	      PLINE *prev;

	      info.want_inside = curh;

	      /* Set jump return */
	      if (setjmp (info.jb))
		{
		  /* Returned here! */
		}
	      else
		{
		  info.result = NULL;
		  /* Rtree search, calling back a routine to longjmp back with data about any hole inside the added one */
		  /*   Be sure not to bother jumping back to report the main contour! */
		  r_search (pa_info->pa->contour_tree, (BoxType *) curh, NULL,
			    find_inside, &info);

		  /* Nothing found? */
		  break;
		}

	      /* We need to find the contour before it, so we can update its next pointer */
	      prev = container;
	      while (prev->next != info.result)
		{
		  prev = prev->next;
		}

	      /* Remove hole from the contour */
	      remove_contour (pa_info->pa, prev, info.result, TRUE);

	      /* Add hole as the next on the list to be processed in this very function */
	      info.result->next = *src;
	      *src = info.result;
	    }
	  /* End check for kicked out holes */

	  /* link at front of hole list */
	  curh->next = container->next;
	  container->next = curh;
	  r_insert_entry (pa_info->pa->contour_tree, (BoxType *) curh, 0);

	}
    }
  r_destroy_tree (&tree);
  free (all_pa_info);
}				/* InsertHoles */


/****************************************************************/
/* routines for collecting result */

typedef enum
{
  UNINITIALISED, FORW, BACKW
} DIRECTION;

/* Jump Rule  */
/* e is considered an edge */
typedef int (*J_Rule) (char p, VNODE *e, DIRECTION *cdir);

/* e is considered an edge */
static int
IsectJ_Rule (char p, VNODE *e, DIRECTION * cdir)
{
  *cdir = FORW;
  return (e->Flags.status == INSIDE || e->Flags.status == SHARED);
}

/* e is considered an edge */
static int
UniteJ_Rule (char p, VNODE *e, DIRECTION * cdir)
{
  *cdir = FORW;
  return (e->Flags.status == OUTSIDE || e->Flags.status == SHARED);
}

/* e is considered an edge */
static int
XorJ_Rule (char p, VNODE *e, DIRECTION * cdir)
{
  if (e->Flags.status == INSIDE)
    {
      *cdir = BACKW;
      return TRUE;
    }
  if (e->Flags.status == OUTSIDE)
    {
      *cdir = FORW;
      return TRUE;
    }
  return FALSE;
}

/* e is considered an edge */
static int
SubJ_Rule (char p, VNODE *e, DIRECTION * cdir)
{
  if (p == 'B' && e->Flags.status == INSIDE)
    {
      *cdir = BACKW;
      return TRUE;
    }
  if (p == 'A' && e->Flags.status == OUTSIDE)
    {
      *cdir = FORW;
      return TRUE;
    }
  if (e->Flags.status == SHARED2)
    {
      if (p == 'A')
	*cdir = FORW;
      else
	*cdir = BACKW;
      return TRUE;
    }
  return FALSE;
}

/* return the edge that comes next.
 * if the direction is BACKW, then we return the next vertex
 * so that prev vertex has the flags for the edge
 *
 * returns true if an edge is found, false otherwise
 */
/* *curv is considered a vertex */
static int
jump (VNODE **curv, DIRECTION *cdir, J_Rule j_rule)
{
  CVCList *d, *start;
  VNODE *e; /* e is considered an edge */
  DIRECTION newone;

  if (!(*curv)->cvc_prev)	/* not a cross-vertex */
    {
      if (VERTEX_DIRECTION_EDGE (*curv, *cdir)->Flags.mark)
	return FALSE;
      return TRUE;
    }
#ifdef DEBUG_JUMP
  DEBUGP ("jump entering node at %$mD\n", (*curv)->point[0], (*curv)->point[1]);
#endif
  /* Pick the descriptor of the edge we came into this vertex with, then spin (anti?)clock-wise one edge descriptor */
  if (*cdir == FORW)
    d = (*curv)->cvc_prev->prev; /* If we start with a CVC Vertex.. this previous edge has not been vetted for whether it belongs in the polygon or not!! */
  else
    d = (*curv)->cvc_next->prev;
  start = d;
  do
    {
      /* Get the edge e, associated with that descriptor */
      e = VERTEX_SIDE_DIR_EDGE (d->parent, d->side);
      newone = *cdir;
      if (!e->Flags.mark && j_rule (d->poly, e, &newone))
	{
	  if ((d->side == 'N' && newone == FORW) ||
	      (d->side == 'P' && newone == BACKW))
	    {
#ifdef DEBUG_JUMP
	      DEBUGP ("jump leaving node at %#mD\n",
	              EDGE_DIRECTION_VERTEX (e, newone)->point[0], EDGE_DIRECTION_VERTEX (e, newone)->point[1]);
#endif
	      *curv = d->parent;
	      *cdir = newone;
	      return TRUE;
	    }
	}
    }
  while ((d = d->prev) != start); /* Keep searching around the cvc vertex for a suitable exit edge */
  return FALSE;
}

/* start is considered a vertex */
static int
Gather (VNODE *startv, PLINE **result, J_Rule j_rule, DIRECTION initdir)
{
  VNODE *curv = startv; /* curv is considered a vertex */
  VNODE *newn;
  DIRECTION dir = initdir;
#ifdef DEBUG_GATHER
  DEBUGP ("gather direction = %d\n", dir);
#endif
  assert (*result == NULL);
  do
    {
      /* add vertex (edge?) to polygon */
      if ((newn = poly_CreateNodeFull (curv->point, VERTEX_DIRECTION_EDGE (curv, dir)->is_round,
                                                    VERTEX_DIRECTION_EDGE (curv, dir)->cx,
                                                    VERTEX_DIRECTION_EDGE (curv, dir)->cy,
                                                    VERTEX_DIRECTION_EDGE (curv, dir)->radius)) == NULL)
        return err_no_memory;

      if (dir == FORW)
        {
          VERTEX_FORWARD_EDGE (newn)->orig_point0[0] = VERTEX_FORWARD_EDGE (curv)->orig_point0[0];
          VERTEX_FORWARD_EDGE (newn)->orig_point0[1] = VERTEX_FORWARD_EDGE (curv)->orig_point0[1];
          VERTEX_FORWARD_EDGE (newn)->orig_point1[0] = VERTEX_FORWARD_EDGE (curv)->orig_point1[0];
          VERTEX_FORWARD_EDGE (newn)->orig_point1[1] = VERTEX_FORWARD_EDGE (curv)->orig_point1[1];
          VERTEX_FORWARD_EDGE (newn)->p0 = VERTEX_FORWARD_EDGE (curv)->p0;
          VERTEX_FORWARD_EDGE (newn)->p1 = VERTEX_FORWARD_EDGE (curv)->p1;
          if (VERTEX_FORWARD_EDGE (newn)->p1 < VERTEX_FORWARD_EDGE (newn)->p0)
            {
              printf ("FORW: OH DEAR, p0=%f, p1=%f\n", VERTEX_FORWARD_EDGE (newn)->p0, VERTEX_FORWARD_EDGE (newn)->p1);
//              *(char *)0 = 0; /* CRASH */
            }
        }
      else
        {
          VERTEX_FORWARD_EDGE (newn)->orig_point0[0] = VERTEX_BACKWARD_EDGE (curv)->orig_point1[0];
          VERTEX_FORWARD_EDGE (newn)->orig_point0[1] = VERTEX_BACKWARD_EDGE (curv)->orig_point1[1];
          VERTEX_FORWARD_EDGE (newn)->orig_point1[0] = VERTEX_BACKWARD_EDGE (curv)->orig_point0[0];
          VERTEX_FORWARD_EDGE (newn)->orig_point1[1] = VERTEX_BACKWARD_EDGE (curv)->orig_point0[1];
          VERTEX_FORWARD_EDGE (newn)->p0 = 1.0 - VERTEX_BACKWARD_EDGE (curv)->p1;
          VERTEX_FORWARD_EDGE (newn)->p1 = 1.0 - VERTEX_BACKWARD_EDGE (curv)->p0;
          if (VERTEX_FORWARD_EDGE (newn)->p1 < VERTEX_FORWARD_EDGE (newn)->p0)
            {
              printf ("!FORW: OH DEAR, p0=%f, p1=%f\n", newn->p0, newn->p1);
//              *(char *)0 = 0; /* CRASH */
            }
        }

      if (!*result)
	{
	  *result = poly_NewContour (newn);
	  if (*result == NULL)
	    return err_no_memory;
	}
      else
	{
	  poly_InclVertex_int (PREV_VERTEX (&(*result)->head), newn);
	}
#ifdef DEBUG_GATHER
      DEBUGP ("gather vertex at %mm, %mm, Dir=%i\n", curv->point[0], curv->point[1], dir);
#endif

      /* Now mark the edge as included.  */
      newn = VERTEX_DIRECTION_EDGE (curv, dir);
      newn->Flags.mark = 1;
      /* for SHARED edge mark both */
      if (newn->shared)
	newn->shared->Flags.mark = 1;

      /* Advance to the next vertex (edge?).  */
      curv = (dir == FORW) ? NEXT_VERTEX (curv) : PREV_VERTEX (curv);

      /* see where to go next */
      if (!jump (&curv, &dir, j_rule))
	break;
    }
  while (1);
  return err_ok;
}				/* Gather */

/* curv is considered a vertex */
static void
Collect1 (jmp_buf *e, VNODE *curv, DIRECTION dir, POLYAREA **contours,
          PLINE **holes, J_Rule j_rule)
{
  PLINE *p = NULL;		/* start making contour */
  int errc = err_ok;
  if ((errc = Gather (curv, &p, j_rule, dir)) != err_ok)
    {
      if (p != NULL)
	poly_DelContour (&p);
      error (errc);
    }
  if (!p)
    return;
  poly_PreContour (p, TRUE);
  if (p->Count > 2)
    {
#ifdef DEBUG_GATHER
      DEBUGP ("adding contour with %d verticies and direction %c\n",
	      p->Count, p->Flags.orient ? 'F' : 'B');
#endif
      PutContour (e, p, contours, holes, NULL, NULL, NULL);
    }
  else
    {
      /* some sort of computation error ? */
#ifdef DEBUG_GATHER
      DEBUGP ("Bad contour! Not enough points!!\n");
#endif
      poly_DelContour (&p);
    }
}

static void
Collect (char poly, jmp_buf * e, PLINE * a, POLYAREA ** contours, PLINE ** holes,
         J_Rule j_rule)
{
  VNODE *cure; /* cure is considered an edge */
  DIRECTION dir = UNINITIALISED;

  cure = (&a->head);
//  cur = (&a->head)->next->next->next->next->next; /* Breaks circ_seg_test9.pcb */
  do
    {
#if 0
      // The following may be a nice speedup, but not sure if it is correct.
      // In particular, consider the case when we collect a 'B' polygon contour.
      // Could some of that countour may already have been collected, and there
      // still be a piece we are interested in after? (Can we reach it though??)
      if (cure->Flags.mark != 0)
        break;
#endif

      if (j_rule (poly, cure, &dir) && cure->Flags.mark == 0)
        Collect1 (e, (dir == FORW) ? EDGE_BACKWARD_VERTEX (cure) :
                                     EDGE_FORWARD_VERTEX (cure),
                  dir, contours, holes, j_rule);
    }
  while ((cure = NEXT_EDGE (cure)) != &a->head);
}				/* Collect */


static int
cntr_Collect (jmp_buf * e, PLINE ** A, POLYAREA ** contours, PLINE ** holes,
	      int action, POLYAREA * owner, POLYAREA * parent,
	      PLINE * parent_contour)
{
  PLINE *tmprev;

  if ((*A)->Flags.status == ISECTED)
    {
      switch (action)
	{
	case PBO_UNITE:
	  Collect ('A', e, *A, contours, holes, UniteJ_Rule);
	  break;
	case PBO_ISECT:
	  Collect ('A', e, *A, contours, holes, IsectJ_Rule);
	  break;
	case PBO_XOR:
	  Collect ('A', e, *A, contours, holes, XorJ_Rule);
	  break;
	case PBO_SUB:
	  Collect ('A', e, *A, contours, holes, SubJ_Rule);
	  break;
	};
    }
  else
    {
      switch (action)
	{
	case PBO_ISECT:
	  if ((*A)->Flags.status == INSIDE)
	    {
	      tmprev = *A;
	      /* disappear this contour (rtree entry removed in PutContour) */
	      *A = tmprev->next;
	      tmprev->next = NULL;
	      PutContour (e, tmprev, contours, holes, owner, NULL, NULL);
	      return TRUE;
	    }
	  break;
	case PBO_XOR:
	  if ((*A)->Flags.status == INSIDE)
	    {
	      tmprev = *A;
	      /* disappear this contour (rtree entry removed in PutContour) */
	      *A = tmprev->next;
	      tmprev->next = NULL;
	      poly_InvContour (tmprev);
	      PutContour (e, tmprev, contours, holes, owner, NULL, NULL);
	      return TRUE;
	    }
	  /* break; *//* Fall through (I think this is correct! pcjc2) */
	case PBO_UNITE:
	case PBO_SUB:
	  if ((*A)->Flags.status == OUTSIDE)
	    {
	      tmprev = *A;
	      /* disappear this contour (rtree entry removed in PutContour) */
	      *A = tmprev->next;
	      tmprev->next = NULL;
	      PutContour (e, tmprev, contours, holes, owner, parent,
			  parent_contour);
	      return TRUE;
	    }
	  break;
	}
    }
  return FALSE;
}				/* cntr_Collect */

static void
M_B_AREA_Collect (jmp_buf * e, POLYAREA * bfst, POLYAREA ** contours,
		  PLINE ** holes, int action)
{
  POLYAREA *b = bfst;
  PLINE **cur, **next, *tmp;

  assert (b != NULL);
  do
    {
      for (cur = &b->contours; *cur != NULL; cur = next)
	{
	  next = &((*cur)->next);
	  if ((*cur)->Flags.status == ISECTED)
	    {
	      /* Check for missed intersect contours here. These can come from
	       * cases where contours of A and B touch at a single-vertex, so
	       * are labeled ISECTED for processing, yet our JUMP rules for a
	       * particular operation does not deem to jump between A & B
	       * contours at the shared vertex.
	       *
	       * NB: There Could be grief if the JUMP rule is inconsistent in
	       *     its interpretation from each side of the vertex.
	       */
	    switch (action)
	      {
	      case PBO_UNITE:
		Collect ('B', e, *cur, contours, holes, UniteJ_Rule);
		break;
	      case PBO_ISECT:
		Collect ('B', e, *cur, contours, holes, IsectJ_Rule);
		break;
	      case PBO_XOR:
		Collect ('B', e, *cur, contours, holes, XorJ_Rule);
		break;
	      case PBO_SUB:
		Collect ('B', e, *cur, contours, holes, SubJ_Rule);
		break;
	      }
	    }
	  else if ((*cur)->Flags.status == INSIDE)
	    switch (action)
	      {
	      case PBO_XOR:
	      case PBO_SUB:
		poly_InvContour (*cur);
	      case PBO_ISECT:
		tmp = *cur;
		*cur = tmp->next;
		next = cur;
		tmp->next = NULL;
		tmp->Flags.status = UNKNWN;
		PutContour (e, tmp, contours, holes, b, NULL, NULL);
		break;
	      case PBO_UNITE:
		break;		/* nothing to do - already included */
	      }
	  else if ((*cur)->Flags.status == OUTSIDE)
	    switch (action)
	      {
	      case PBO_XOR:
	      case PBO_UNITE:
		/* include */
		tmp = *cur;
		*cur = tmp->next;
		next = cur;
		tmp->next = NULL;
		tmp->Flags.status = UNKNWN;
		PutContour (e, tmp, contours, holes, b, NULL, NULL);
		break;
	      case PBO_ISECT:
	      case PBO_SUB:
		break;		/* do nothing, not included */
	      }
	}
    }
  while ((b = b->f) != bfst);
}


static inline int
contour_is_first (POLYAREA * a, PLINE * cur)
{
  return (a->contours == cur);
}


static inline int
contour_is_last (PLINE * cur)
{
  return (cur->next == NULL);
}


static inline void
remove_polyarea (POLYAREA ** list, POLYAREA * piece)
{
  /* If this item was the start of the list, advance that pointer */
  if (*list == piece)
    *list = (*list)->f;

  /* But reset it to NULL if it wraps around and hits us again */
  if (*list == piece)
    *list = NULL;

  piece->b->f = piece->f;
  piece->f->b = piece->b;
  piece->f = piece->b = piece;

  /* Reset parentage information */
  piece->parentage = no_parentage;
}

static void
M_POLYAREA_separate_isected (jmp_buf * e, POLYAREA ** pieces,
			     PLINE ** holes, PLINE ** isected)
{
  POLYAREA *a = *pieces;
  POLYAREA *anext;
  PLINE *curc, *next, *prev;
  int finished;

  if (a == NULL)
    return;

  /* TODO: STASH ENOUGH INFORMATION EARLIER ON, SO WE CAN REMOVE THE INTERSECTED
     CONTOURS WITHOUT HAVING TO WALK THE FULL DATA-STRUCTURE LOOKING FOR THEM. */

  do
    {
      int hole_contour = 0;
      int is_outline = 1;

      anext = a->f;
      finished = (anext == *pieces);

      prev = NULL;
      for (curc = a->contours; curc != NULL; curc = next, is_outline = 0)
	{
	  int is_first = contour_is_first (a, curc);
	  int is_last = contour_is_last (curc);
	  int isect_contour = (curc->Flags.status == ISECTED);

	  next = curc->next;

	  if (isect_contour || hole_contour)
	    {

	      /* Reset the intersection flags, since we keep these pieces */
	      if (curc->Flags.status != ISECTED)
		curc->Flags.status = UNKNWN;

	      remove_contour (a, prev, curc, !(is_first && is_last));

	      if (isect_contour)
		{
		  /* Link into the list of intersected contours */
		  curc->next = *isected;
		  *isected = curc;
		}
	      else if (hole_contour)
		{
		  /* Link into the list of holes */
		  curc->next = *holes;
		  *holes = curc;
		}
	      else
		{
		  assert (0);
		}

	      if (is_first && is_last)
		{
		  remove_polyarea (pieces, a);
		  poly_Free (&a);	/* NB: Sets a to NULL */
		}

	    }
	  else
	    {
	      /* Note the item we just didn't delete as the next
	         candidate for having its "next" pointer adjusted.
	         Saves walking the contour list when we delete one. */
	      prev = curc;
	    }

	  /* If we move or delete an outer contour, we need to move any holes
	     we wish to keep within that contour to the holes list. */
	  if (is_outline && isect_contour)
	    hole_contour = 1;

	}

      /* If we deleted all the pieces of the polyarea, *pieces is NULL */
    }
  while ((a = anext), *pieces != NULL && !finished);
}


struct find_inside_m_pa_info
{
  jmp_buf jb;
  POLYAREA *want_inside;
  PLINE *result;
};

static int
find_inside_m_pa (const BoxType * b, void *cl)
{
  struct find_inside_m_pa_info *info = (struct find_inside_m_pa_info *) cl;
  PLINE *check = (PLINE *) b;
  /* Don't report for the main contour */
  if (check->Flags.orient == PLF_DIR)
    return 0;
  /* Don't look at contours marked as being intersected */
  if (check->Flags.status == ISECTED)
    return 0;
  if (cntr_in_M_POLYAREA (check, info->want_inside, FALSE))
    {
      info->result = check;
      longjmp (info->jb, 1);
    }
  return 0;
}


static void
M_POLYAREA_update_primary (jmp_buf * e, POLYAREA ** pieces,
			   PLINE ** holes, int action, POLYAREA * bpa)
{
  POLYAREA *a = *pieces;
  POLYAREA *b;
  POLYAREA *anext;
  PLINE *curc, *next, *prev;
  BoxType box;
  /* int inv_inside = 0; */
  int del_inside = 0;
  int del_outside = 0;
  int finished;

  if (a == NULL)
    return;

  switch (action)
    {
    case PBO_ISECT:
      del_outside = 1;
      break;
    case PBO_UNITE:
    case PBO_SUB:
      del_inside = 1;
      break;
    case PBO_XOR:		/* NOT IMPLEMENTED OR USED */
      /* inv_inside = 1; */
      assert (0);
      break;
    }

  box = *((BoxType *) bpa->contours);
  b = bpa;
  while ((b = b->f) != bpa)
    {
      BoxType *b_box = (BoxType *) b->contours;
      MAKEMIN (box.X1, b_box->X1);
      MAKEMIN (box.Y1, b_box->Y1);
      MAKEMAX (box.X2, b_box->X2);
      MAKEMAX (box.Y2, b_box->Y2);
    }

  if (del_inside)
    {

      do
	{
	  anext = a->f;
	  finished = (anext == *pieces);

	  /* Test the outer contour first, as we may need to remove all children */

	  /* We've not yet split intersected contours out, just ignore them */
	  if (a->contours->Flags.status != ISECTED &&
	      /* Pre-filter on bounding box */
	      ((a->contours->xmin >= box.X1) && (a->contours->ymin >= box.Y1)
	       && (a->contours->xmax <= box.X2)
	       && (a->contours->ymax <= box.Y2)) &&
	      /* Then test properly */
	      cntr_in_M_POLYAREA (a->contours, bpa, FALSE))
	    {

	      /* Delete this contour, all children -> holes queue */

	      /* Delete the outer contour */
	      curc = a->contours;
	      remove_contour (a, NULL, curc, FALSE);	/* Rtree deleted in poly_Free below */
	      /* a->contours now points to the remaining holes */
	      poly_DelContour (&curc);

	      if (a->contours != NULL)
		{
		  /* Find the end of the list of holes */
		  curc = a->contours;
		  while (curc->next != NULL)
		    curc = curc->next;

		  /* Take the holes and prepend to the holes queue */
		  curc->next = *holes;
		  *holes = a->contours;
		  a->contours = NULL;
		}

	      remove_polyarea (pieces, a);
	      poly_Free (&a);	/* NB: Sets a to NULL */

	      continue;
	    }

	  /* Loop whilst we find INSIDE contours to delete */
	  while (1)
	    {
	      struct find_inside_m_pa_info info;
	      PLINE *prev;

	      info.want_inside = bpa;

	      /* Set jump return */
	      if (setjmp (info.jb))
		{
		  /* Returned here! */
		}
	      else
		{
		  info.result = NULL;
		  /* r-tree search, calling back a routine to longjmp back with
		   * data about any hole inside the B polygon.
		   * NB: Does not jump back to report the main contour!
		   */
		  r_search (a->contour_tree, &box, NULL, find_inside_m_pa,
			    &info);

		  /* Nothing found? */
		  break;
		}

	      /* We need to find the contour before it, so we can update its next pointer */
	      prev = a->contours;
	      while (prev->next != info.result)
		{
		  prev = prev->next;
		}

	      /* Remove hole from the contour */
	      remove_contour (a, prev, info.result, TRUE);
	      poly_DelContour (&info.result);
	    }
	  /* End check for deleted holes */

	  /* If we deleted all the pieces of the polyarea, *pieces is NULL */
	}
      while ((a = anext), *pieces != NULL && !finished);

      return;
    }
  else
    {
      /* This path isn't optimised for speed */
    }

  do
    {
      int hole_contour = 0;
      int is_outline = 1;

      anext = a->f;
      finished = (anext == *pieces);

      prev = NULL;
      for (curc = a->contours; curc != NULL; curc = next, is_outline = 0)
	{
	  int is_first = contour_is_first (a, curc);
	  int is_last = contour_is_last (curc);
	  int del_contour = 0;

	  next = curc->next;

	  if (del_outside)
	    del_contour = curc->Flags.status != ISECTED &&
	      !cntr_in_M_POLYAREA (curc, bpa, FALSE);

	  /* Skip intersected contours */
	  if (curc->Flags.status == ISECTED)
	    {
	      prev = curc;
	      continue;
	    }

	  /* Reset the intersection flags, since we keep these pieces */
	  curc->Flags.status = UNKNWN;

	  if (del_contour || hole_contour)
	    {

	      remove_contour (a, prev, curc, !(is_first && is_last));

	      if (del_contour)
		{
		  /* Delete the contour */
		  poly_DelContour (&curc);	/* NB: Sets curc to NULL */
		}
	      else if (hole_contour)
		{
		  /* Link into the list of holes */
		  curc->next = *holes;
		  *holes = curc;
		}
	      else
		{
		  assert (0);
		}

	      if (is_first && is_last)
		{
		  remove_polyarea (pieces, a);
		  poly_Free (&a);	/* NB: Sets a to NULL */
		}

	    }
	  else
	    {
	      /* Note the item we just didn't delete as the next
	         candidate for having its "next" pointer adjusted.
	         Saves walking the contour list when we delete one. */
	      prev = curc;
	    }

	  /* If we move or delete an outer contour, we need to move any holes
	     we wish to keep within that contour to the holes list. */
	  if (is_outline && del_contour)
	    hole_contour = 1;

	}

      /* If we deleted all the pieces of the polyarea, *pieces is NULL */
    }
  while ((a = anext), *pieces != NULL && !finished);
}

static void
M_POLYAREA_Collect_separated (jmp_buf * e, PLINE * afst, POLYAREA ** contours,
			      PLINE ** holes, int action, BOOLp maybe)
{
  PLINE **cur, **next;

  for (cur = &afst; *cur != NULL; cur = next)
    {
      next = &((*cur)->next);
      /* if we disappear a contour, don't advance twice */
      if (cntr_Collect (e, cur, contours, holes, action, NULL, NULL, NULL))
	next = cur;
    }
}

static void
M_POLYAREA_Collect (jmp_buf * e, POLYAREA * afst, POLYAREA ** contours,
		    PLINE ** holes, int action, BOOLp maybe)
{
  POLYAREA *a = afst;
  POLYAREA *parent = NULL;	/* Quiet compiler warning */
  PLINE **cur, **next, *parent_contour;

  assert (a != NULL);
  while ((a = a->f) != afst);
  /* now the non-intersect parts are collected in temp/holes */
  do
    {
      if (maybe && a->contours->Flags.status != ISECTED)
	parent_contour = a->contours;
      else
	parent_contour = NULL;

      /* Take care of the first contour - so we know if we
       * can shortcut reparenting some of its children
       */
      cur = &a->contours;
      if (*cur != NULL)
	{
	  next = &((*cur)->next);
	  /* if we disappear a contour, don't advance twice */
	  if (cntr_Collect (e, cur, contours, holes, action, a, NULL, NULL))
	    {
	      parent = (*contours)->b;	/* InsCntr inserts behind the head */
	      next = cur;
	    }
	  else
	    parent = a;
	  cur = next;
	}
      for (; *cur != NULL; cur = next)
	{
	  next = &((*cur)->next);
	  /* if we disappear a contour, don't advance twice */
	  if (cntr_Collect (e, cur, contours, holes, action, a, parent,
			    parent_contour))
	    next = cur;
	}
    }
  while ((a = a->f) != afst);
}

/* determine if two polygons touch or overlap */
BOOLp
Touching (POLYAREA * a, POLYAREA * b)
{
  jmp_buf e;
  int code;

  if ((code = setjmp (e)) == 0)
    {
#ifdef DEBUG
      if (!poly_Valid (a))
	return -1;
      if (!poly_Valid (b))
	return -1;
#endif
      M_POLYAREA_intersect (&e, a, b, false);

      if (M_POLYAREA_label (a, b, TRUE))
	return TRUE;
      if (M_POLYAREA_label (b, a, TRUE))
	return TRUE;
    }
  else if (code == TOUCHES)
    return TRUE;
  return FALSE;
}

/* the main clipping routines */
int
poly_Boolean (const POLYAREA * a_org, const POLYAREA * b_org,
	      POLYAREA ** res, int action)
{
  POLYAREA *a = NULL, *b = NULL;

  *res = NULL;

  if (!poly_M_Copy0 (&a, a_org) || !poly_M_Copy0 (&b, b_org))
    return err_no_memory;

  return poly_Boolean_free (a, b, res, action);
}				/* poly_Boolean */

static void
M_Set_Parentage (POLYAREA *poly, POLYPARENTAGE parentage)
{
  POLYAREA *piece = poly;

  if (poly == NULL)
    return;

  do
    {
      piece->parentage = parentage;
    }
  while ((piece = piece->f) != poly);

}

static void test_polyInvContour (void);

/* just like poly_Boolean but frees the input polys */
int
poly_Boolean_free (POLYAREA * ai, POLYAREA * bi, POLYAREA ** res, int action)
{
  POLYAREA *a = ai, *b = bi;
  PLINE *a_isected = NULL;
  PLINE *p, *holes = NULL;
  jmp_buf e;
  int code;
  POLYAREA *a_copy, *b_copy;

  test_polyInvContour ();

  *res = NULL;

  /* Make copies for tracking polygon parentage (DEBUG) */
  if (!poly_M_Copy0 (&a_copy, a) || !poly_M_Copy0 (&b_copy, b))
      return err_no_memory;

  /* Move the parentage information over onto the copy */
  if (a_copy != NULL)
    {
      M_Set_Parentage (a_copy, a->parentage);
      M_Set_Parentage (a, no_parentage);
    }

  if (b_copy != NULL)
    {
      M_Set_Parentage (b_copy, b->parentage);
      M_Set_Parentage (b, no_parentage);
    }

  if (!a)
    {
      switch (action)
	{
	case PBO_XOR:
	case PBO_UNITE:
	  *res = bi;
	  code = err_ok;
	  goto out;
	case PBO_SUB:
	case PBO_ISECT:
	  if (b != NULL)
	    poly_Free (&b);
	  code = err_ok;
	  goto out;
	}
    }
  if (!b)
    {
      switch (action)
	{
	case PBO_SUB:
	case PBO_XOR:
	case PBO_UNITE:
	  *res = ai;
	  code = err_ok;
	  goto out;
	case PBO_ISECT:
	  if (a != NULL)
	    poly_Free (&a);
	  code = err_ok;
	  goto out;
	}
    }

  if ((code = setjmp (e)) == 0)
    {
#ifdef DEBUG
      assert (poly_Valid (a));
      assert (poly_Valid (b));
#endif

      /* intersect needs to make a list of the contours in a and b which are intersected */
      M_POLYAREA_intersect (&e, a, b, TRUE);

      /* We could speed things up a lot here if we only processed the relevant contours */
      /* NB: Relevant parts of a are labeled below */
      M_POLYAREA_label (b, a, FALSE);

      *res = a;
      M_POLYAREA_update_primary (&e, res, &holes, action, b);
      M_POLYAREA_separate_isected (&e, res, &holes, &a_isected);
      M_POLYAREA_label_separated (a_isected, b, FALSE);
      M_POLYAREA_Collect_separated (&e, a_isected, res, &holes, action,
				    FALSE);
      M_B_AREA_Collect (&e, b, res, &holes, action);
      poly_Free (&b);

      /* free a_isected */
      while ((p = a_isected) != NULL)
	{
	  a_isected = p->next;
	  poly_DelContour (&p);
	}

      /* XXX: Seen a crash here with certain input (OctalSpartan) with orig_point substituted for segment angles */
      /* The contour was deleted with the poly_DelContour above, possibly suggesting it managed to get into the isected
       * list, AND the holes list
       */
      InsertHoles (&e, *res, &holes);
    }
  /* delete holes if any left */
  while ((p = holes) != NULL)
    {
      holes = p->next;
      poly_DelContour (&p);
    }

  if (code)
    {
      poly_Free (res);
      return code;
    }

out:
  assert (!*res || poly_Valid (*res));

  /* Store perantage information */
  if (*res != NULL)
    {
      POLYPARENTAGE parentage;

      parentage.immaculate_conception = false;
      parentage.action = action;
      parentage.a = a_copy;
      parentage.b = b_copy;

      M_Set_Parentage (*res, parentage);
    }

  return code;
}				/* poly_Boolean_free */

static void
clear_marks (POLYAREA * p)
{
  POLYAREA *n = p;
  PLINE *c;
  VNODE *v;

  do
    {
      for (c = n->contours; c; c = c->next)
	{
	  v = &c->head;
	  do
	    {
	      v->Flags.mark = 0;
	    }
	  while ((v = NEXT_EDGE (v)) != &c->head);
	}
    }
  while ((n = n->f) != p);
}

/* compute the intersection and subtraction (divides "a" into two pieces)
 * and frees the input polys. This assumes that bi is a single simple polygon.
 */
int
poly_AndSubtract_free (POLYAREA * ai, POLYAREA * bi,
		       POLYAREA ** aandb, POLYAREA ** aminusb)
{
  POLYAREA *a = ai, *b = bi;
  PLINE *p, *holes = NULL;
  jmp_buf e;
  int code;

  *aandb = NULL;
  *aminusb = NULL;

  if ((code = setjmp (e)) == 0)
    {

#ifdef DEBUG
      if (!poly_Valid (a))
	return -1;
      if (!poly_Valid (b))
	return -1;
#endif
      M_POLYAREA_intersect (&e, a, b, TRUE);

      M_POLYAREA_label (a, b, FALSE);
      M_POLYAREA_label (b, a, FALSE);

      M_POLYAREA_Collect (&e, a, aandb, &holes, PBO_ISECT, FALSE);
      InsertHoles (&e, *aandb, &holes);
      assert (poly_Valid (*aandb));
      /* delete holes if any left */
      while ((p = holes) != NULL)
	{
	  holes = p->next;
	  poly_DelContour (&p);
	}
      holes = NULL;
      clear_marks (a);
      clear_marks (b);
      M_POLYAREA_Collect (&e, a, aminusb, &holes, PBO_SUB, FALSE);
      InsertHoles (&e, *aminusb, &holes);
      poly_Free (&a);
      poly_Free (&b);
      assert (poly_Valid (*aminusb));
    }
  /* delete holes if any left */
  while ((p = holes) != NULL)
    {
      holes = p->next;
      poly_DelContour (&p);
    }


  if (code)
    {
      poly_Free (aandb);
      poly_Free (aminusb);
      return code;
    }
  assert (!*aandb || poly_Valid (*aandb));
  assert (!*aminusb || poly_Valid (*aminusb));
  return code;
}				/* poly_AndSubtract_free */

static inline int
cntrbox_pointin (PLINE * c, Vector p)
{
  return (p[0] >= c->xmin && p[1] >= c->ymin &&
	  p[0] <= c->xmax && p[1] <= c->ymax);

}

static inline int
node_neighbours (VNODE * a, VNODE * b)
{
  return (a == b) || (a->next == b) || (b->next == a) || (a->next == b->next);
}

void
poly_IniContour (PLINE * c)
{
  if (c == NULL)
    return;
  /* bzero (c, sizeof(PLINE)); */
  NEXT_EDGE (&c->head) = PREV_EDGE (&c->head) = &c->head;
  c->xmin = c->ymin = COORD_MAX;
  c->xmax = c->ymax = -COORD_MAX - 1;
  c->is_round = FALSE;
  c->cx = 0;
  c->cy = 0;
  c->radius = 0;
}

PLINE *
poly_NewContour (VNODE *node)
{
  PLINE *res;

  res = (PLINE *) calloc (1, sizeof (PLINE));
  if (res == NULL)
    return NULL;

  poly_IniContour (res);

  Vcopy (res->head.point, node->point);
  Vcopy (res->head.orig_point0, node->orig_point0);
  Vcopy (res->head.orig_point1, node->orig_point1);
  res->head.p0 = node->p0;
  res->head.p1 = node->p1;
  res->head.is_round = node->is_round;
  res->head.cx = node->cx;
  res->head.cy = node->cy;
  res->head.radius = node->radius;
#warning THIS WILL BE BOGUS IF WE GET A CIRCULAR CONTOUR STARTING AT THE HEAD.. NEED 2ND POINT TO DETMERMINE BOUNDS
  cntrbox_adjust (res, res->head.point);
  g_slice_free (VNODE, node);

  return res;
}

void
poly_ClrContour (PLINE * c)
{
  VNODE *cur;

  assert (c != NULL);
  while ((cur = NEXT_EDGE (&c->head)) != &c->head)
    {
      poly_ExclVertex (cur);
      g_slice_free (VNODE, cur);
    }
  free (c->tristrip_vertices);
  c->tristrip_vertices = NULL;
  c->tristrip_num_vertices = 0;
  poly_IniContour (c);
}

void
poly_DelContour (PLINE ** c)
{
  VNODE *cur, *prev;

  if (*c == NULL)
    return;
  for (cur = PREV_EDGE (&(*c)->head); cur != &(*c)->head; cur = prev)
    {
      prev = PREV_EDGE (cur);
      if (cur->cvc_next != NULL)
	{
	  free (cur->cvc_next);
	  free (cur->cvc_prev);
	}
      g_slice_free (VNODE, cur);
    }
  if ((*c)->head.cvc_next != NULL)
    {
      free ((*c)->head.cvc_next);
      free ((*c)->head.cvc_prev);
    }
  /* FIXME -- strict aliasing violation.  */
  if ((*c)->tree)
    {
      rtree_t *r = (*c)->tree;
      r_destroy_tree (&r);
    }
  free ((*c)->tristrip_vertices);
  free (*c), *c = NULL;
}

void
poly_PreContour (PLINE * C, BOOLp optimize)
{
  double area = 0;
  VNODE *p, *c;
//  Vector p1, p2;

  assert (C != NULL);

  if (optimize)
    {
      for (c = NEXT_VERTEX ((p = &C->head)); c != &C->head; c = NEXT_VERTEX (p = c))
	{
	  /* if the previous node is on the same line with this one, we should remove it */
//	  Vsub2 (p1, c->point, p->point);
//	  Vsub2 (p2, NEXT_VERTEX (c)->point, c->point);
	  /* If the product below is zero then
	   * the points on either side of c 
	   * are on the same line!
	   * So, remove the point c
	   */

#warning BROKEN FOR CIRCULAR CONTOURS
//	  if (vect_det2 (p1, p2) == 0)
          if (0)
	    {
	      poly_ExclVertex (c);
	      g_slice_free (VNODE, c);
	      c = p;
	    }
	}
    }
  C->Count = 0;
  C->xmin = C->xmax = C->head.point[0];
  C->ymin = C->ymax = C->head.point[1];

  p = PREV_VERTEX ((c = &C->head));
  if (c != p)
    {
      do
	{
	  /* calculate area for orientation */
	  area +=
	    (double) (p->point[0] - c->point[0]) * (p->point[1] +
						    c->point[1]);
#warning NEED TO CATER FOR A ROUND SEGMENT
	  cntrbox_adjust (C, c->point);
          if (p->is_round)
            {
              Angle start_angle;
              Angle end_angle;
              Angle delta_angle;
              BoxType arc_bound;

              start_angle = atan2 ((p->point[1] - p->cy), -(p->point[0] - p->cx)) / M180;
              end_angle   = atan2 ((c->point[1] - p->cy), -(c->point[0] - p->cx)) / M180;

#warning delta angle calculation looks rather suspect - wont work for arcs > 180 degrees span
              delta_angle = end_angle - start_angle;

              if (delta_angle > 180.) delta_angle -= 360.;
              if (delta_angle < -180.) delta_angle += 360.;

              arc_bound = calc_thin_arc_bounds (p->cx, p->cy, p->radius, p->radius, start_angle, delta_angle);

              C->xmin = min (C->xmin, arc_bound.X1);
              C->xmax = max (C->xmax, arc_bound.X2);
              C->ymin = min (C->ymin, arc_bound.Y1);
              C->ymax = max (C->ymax, arc_bound.Y2);
            }
	  C->Count++;
	}
      while ((c = NEXT_VERTEX (p = c)) != &C->head);
    }
  C->area = ABS (area);
  if (C->Count > 2)
    C->Flags.orient = ((area < 0) ? PLF_INV : PLF_DIR);
  C->tree = (rtree_t *)make_edge_tree (C);
}				/* poly_PreContour */

static int
flip_cb (const BoxType * b, void *cl)
{
  struct seg *s = (struct seg *) b;
  s->v = PREV_EDGE (s->v);
  return 1;
}

static void
test_polyInvContour (void)
{
  static bool done = false;
  PLINE *contour;
  Vector v;

  if (done)
    return;

  printf ("Testing inv_contour\n");

  v[0] = 0, v[1] = 0;           contour = poly_NewContour (poly_CreateNodeArcApproximation (v, 0, 0, 1));
  v[0] = 0, v[1] = 2; poly_InclVertex (contour->head.prev, poly_CreateNodeArcApproximation (v, 0, 0, 3));
  v[0] = 0, v[1] = 4; poly_InclVertex (contour->head.prev, poly_CreateNodeArcApproximation (v, 0, 0, 5));
  v[0] = 0; v[1] = 6; poly_InclVertex (contour->head.prev, poly_CreateNodeArcApproximation (v, 0, 0, 7));

  pline_dump (&contour->head);
  poly_InvContour (contour);
  pline_dump (&contour->head);

  poly_FreeContours (&contour);

  done = true;
}

void
poly_InvContour (PLINE * c)
{
  VNODE *cur, *next;
#ifndef NDEBUG
  int r;
#endif

  /* Stash the first data which will get over-written in the loop */

  bool stash_is_round = c->head.is_round;
  Coord stash_cx = c->head.cx;
  Coord stash_cy = c->head.cy;
  Coord stash_radius = c->head.radius;
  Vector stash_orig_point0;
  Vector stash_orig_point1;
  double stash_p0 = c->head.p0;
  double stash_p1 = c->head.p1;

  bool next_is_round;
  Coord next_cx;
  Coord next_cy;
  Coord next_radius;
  Vector next_orig_point0;
  Vector next_orig_point1;
  double next_p0;
  double next_p1;

  Vcopy (stash_orig_point0, c->head.orig_point0);
  Vcopy (stash_orig_point1, c->head.orig_point1);

//  printf ("poly_InvContour\n");

  assert (c != NULL);
  cur = &c->head;
  do
    {
      /* Swap the attachement of round contour information */
      next_is_round = cur->next->is_round;
      next_cx = cur->next->cx;
      next_cy = cur->next->cy;
      next_radius = cur->next->radius;
      Vcopy (next_orig_point0, cur->next->orig_point0);
      Vcopy (next_orig_point1, cur->next->orig_point1);
      next_p0 = cur->next->p0;
      next_p1 = cur->next->p1;

      cur->next->is_round = stash_is_round;
      cur->next->cx = stash_cx;
      cur->next->cy = stash_cy;
      cur->next->radius = stash_radius;
      Vcopy (cur->next->orig_point0, stash_orig_point1);
      Vcopy (cur->next->orig_point1, stash_orig_point0);
      cur->next->p0 = 1.0 - stash_p1;
      cur->next->p1 = 1.0 - stash_p0;

      stash_is_round = next_is_round;
      stash_cx = next_cx;
      stash_cy = next_cy;
      stash_radius = next_radius;
      Vcopy (stash_orig_point0, next_orig_point0);
      Vcopy (stash_orig_point1, next_orig_point1);
      stash_p0 = next_p0;
      stash_p1 = next_p1;

      next = NEXT_EDGE (cur);
      NEXT_EDGE(cur) = PREV_EDGE (cur);
      PREV_EDGE (cur) = next;

      /* fix the segment tree */
    }
  while ((cur = next) != &c->head);

  c->Flags.orient ^= 1;
  if (c->tree)
    {
#ifndef NDEBUG
      r =
#endif
        r_search (c->tree, NULL, NULL, flip_cb, NULL);
#ifndef NDEBUG
      assert (r == c->Count);
#endif
    }
}

void
poly_ExclVertex (VNODE * node)
{
  assert (node != NULL);
  if (node->cvc_next)
    {
      free (node->cvc_next);
      free (node->cvc_prev);
    }
  NEXT_VERTEX (PREV_VERTEX (node)) = NEXT_VERTEX (node);
  PREV_VERTEX (NEXT_VERTEX (node)) = PREV_VERTEX (node);
}

static void
poly_InclVertex_int (VNODE * after, VNODE * node)
{
  assert (after != NULL);
  assert (node != NULL);

  node->prev = after;
  node->next = after->next;
  after->next = after->next->prev = node;
}

void
poly_InclVertex (VNODE * after, VNODE * node)
{
  double a, b;
  assert (after != NULL);
  assert (node != NULL);

  PREV_VERTEX (node) = after;
  NEXT_VERTEX (node) = NEXT_VERTEX (after);
  NEXT_VERTEX (after) = PREV_VERTEX (NEXT_VERTEX (after)) = node;

  /* Fill in the orig_point1 data looking at the orig_point0 data of neighbouring
   * edges. This is because edges only get orig_point0 set initially.
   */
  after->orig_point1[0] = node->orig_point0[0];
  after->orig_point1[1] = node->orig_point0[1];
  node->orig_point1[0] = node->next->orig_point0[0];
  node->orig_point1[1] = node->next->orig_point0[1];

  /* remove points on same line */
  if (PREV_VERTEX (PREV_VERTEX (node)) == node)
    return;			/* we don't have 3 points in the poly yet */

  /* NB: a-b below is the two-dimensional cross product of the vectors
   *     node->prev->prev->point -> node->prev->point  and
   *     node->prev->prev->point -> node->point.
   *
   * Its magnitude is the area of the parallelogram with those vectors as sides.
   * If the vectors are colinear, this is zero.
   */
  a = (node->point[1] - PREV_VERTEX (PREV_VERTEX (node))->point[1]);
  a *= (PREV_VERTEX (node)->point[0] - PREV_VERTEX (PREV_VERTEX (node))->point[0]);
  b = (node->point[0] - PREV_VERTEX (PREV_VERTEX (node))->point[0]);
  b *= (PREV_VERTEX (node)->point[1] - PREV_VERTEX (PREV_VERTEX (node))->point[1]);

//  printf ("a-b = %f\n", a-b);

  /* XXX: HMM - This doesn't seem to be involved when extra points are left in polygon contours after boolean operations */
  if (0)
//  if (fabs (a - b) < 1000000) //EPSILON &&
//      !node->prev->is_round && !node->is_round)
//      !node->prev->is_round && !node->is_round)
    {
      VNODE *t = PREV_VERTEX (node);
      NEXT_VERTEX (PREV_VERTEX (t)) = node;
      PREV_VERTEX (node) = PREV_VERTEX (t);
      g_slice_free (VNODE, t);
    }
}

BOOLp
poly_CopyContour (PLINE ** dst, PLINE * src)
{
  VNODE *cur, *newnode;

  assert (src != NULL);
  *dst = NULL;
  *dst = poly_NewContour (poly_CreateNodeFull (src->head.point, src->head.is_round, src->head.cx, src->head.cy, src->head.radius));
  if (*dst == NULL)
    return FALSE;

  Vcopy ((*dst)->head.orig_point0, src->head.orig_point0);
  Vcopy ((*dst)->head.orig_point1, src->head.orig_point1);
  (*dst)->head.p0 = src->head.p0;
  (*dst)->head.p1 = src->head.p1;

  (*dst)->Count = src->Count;
  (*dst)->Flags.orient = src->Flags.orient;
  (*dst)->xmin = src->xmin, (*dst)->xmax = src->xmax;
  (*dst)->ymin = src->ymin, (*dst)->ymax = src->ymax;
  (*dst)->area = src->area;

  for (cur = NEXT_EDGE (&src->head); cur != &src->head; cur = NEXT_VERTEX (cur))
    {
      if ((newnode = poly_CreateNodeFull (cur->point, cur->is_round, cur->cx, cur->cy, cur->radius)) == NULL)
	return FALSE;

      Vcopy (newnode->orig_point0, cur->orig_point0);
      Vcopy (newnode->orig_point1, cur->orig_point1);
      newnode->p0 = cur->p0;
      newnode->p1 = cur->p1;

      // newnode->Flags = cur->Flags;
      poly_InclVertex_int (PREV_EDGE (&(*dst)->head), newnode);
    }
  (*dst)->tree = (rtree_t *)make_edge_tree (*dst);
  return TRUE;
}

/**********************************************************************/
/* polygon routines */

static BOOLp
poly_Copy1 (POLYAREA * dst, const POLYAREA * src)
{
  PLINE *cur, **last = &dst->contours;

  *last = NULL;
  dst->f = dst->b = dst;

  for (cur = src->contours; cur != NULL; cur = cur->next)
    {
      if (!poly_CopyContour (last, cur))
        return FALSE;
      r_insert_entry (dst->contour_tree, (BoxType *) * last, 0);
      last = &(*last)->next;
    }

  return TRUE;
}

BOOLp
poly_Copy0 (POLYAREA ** dst, const POLYAREA * src)
{
  *dst = NULL;
  if (src == NULL)
    return TRUE;

  if ((*dst = poly_Create ()) == NULL || !poly_Copy1 (*dst, src))
    return FALSE;

  return TRUE;
}

void
poly_M_Incl (POLYAREA ** list, POLYAREA * a)
{
  if (*list == NULL)
    a->f = a->b = a, *list = a;
  else
    {
      a->f = *list;
      a->b = (*list)->b;
      (*list)->b = (*list)->b->f = a;
    }
}

BOOLp
poly_M_Copy0 (POLYAREA ** dst, const POLYAREA * srcfst)
{
  const POLYAREA *src = srcfst;
  POLYAREA *di;

  *dst = NULL;
  if (src == NULL)
    return TRUE;

  do
    {
      if ((di = poly_Create ()) == NULL || !poly_Copy1 (di, src))
	return FALSE;
      poly_M_Incl (dst, di);
    }
  while ((src = src->f) != srcfst);
  return TRUE;
}

BOOLp
poly_InclContour (POLYAREA * p, PLINE * c)
{
  PLINE *tmp;

  if ((c == NULL) || (p == NULL))
    return FALSE;
  if (c->Flags.orient == PLF_DIR)
    {
      if (p->contours != NULL)
	return FALSE;
      p->contours = c;
    }
  else
    {
      if (p->contours == NULL)
	return FALSE;
      /* link at front of hole list */
      tmp = p->contours->next;
      p->contours->next = c;
      c->next = tmp;
    }
  r_insert_entry (p->contour_tree, (BoxType *) c, 0);
  return TRUE;
}

typedef struct pip
{
  int f;
  Vector p;
  jmp_buf env;
} pip;


static int
crossing (const BoxType * b, void *cl)
{
  struct seg *s = (struct seg *) b;
  struct pip *p = (struct pip *) cl;

#warning BREAKAGE!
/* XXX: This should probably not count crossings of the ray which are outside the p0-p1 bounds of the actual line segment! */
/* XXX: Using intersected (non orig_point{0,1} data), causes inconsistencies and a crash with some examples */

#if 1
  if (EDGE_BACKWARD_VERTEX (s->v)->point[1] <= p->p[1])
#else
  if (s->v->orig_point0[1] <= p->p[1])
#endif
    {
#if 1
      if (EDGE_FORWARD_VERTEX (s->v)->point[1] > p->p[1])
#else
      if (s->v->orig_point1[1] > p->p[1])
#endif
	{
	  Vector v1, v2;
	  long long cross;
#if 1
	  Vsub2 (v1, EDGE_FORWARD_VERTEX (s->v)->point, EDGE_BACKWARD_VERTEX (s->v)->point);
	  Vsub2 (v2, p->p, EDGE_BACKWARD_VERTEX (s->v)->point);
#else
	  Vsub2 (v1, s->v->orig_point1, s->v->orig_point0);
	  Vsub2 (v2, p->p, s->v->orig_point0);
#endif
#warning THIS WILL PROBABLY OVERFLOW FOR COORDINATES > 32 BIT
          /* XXX Convert to double? */
	  cross = (long long) v1[0] * v2[1] - (long long) v2[0] * v1[1];
	  if (cross == 0)
	    {
	      p->f = 1;
	      longjmp (p->env, 1);
	    }
	  if (cross > 0)
	    p->f += 1;
	}
    }
  else
    {
#if 1
      if (EDGE_FORWARD_VERTEX (s->v)->point[1] <= p->p[1])
#else
      if (s->v->orig_point1[1] <= p->p[1])
#endif
	{
	  Vector v1, v2;
	  long long cross;
/* XXX: COULD USE ORIGINAL GEOMETRY, ALSO PROBABLY NEEDS UPDATING FOR CURVES */
#if 1
	  Vsub2 (v1, EDGE_FORWARD_VERTEX (s->v)->point, EDGE_BACKWARD_VERTEX (s->v)->point);
	  Vsub2 (v2, p->p, EDGE_BACKWARD_VERTEX (s->v)->point);
#else
	  Vsub2 (v1, s->v->orig_point1, s->v->orig_point0);
	  Vsub2 (v2, p->p, s->v->orig_point0);
#endif
#warning THIS WILL PROBABLY OVERFLOW FOR COORDINATES > 32 BIT
          /* XXX Convert to double? */
	  cross = (long long) v1[0] * v2[1] - (long long) v2[0] * v1[1];
	  if (cross == 0)
	    {
	      p->f = 1;
	      longjmp (p->env, 1);
	    }
	  if (cross < 0)
	    p->f -= 1;
	}
    }
  return 1;
}

int
poly_InsideContour (PLINE * c, Vector p)
{
  struct pip info;
  BoxType ray;

  if (!cntrbox_pointin (c, p))
    return FALSE;
  info.f = 0;
  info.p[0] = ray.X1 = p[0];
  info.p[1] = ray.Y1 = p[1];
  ray.X2 = COORD_MAX;
  ray.Y2 = p[1] + 1;
  if (setjmp (info.env) == 0)
    r_search (c->tree, &ray, NULL, crossing, &info);
  return info.f;
}

BOOLp
poly_CheckInside (POLYAREA * p, Vector v0)
{
  PLINE *cur;

  if ((p == NULL) || (v0 == NULL) || (p->contours == NULL))
    return FALSE;
  cur = p->contours;
  if (poly_InsideContour (cur, v0))
    {
      for (cur = cur->next; cur != NULL; cur = cur->next)
	if (poly_InsideContour (cur, v0))
	  return FALSE;
      return TRUE;
    }
  return FALSE;
}

BOOLp
poly_M_CheckInside (POLYAREA * p, Vector v0)
{
  POLYAREA *cur;

  if ((p == NULL) || (v0 == NULL))
    return FALSE;
  cur = p;
  do
    {
      if (poly_CheckInside (cur, v0))
	return TRUE;
    }
  while ((cur = cur->f) != p);
  return FALSE;
}

static double
dot (Vector A, Vector B)
{
  return (double)A[0] * (double)B[0] + (double)A[1] * (double)B[1];
}

/* Compute whether point is inside a triangle formed by 3 other pints */
/* Algorithm from http://www.blackpawn.com/texts/pointinpoly/default.html */
static int
point_in_triangle (Vector A, Vector B, Vector C, Vector P)
{
  Vector v0, v1, v2;
  double dot00, dot01, dot02, dot11, dot12;
  double invDenom;
  double u, v;

  /* Compute vectors */
  v0[0] = C[0] - A[0];  v0[1] = C[1] - A[1];
  v1[0] = B[0] - A[0];  v1[1] = B[1] - A[1];
  v2[0] = P[0] - A[0];  v2[1] = P[1] - A[1];

  /* Compute dot products */
  dot00 = dot (v0, v0);
  dot01 = dot (v0, v1);
  dot02 = dot (v0, v2);
  dot11 = dot (v1, v1);
  dot12 = dot (v1, v2);

  /* Compute barycentric coordinates */
  invDenom = 1. / (dot00 * dot11 - dot01 * dot01);
  u = (dot11 * dot02 - dot01 * dot12) * invDenom;
  v = (dot00 * dot12 - dot01 * dot02) * invDenom;

  /* Check if point is in triangle */
  return (u > 0.0) && (v > 0.0) && (u + v < 1.0);
}


/* Returns the dot product of Vector A->B, and a vector
 * orthogonal to Vector C->D. The result is not normalisd, so will be
 * weighted by the magnitude of the C->D vector.
 */
static double
dot_orthogonal_to_direction (Vector A, Vector B, Vector C, Vector D)
{
  Vector l1, l2, l3;
  l1[0] = B[0] - A[0];  l1[1] = B[1] - A[1];
  l2[0] = D[0] - C[0];  l2[1] = D[1] - C[1];

  l3[0] = -l2[1];
  l3[1] = l2[0];

  return dot (l1, l3);
}

/* Algorithm from http://www.exaflop.org/docs/cgafaq/cga2.html
 *
 * "Given a simple polygon, find some point inside it. Here is a method based
 * on the proof that there exists an internal diagonal, in [O'Rourke, 13-14].
 * The idea is that the midpoint of a diagonal is interior to the polygon.
 *
 * 1.  Identify a convex vertex v; let its adjacent vertices be a and b.
 * 2.  For each other vertex q do:
 * 2a. If q is inside avb, compute distance to v (orthogonal to ab).
 * 2b. Save point q if distance is a new min.
 * 3.  If no point is inside, return midpoint of ab, or centroid of avb.
 * 4.  Else if some point inside, qv is internal: return its midpoint."
 *
 * [O'Rourke]: Computational Geometry in C (2nd Ed.)
 *             Joseph O'Rourke, Cambridge University Press 1998,
 *             ISBN 0-521-64010-5 Pbk, ISBN 0-521-64976-5 Hbk
 */
static void
poly_ComputeInteriorPoint (PLINE *poly, Vector v)
{
  VNODE *pt1, *pt2, *pt3, *q;
  VNODE *min_q = NULL;
  double dist;
  double min_dist = 0.0;
  double dir = (poly->Flags.orient == PLF_DIR) ? 1. : -1;

  /* Find a convex node on the polygon */
  pt1 = &poly->head;
  do
    {
      double dot_product;

      pt2 = NEXT_VERTEX (pt1);
      pt3 = NEXT_VERTEX (pt2);

      dot_product = dot_orthogonal_to_direction (pt1->point, pt2->point,
                                                 pt3->point, pt2->point);

      if (dot_product * dir > 0.)
        break;
    }
  while ((pt1 = NEXT_VERTEX (pt1)) != &poly->head);

  /* Loop over remaining vertices */
  q = pt3;
  do
    {
      /* Is current vertex "q" outside pt1 pt2 pt3 triangle? */
      if (!point_in_triangle (pt1->point, pt2->point, pt3->point, q->point))
        continue;

      /* NO: compute distance to pt2 (v) orthogonal to pt1 - pt3) */
      /*     Record minimum */
      dist = dot_orthogonal_to_direction (q->point, pt2->point, pt1->point, pt3->point);
      if (min_q == NULL || dist < min_dist) {
        min_dist = dist;
        min_q = q;
      }
    }
  while ((q = NEXT_VERTEX (q)) != pt2);

  /* Were any "q" found inside pt1 pt2 pt3? */
  if (min_q == NULL) {
    /* NOT FOUND: Return midpoint of pt1 pt3 */
    v[0] = (pt1->point[0] + pt3->point[0]) / 2;
    v[1] = (pt1->point[1] + pt3->point[1]) / 2;
  } else {
    /* FOUND: Return mid point of min_q, pt2 */
    v[0] = (pt2->point[0] + min_q->point[0]) / 2;
    v[1] = (pt2->point[1] + min_q->point[1]) / 2;
  }
}


/* NB: This function assumes the caller _knows_ the contours do not
 *     intersect. If the contours intersect, the result is undefined.
 *     It will return the correct result if the two contours share
 *     common points beteween their contours. (Identical contours
 *     are treated as being inside each other).
 */
int
poly_ContourInContour (PLINE * poly, PLINE * inner)
{
  Vector point;
  assert (poly != NULL);
  assert (inner != NULL);
  if (cntrbox_inside (inner, poly))
    {
      /* We need to prove the "inner" contour is not outside
       * "poly" contour. If it is outside, we can return.
       */
      if (!poly_InsideContour (poly, inner->head.point))
        return 0;

      poly_ComputeInteriorPoint (inner, point);
      return poly_InsideContour (poly, point);
    }
  return 0;
}

void
poly_Init (POLYAREA * p)
{
  p->f = p->b = p;
  p->contours = NULL;
  p->contour_tree = r_create_tree (NULL, 0, 0);
  p->parentage = no_parentage;
}

POLYAREA *
poly_Create (void)
{
  POLYAREA *res;

  if ((res = (POLYAREA *)malloc (sizeof (POLYAREA))) != NULL)
    poly_Init (res);
  return res;
}

void
poly_FreeContours (PLINE ** pline)
{
  PLINE *pl;

  while ((pl = *pline) != NULL)
    {
      *pline = pl->next;
      poly_DelContour (&pl);
    }
}

void
poly_Free (POLYAREA ** p)
{
  POLYAREA *cur;

  if (*p == NULL)
    return;
  for (cur = (*p)->f; cur != *p; cur = (*p)->f)
    {
      poly_FreeContours (&cur->contours);
      r_destroy_tree (&cur->contour_tree);
      cur->f->b = cur->b;
      cur->b->f = cur->f;
      free (cur);
    }
  poly_FreeContours (&cur->contours);
  r_destroy_tree (&cur->contour_tree);

  /* Free parentage information - assume all linked polygons share this, so only need to do it for the past polygon */
  if ((*p)->parentage.a != NULL)
    poly_Free (&(*p)->parentage.a);
  if ((*p)->parentage.b != NULL)
    poly_Free (&(*p)->parentage.b);

  free (*p), *p = NULL;
}

static BOOLp
inside_sector (VNODE * pn, Vector p2)
{
  Vector cdir, ndir, pdir;
  int p_c, n_c, p_n;

  assert (pn != NULL);
  vect_sub (cdir, p2, pn->point);
  vect_sub (pdir, pn->point, PREV_VERTEX (pn)->point);
  vect_sub (ndir, NEXT_VERTEX (pn)->point, pn->point);

  p_c = vect_det2 (pdir, cdir) >= 0;
  n_c = vect_det2 (ndir, cdir) >= 0;
  p_n = vect_det2 (pdir, ndir) >= 0;

  if ((p_n && p_c && n_c) || ((!p_n) && (p_c || n_c)))
    return TRUE;
  else
    return FALSE;
}				/* inside_sector */

/* returns TRUE if bad contour */
BOOLp
poly_ChkContour (PLINE * a)
{
  VNODE *a1, *a2, *hit1, *hit2;
  Vector i1, i2;
  int icnt;

  assert (a != NULL);
  a1 = &a->head;
  do
    {
      a2 = a1;
      do
	{
#warning THIS DOES NOT TAKE INTO ACCOUNT arc-arc and arc-line segments
	  if (!node_neighbours (a1, a2) &&
	      (icnt = vect_inters2 (a1->point, a1->next->point, 0.0, 1.0,
				    a2->point, a2->next->point, 0.0, 1.0,
				    i1, i2, NULL, NULL, NULL, NULL)) > 0)
	    {
	      if (icnt > 1)
		return TRUE;

	      if (vect_dist2 (i1, a1->point) < EPSILON)
		hit1 = a1;
	      else if (vect_dist2 (i1, a1->next->point) < EPSILON)
		hit1 = a1->next;
	      else
		hit1 = NULL;

	      if (vect_dist2 (i1, a2->point) < EPSILON)
		hit2 = a2;
	      else if (vect_dist2 (i1, a2->next->point) < EPSILON)
		hit2 = a2->next;
	      else
		hit2 = NULL;

	      if ((hit1 == NULL) && (hit2 == NULL))
		{
		  /* If the intersection didn't land on an end-point of either
		   * line, we know the lines cross and we return TRUE.
		   */
		  return TRUE;
		}
	      else if (hit1 == NULL)
		{
		/* An end-point of the second line touched somewhere along the
		   length of the first line. Check where the second line leads. */
		  if (inside_sector (hit2, a1->point) !=
		      inside_sector (hit2, a1->next->point))
		    return TRUE;
		}
	      else if (hit2 == NULL)
		{
		/* An end-point of the first line touched somewhere along the
		   length of the second line. Check where the first line leads. */
		  if (inside_sector (hit1, a2->point) !=
		      inside_sector (hit1, a2->next->point))
		    return TRUE;
		}
	      else
		{
		/* Both lines intersect at an end-point. Check where they lead. */
		  if (inside_sector (hit1, hit2->prev->point) !=
		      inside_sector (hit1, hit2->next->point))
		    return TRUE;
		}
	    }
	}
      while ((a2 = a2->next) != &a->head);
    }
  while ((a1 = a1->next) != &a->head);
  return FALSE;
}


BOOLp
poly_Valid (POLYAREA * p)
{
  PLINE *c;

  if ((p == NULL) || (p->contours == NULL))
    return FALSE;

  if (p->contours->Flags.orient == PLF_INV || poly_ChkContour (p->contours))
    {
#ifndef NDEBUG
      VNODE *v, *n;
      DEBUGP ("Invalid Outer PLINE\n");
      if (p->contours->Flags.orient == PLF_INV)
	DEBUGP ("failed orient\n");
      if (poly_ChkContour (p->contours))
	DEBUGP ("failed self-intersection\n");
      v = &p->contours->head;
      do
	{
	  n = NEXT_VERTEX (v);
	  pcb_fprintf (stderr, "Line [%#mS %#mS %#mS %#mS 100 100 \"\"]\n",
		   v->point[0], v->point[1], n->point[0], n->point[1]);
	}
      while ((v = NEXT_VERTEX (v)) != &p->contours->head);
#endif
      return FALSE;
    }
  for (c = p->contours->next; c != NULL; c = c->next)
    {
      if (c->Flags.orient == PLF_DIR ||
	  poly_ChkContour (c) || !poly_ContourInContour (p->contours, c))
	{
#ifndef NDEBUG
	  VNODE *v, *n;
	  DEBUGP ("Invalid Inner PLINE orient = %d\n", c->Flags.orient);
	  if (c->Flags.orient == PLF_DIR)
	    DEBUGP ("failed orient\n");
	  if (poly_ChkContour (c))
	    DEBUGP ("failed self-intersection\n");
	  if (!poly_ContourInContour (p->contours, c))
	    DEBUGP ("failed containment\n");
	  v = &c->head;
	  do
	    {
	      n = NEXT_VERTEX (v);
	      pcb_fprintf (stderr, "Line [%#mS %#mS %#mS %#mS 100 100 \"\"]\n",
		       v->point[0], v->point[1], n->point[0], n->point[1]);
	    }
	  while ((v = NEXT_VERTEX (v)) != &c->head);
#endif
	  return FALSE;
	}
    }
  return TRUE;
}


Vector vect_zero = { (long) 0, (long) 0 };

/*********************************************************************/
/*             L o n g   V e c t o r   S t u f f                     */
/*********************************************************************/

void
vect_init (Vector v, double x, double y)
{
  v[0] = (long) x;
  v[1] = (long) y;
}				/* vect_init */

#define Vzero(a)   ((a)[0] == 0. && (a)[1] == 0.)

#define Vsub(a,b,c) {(a)[0]=(b)[0]-(c)[0];(a)[1]=(b)[1]-(c)[1];}

int
vect_equal (Vector v1, Vector v2)
{
  return (v1[0] == v2[0] && v1[1] == v2[1]);
}				/* vect_equal */


void
vect_sub (Vector res, Vector v1, Vector v2)
{
Vsub (res, v1, v2)}		/* vect_sub */

void
vect_min (Vector v1, Vector v2, Vector v3)
{
  v1[0] = (v2[0] < v3[0]) ? v2[0] : v3[0];
  v1[1] = (v2[1] < v3[1]) ? v2[1] : v3[1];
}				/* vect_min */

void
vect_max (Vector v1, Vector v2, Vector v3)
{
  v1[0] = (v2[0] > v3[0]) ? v2[0] : v3[0];
  v1[1] = (v2[1] > v3[1]) ? v2[1] : v3[1];
}				/* vect_max */

double
vect_len2 (Vector v)
{
  return ((double) v[0] * v[0] + (double) v[1] * v[1]);	/* why sqrt? only used for compares */
}

double
vect_dist2 (Vector v1, Vector v2)
{
  double dx = v1[0] - v2[0];
  double dy = v1[1] - v2[1];

  return (dx * dx + dy * dy);	/* why sqrt */
}

/* value has sign of angle between vectors */
double
vect_det2 (Vector v1, Vector v2)
{
  return (((double) v1[0] * v2[1]) - ((double) v2[0] * v1[1]));
}

static double
vect_m_dist (Vector v1, Vector v2)
{
  double dx = v1[0] - v2[0];
  double dy = v1[1] - v2[1];
  double dd = (dx * dx + dy * dy);	/* sqrt */

  if (dx > 0)
    return +dd;
  if (dx < 0)
    return -dd;
  if (dy > 0)
    return +dd;
  return -dd;
}				/* vect_m_dist */

/* Compute the parameter of the closest point to 'point' on the (infinite) line
 * l1-l2. param == 0.0 corresponds to point projecting onto l1, param == 1.0
 * corresponds to point projecting onto l2. The returned param may be outside
 * the 0.0-1.0 range.
 */
static double
calculate_line_point_intersection (Vector l1, Vector l2, Vector point)
{
  Vector l1l2;
  Vector l1point;
  double param;

  Vsub2 (l1l2,    l2,    l1);
  Vsub2 (l1point, point, l1);

  param = dot (l1point, l1l2) / dot (l1l2, l1l2);

//  pcb_printf ("  Line (%mm, %mm)-(%mm, %mm)  Point (%mm, %mm)  Param = %f <<<<<<\n",
//              l1[0], l1[1], l2[0], l2[1], point[0], point[1], param);

  return param;
}

/*
vect_inters2
 (C) 1993 Klamer Schutte
 (C) 1997 Michael Leonov, Alexey Nikitin
*/

int
vect_inters2 (Vector p1, Vector p2, double s1, double s2,
              Vector q1, Vector q2, double t1, double t2,
              Vector i1, Vector i2,
              double *i1s, double *i1t,
              double *i2s, double *i2t)
{
  double s, t, deel;
  double rpx, rpy, rqx, rqy;

  /* DUMMY */
  if (i1s != NULL) *i1s = -1.0;
  if (i1t != NULL) *i1t = -1.0;
  if (i2s != NULL) *i2s = -1.0;
  if (i2t != NULL) *i2t = -1.0;

  /* XXX: Presumably the r-tree search which usually gets us into this function means this test ought not to hit? */
  if (max (p1[0], p2[0]) < min (q1[0], q2[0]) ||
      max (q1[0], q2[0]) < min (p1[0], p2[0]) ||
      max (p1[1], p2[1]) < min (q1[1], q2[1]) ||
      max (q1[1], q2[1]) < min (p1[1], p2[1]))
    return 0;

  rpx = p2[0] - p1[0];
  rpy = p2[1] - p1[1];
  rqx = q2[0] - q1[0];
  rqy = q2[1] - q1[1];

  deel = rpy * rqx - rpx * rqy;	/* -vect_det(rp,rq); */

  /* coordinates are 30-bit integers so deel will be exactly zero
   *                 ^^^^^^^^^^^^^^^_____ NOT ANYMORE!!! (pcjc).
   * if the lines are parallel
   */

  if (deel == 0)		/* parallel */
    {
      Vector p1_trimmed;
      Vector p2_trimmed;
      Vector q1_trimmed;
      Vector q2_trimmed;

//      double sq1;
//      double sq2;
//      double tp1;
//      double tp2;

      p1_trimmed[0] = p1[0] + ROUND (s1 * rpx);
      p1_trimmed[1] = p1[1] + ROUND (s1 * rpy);
      p2_trimmed[0] = p1[0] + ROUND (s2 * rpx);
      p2_trimmed[1] = p1[1] + ROUND (s2 * rpy);

      q1_trimmed[0] = q1[0] + ROUND (t1 * rqx);
      q1_trimmed[1] = q1[1] + ROUND (t1 * rqy);
      q2_trimmed[0] = q1[0] + ROUND (t2 * rqx);
      q2_trimmed[1] = q1[1] + ROUND (t2 * rqy);

      /* Calculate the parameter values on each line (p1-p2 has s param, q1-q2 has t param), of the end-points of the other line */
//      sq1 = calculate_line_point_intersection (p1, p2, q1_trimmed);
//      sq2 = calculate_line_point_intersection (p1, p2, q2_trimmed);
//      tp1 = calculate_line_point_intersection (q1, q2, p1_trimmed);
//      tp2 = calculate_line_point_intersection (q1, q2, p2_trimmed);

      {
      double dc1, dc2, d1, d2, h;	/* Check to see whether p1-p2 and q1-q2 are on the same line */
      Vector hp1, hq1, hp2, hq2, q1p1, q1q2;
      double hs1, hs2, ht1, ht2;

      Vsub2 (q1p1, q1, p1);
      Vsub2 (q1q2, q1, q2);


      /* XXX: Isn't this the same as the deel=0 test?) */
      /* If this product is not zero then p1-p2 and q1-q2 are not on same line! */
      if (vect_det2 (q1p1, q1q2) != 0)
	return 0;
      dc1 = 0;			/* m_len(p1 - p1) */


      /* XXX: Using trimmed coordinates is a potential problem, as we loose the advantage of tracing everything back to undisturbed geometery */
      dc2 = vect_m_dist (p1_trimmed, p2_trimmed); /* Signed square length p1-p2, positive if p1 is "rightmost" of p2 */
      d1 = vect_m_dist (p1_trimmed, q1_trimmed);
      d2 = vect_m_dist (p1_trimmed, q2_trimmed);

/* Sorting the independent points from small to large */
      Vcpy2 (hp1, p1_trimmed);
      Vcpy2 (hp2, p2_trimmed);
      Vcpy2 (hq1, q1_trimmed);
      Vcpy2 (hq2, q2_trimmed);
      hs1 = s1;
      hs2 = s2;
      ht1 = t1;
      ht2 = t2;

      if (dc1 > dc2)
	{/* dc2 is negative, e.g. p2 is "rightmost" of p1. */
          /* hv and h are used as help-variable. */
	  Vswp2 (hp1, hp2);            /* Make hp1 the "rightmost" point of line p1-p2, hp2 the "leftmost" */
	  h = dc1, dc1 = dc2, dc2 = h; /* dc1 and dc2 represent signed distances from hp1 and hp2 to p1 */
	  h = hs1, hs1 = hs2, hs2 = h;
	}
      if (d1 > d2)
	{ /* p1-q1 > p1-q2 ... IE q1-p1 < q2-p1 .. IE q1 < q2... q1 is "left" of q2 */
	  Vswp2 (hq1, hq2);            /* Make hq1 the "righmost" point of the line q1-q2 */
	  h = d1, d1 = d2, d2 = h;     /* d1 and d2 represent signed distances from p1 to hq1 and hq2 */
	  h = ht1, ht1 = ht2, ht2 = h;
	}

/* Now the line-pieces are compared */

      if (dc1 < d1)
	{               /* p1-hp1 < p1-hq1  ... IE  hp1 > hq1 .... hp1 is "rightmost" of hq1 */
	  if (dc2 < d1)
	    return 0;   /* p1-hp2 < p1-hq1  ... IE  hp2 > hq1 .... hp2 is "rightmost" of hq1... IE both hp1 and hp2 are right of the rightmost point of q1-q2, hq1 */
	  if (dc2 < d2)
            {           /* p1-hp2 < p1-hq2  ... IE  hp2 > hq2 .... hp2 is "rightmost" of hq2... */
	      Vcpy2 (i1, hp2); /* One intersection is the leftmost  point of p1-p2 */
	      Vcpy2 (i2, hq1); /* One intersection is the rightmost point of q1-q2 */
              if (i1s != NULL) *i1s = hs2;
              if (i1t != NULL) *i1t = calculate_line_point_intersection (q1, q2, hp2); /* COMPUTE COEFFICIENT OF WHERE hp2 lies ALONG q1-q2 */
              if (i2s != NULL) *i2s = calculate_line_point_intersection (p1, p2, hq1); /* COMPUTE COEFFICIENT OF WHERE hq1 lies ALONG p1-p2 */
              if (i2t != NULL) *i2t = ht1;
	    }
	  else
	    {           /* p1-hp2 >= p1-hq2  ... IE  hp2 <= hq2 ... hp2 is "left of (or coincident to)" hq2... */
	      Vcpy2 (i1, hq1); /* One intersection is the rightmost point of q1-q2 */
	      Vcpy2 (i2, hq2); /* One intersection is the leftmost  point of q1-q2 */
              if (i1s != NULL) *i1s = calculate_line_point_intersection (p1, p2, hq1); /* NEED TO COMPUTE COEFFICIENT OF WHERE hq1 lies ALONG p1-p2 */
              if (i1t != NULL) *i1t = ht1;
              if (i2s != NULL) *i2s = calculate_line_point_intersection (p1, p2, hq2); /* NEED TO COMPUTE COEFFICIENT OF WHERE hq2 lies ALONG p1-p2 */
              if (i2t != NULL) *i2t = ht2;
	    };
	}
      else
	{               /* p1-hp1 >= p1-hq1  ... IE  hp1 <= hq1 ... hp1 is "left of (or coincident to)" hq1 */
	  if (dc1 > d2)
	    return 0;   /* p1-hp1 > p1-hq2   ... IE  hp1 < hq2 .... hp1 is "left" of hq2... IE rightmost point of line p1-p2 is left of both points of q1-q2 */
	  if (dc2 < d2)
	    {           /* p1-hp2 < p1-hq2   ... IE  hp2 > hq2 .... hp2 is "right" of hq2 */
	      Vcpy2 (i1, hp1);  /* One intersection is the rightmost point of p1-p2 */
	      Vcpy2 (i2, hp2);  /* One intersection is the leftmost  point of p1-p2 */
              if (i1s != NULL) *i1s = hs1;
              if (i1t != NULL) *i1t = calculate_line_point_intersection (q1, q2, hp1); /* NEED TO COMPUTE COEFFICIENT OF WHERE hp1 lies ALONG q1-q2 */
              if (i2s != NULL) *i2s = hs2;
              if (i2t != NULL) *i2t = calculate_line_point_intersection (q1, q2, hp2); /* NEED TO COMPUTE COEFFICIENT OF WHERE hp2 lies ALONG q1-q2 */
	    }
	  else
	    {           /* p1-hp2 >= p1-hq2   ... IE  hp2 <= hq2 .... hp2 is "left of (or coincident to) " of hq2 */
	      Vcpy2 (i1, hp1);  /* One intersection is the rightmost point of p1-p2 */
	      Vcpy2 (i2, hq2);  /* One intersection is the leftmost  point of q1-q2 */
              if (i1s != NULL) *i1s = hs1;
              if (i1t != NULL) *i1t = calculate_line_point_intersection (q1, q2, hp1); /* NEED TO COMPUTE COEFFICIENT OF WHERE hp1 lies ALONG q1-q2 */
              if (i2s != NULL) *i2s = calculate_line_point_intersection (p1, p2, hq2); /* NEED TO COMPUTE COEFFICIENT OF WHERE hq2 lies ALONG p1-p2 */
              if (i2t != NULL) *i2t = ht2;
	    };
	}

#if 0
      /* XXX: NEED TO RETURN THE POINTS ON EACH LINE */
#warning MISSING RETURN CODE FOR PARALLEL LINE CASES

      printf ("!!!!!!!!!!!!!!!!!!!!!!!!!!!! XXXXXXXXXXXXXXXXXXXXXXXX "
              "!!!!!!!!!!!!!!!!!!!!!!!!!!!! <<<<<<<<<<<<<<<<<<<<<<<<\n");
#endif

      return (Vequ2 (i1, i2) ? 1 : 2);
      }
    }

  else
    {				/* not parallel */
      /*
       * We have the lines:
       * l1: p1 + s(p2 - p1)
       * l2: q1 + t(q2 - q1)
       * And we want to know the intersection point.
       * Calculate t:
       * p1 + s(p2-p1) = q1 + t(q2-q1)
       * which is similar to the two equations:
       * p1x + s * rpx = q1x + t * rqx
       * p1y + s * rpy = q1y + t * rqy
       * Multiplying these by rpy resp. rpx gives:
       * rpy * p1x + s * rpx * rpy = rpy * q1x + t * rpy * rqx
       * rpx * p1y + s * rpx * rpy = rpx * q1y + t * rpx * rqy
       * Subtracting these gives:
       * rpy * p1x - rpx * p1y = rpy * q1x - rpx * q1y + t * ( rpy * rqx - rpx * rqy )
       * So t can be isolated:
       * t = (rpy * ( p1x - q1x ) + rpx * ( - p1y + q1y )) / ( rpy * rqx - rpx * rqy )
       * and s can be found similarly
       * s = (rqy * (q1x - p1x) + rqx * (p1y - q1y))/( rqy * rpx - rqx * rpy)
       */

      if (Vequ2 (q1, p1))
	{
	  i1[0] = q1[0];
	  i1[1] = q1[1];
	  s = 0.0;
	  t = 0.0;
	}
      else if  (Vequ2 (q1, p2))
        {
	  i1[0] = q1[0];
	  i1[1] = q1[1];
	  s = 1.0;
	  t = 0.0;
        }
      else if (Vequ2 (q2, p1))
	{
	  i1[0] = q2[0];
	  i1[1] = q2[1];
	  s = 0.0;
	  t = 1.0;
	}
      else if (Vequ2 (q2, p2))
	{
	  i1[0] = q2[0];
	  i1[1] = q2[1];
	  s = 1.0;
	  t = 1.0;
	}
      else
	{
	  s = (rqy * (p1[0] - q1[0]) + rqx * (q1[1] - p1[1])) / deel;
//	  if (s < -EPSILON || s > 1. + EPSILON)
	  if (s < 0.       || s > 1.          )
	    return 0;
	  t = (rpy * (p1[0] - q1[0]) + rpx * (q1[1] - p1[1])) / deel;
//	  if (t < -EPSILON || t > 1. + EPSILON)
	  if (t < 0.       || t > 1.          )
	    return 0;

	  i1[0] = q1[0] + ROUND (t * rqx);
	  i1[1] = q1[1] + ROUND (t * rqy);
	}

      /* Output the coefficients of the intersection along each line */
      if (i1s != NULL) *i1s = s;
      if (i1t != NULL) *i1t = t;

      return 1;
    }
}				/* vect_inters2 */

/* how about expanding polygons so that edges can be arcs rather than
 * lines. Consider using the third coordinate to store the radius of the
 * arc. The arc would pass through the vertex points. Positive radius
 * would indicate the arc bows left (center on right of P1-P2 path)
 * negative radius would put the center on the other side. 0 radius
 * would mean the edge is a line instead of arc
 * The intersections of the two circles centered at the vertex points
 * would determine the two possible arc centers. If P2.x > P1.x then
 * the center with smaller Y is selected for positive r. If P2.y > P1.y
 * then the center with greate X is selected for positive r.
 *
 * the vec_inters2() routine would then need to handle line-line
 * line-arc and arc-arc intersections.
 *
 * perhaps reverse tracing the arc would require look-ahead to check
 * for arcs
 */


BoxType *
get_seg_bounds (PLINE *contour, VNODE *node)
{
  struct info info;
  BoxType box;
  double dx;

  /* XXX: Use original slope rather than that of the intersected segments? */
  info.v = node;
  /* compute the slant for region trimming */
  dx = node->next->point[0] - node->point[0];
  if (dx == 0)
    info.m = 0;
  else
    {
      info.m = (node->next->point[1] - node->point[1]) / dx;
      info.b = node->point[1] - info.m * node->point[0];
    }

  box.X2 = (box.X1 = node->point[0]) + 1;
  box.Y2 = (box.Y1 = node->point[1]) + 1;

  /* fill in the segment in info corresponding to this node */
  if (setjmp (info.sego) == 0)
    {
      r_search (contour->tree, &box, NULL, get_seg, &info);
      assert (0);
    }

  return &info.s->box;
}
