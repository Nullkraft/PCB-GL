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
//#define NDEBUG
#include	<assert.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<setjmp.h>
#include	<math.h>
#include	<string.h>

#include "global.h"
#include "polyarea.h"
#include "rtree.h"
#include "heap.h"

#define ROUND(a) (long)((a) > 0 ? ((a) + 0.5) : ((a) - 0.5))

#define EPSILON (1E-8)
#define IsZero(a, b) (fabs((a) - (b)) < EPSILON)
#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

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

int vect_inters2 (Vector A, Vector B, Vector C, Vector D, Vector S1,
		  Vector S2);

/* note that a vertex v's Flags.status represents the edge defined by
 * v to v->next (i.e. the edge is forward of v)
 */
#define ISECTED 3
#define UNKNWN  0
#define INSIDE  1
#define OUTSIDE 2
#define SHARED 3
#define SHARED2 4

#define TOUCHES 99

#define NODE_LABEL(n)  ((n)->Flags.status)
#define LABEL_NODE(n,l) ((n)->Flags.status = (l))

#define error(code)  longjmp(*(e), code)

#warning TODO: Unlikely
#define MemGet(ptr, type) \
if (((ptr) = malloc(sizeof(type))) == NULL) \
    error(err_no_memory);

#undef DEBUG_LABEL
#undef DEBUG_ALL_LABELS
#undef DEBUG_JUMP
#define DEBUG_GATHER
#undef DEBUG_ANGLE
#undef DEBUG
#ifdef DEBUG
#define DEBUGP(...) fprintf(stderr, ## __VA_ARGS__)
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

#ifdef DEBUG
static char *theState (VNODE * v);

static void
pline_dump (VNODE * v)
{
  VNODE *s;

  s = v;
  do
    {
      if (v != s)
	fprintf (stderr, "%d %d 10 10 \"%s\"]\n", v->point[0], v->point[1],
		 theState (v));
      fprintf (stderr, "Line [%d %d ", v->point[0], v->point[1]);
    }
  while ((v = v->next) != s);
  fprintf (stderr, "%d %d 10 10 \"%s\"]\n", v->point[0], v->point[1],
	   theState (v));
  fprintf (stderr, "NEXT PLINE\n");
}

static void
poly_dump (POLYAREA * p)
{
  POLYAREA *f = p;

  do
    {
      pline_dump (&p->contours->head);
    }
  while ((p = p->f) != f);
  fprintf (stderr, "NEXT_POLY\n");
}
#endif

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
static VNODE *
node_add (VNODE * dest, Vector po, int *new_point)
{
  VNODE *p;

  if (vect_equal (po, dest->point))
    return dest;
  if (vect_equal (po, dest->next->point))
    {
      (*new_point) += 4;
      return dest->next;
    }
  p = poly_CreateNode (po);
  if (p == NULL)
    return NULL;
  (*new_point) += 5;
  p->prev = dest;
  p->next = dest->next;
  p->cvc_prev = p->cvc_next = NULL;
  p->Flags.status = UNKNWN;
  return (dest->next = dest->next->prev = p);
}				/* node_add */

#define ISECT_BAD_PARAM (-1)
#define ISECT_NO_MEMORY (-2)


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
  if (side == 'P')		/* previous */
    vect_sub (v, a->prev->point, a->point);
  else				/* next */
    vect_sub (v, a->next->point, a->point);
  /* Uses slope/(slope+1) in quadrant 1 as a proxy for the angle.
   * It still has the same monotonic sort result
   * and is far less expensive to compute than the real angle.
   */
  if (vect_equal (v, vect_zero))
    {
      if (side == 'P')
	{
	  if (a->prev->cvc_prev == (CVCList *) - 1)
	    a->prev->cvc_prev = a->prev->cvc_next = NULL;
	  poly_ExclVertex (a->prev);
	  vect_sub (v, a->prev->point, a->point);
	}
      else
	{
	  if (a->next->cvc_prev == (CVCList *) - 1)
	    a->next->cvc_prev = a->next->cvc_next = NULL;
	  poly_ExclVertex (a->next);
	  vect_sub (v, a->next->point, a->point);
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
  DEBUGP ("node on %c at (%ld,%ld) assigned angle %g on side %c\n", poly,
	  a->point[0], a->point[1], ang, side);
#endif
  return l;
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
  CVCList *l, *new, *big, *small;

  if (!(new = new_descriptor (a, poly, side)))
    return NULL;
  /* search for the CVCList for this point */
  if (!start)
    {
      start = new;		/* return is also new, so we know where start is */
      start->head = new;	/* circular list */
      return new;
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
	      new->head = l->head;
	      break;
	    }
	  if (l->head->parent->point[0] == start->parent->point[0]
	      && l->head->parent->point[1] == start->parent->point[1])
	    {
	      /* this seems to be a new point */
	      /* link this cvclist to the list of all cvclists */
	      for (; l->head != new; l = l->next)
		l->head = new;
	      new->head = start;
	      return new;
	    }
	  l = l->head;
	}
      while (1);
    }
  assert (start);
  l = big = small = start;
  do
    {
      if (l->next->angle < l->angle)	/* find start/end of list */
	{
	  small = l->next;
	  big = l;
	}
      else if (new->angle >= l->angle && new->angle <= l->next->angle)
	{
	  /* insert new cvc if it lies between existing points */
	  new->prev = l;
	  new->next = l->next;
	  l->next = l->next->prev = new;
	  return new;
	}
    }
  while ((l = l->next) != start);
  /* didn't find it between points, it must go on an end */
  if (big->angle <= new->angle)
    {
      new->prev = big;
      new->next = big->next;
      big->next = big->next->prev = new;
      return new;
    }
  assert (small->angle >= new->angle);
  new->next = small;
  new->prev = small->prev;
  small->prev = small->prev->next = new;
  return new;
}

/*
node_add_point
 (C) 1993 Klamer Schutte
 (C) 1997 Alexey Nikitin, Michael Leonov

 return 1 if new node in b, 2 if new node in a and 3 if new node in both
*/

static int
node_add_point (VNODE * a, VNODE * b, Vector p)
{
  int res = 0;

  VNODE *node_a, *node_b;

  node_a = node_add (a, p, &res);
  res += res;
  node_b = node_add (b, p, &res);

  if (node_a == NULL || node_b == NULL)
    return ISECT_NO_MEMORY;
  node_b->cvc_prev = node_b->cvc_next = (CVCList *) - 1;
  node_a->cvc_prev = node_a->cvc_next = (CVCList *) - 1;
  return res;
}				/* node_add_point */

/*
node_label
 (C) 2006 harry eaton
*/
static unsigned int
node_label (VNODE * pn)
{
  CVCList *l;
  char this_poly;
  int region = UNKNWN;

  assert (pn);
  assert (pn->cvc_prev);
  this_poly = pn->cvc_prev->poly;
  /* search clockwise in the cross vertex connectivity (CVC) list
   *
   * check for shared edges, and check if our edges
   * are ever found between the other poly's entry and exit
   */
#ifdef DEBUG_LABEL
  DEBUGP ("CVCLIST for point (%ld,%ld)\n", pn->point[0], pn->point[1]);
#endif
  /* first find whether we're starting inside or outside */
  for (l = pn->cvc_prev->prev; l != pn->cvc_prev; l = l->prev)
    {
      if (l->poly != this_poly)
	{
	  if (l->side == 'P')
	    region = INSIDE;
	  else
	    region = OUTSIDE;
	}
    }
  l = pn->cvc_prev;
  do
    {
      assert (l->angle >= 0 && l->angle <= 4.0);
#ifdef DEBUG_LABEL
      DEBUGP ("  poly %c side %c angle = %g\n", l->poly, l->side, l->angle);
#endif
      if (l->poly != this_poly)
	{
	  if (l->side == 'P')
	    {
	      region = INSIDE;
	      if (l->parent->prev->point[0] == pn->prev->point[0] &&
		  l->parent->prev->point[1] == pn->prev->point[1])
		{
		  LABEL_NODE (pn->prev, SHARED);	/* incoming is shared */
		  pn->prev->shared = l->parent->prev;
		}
	      else if (l->parent->prev->point[0] == pn->next->point[0] &&
		       l->parent->prev->point[1] == pn->next->point[1])
		{
		  LABEL_NODE (pn, SHARED2);	/* outgoing is shared2 */
		  pn->shared = l->parent->prev;
		}
	    }
	  else
	    {
	      region = OUTSIDE;
	      if (l->parent->next->point[0] == pn->next->point[0] &&
		  l->parent->next->point[1] == pn->next->point[1])
		{
		  LABEL_NODE (pn, SHARED);
		  pn->shared = l->parent;
		}
	      else if (l->parent->next->point[0] == pn->prev->point[0] &&
		       l->parent->next->point[1] == pn->prev->point[1])
		{
		  LABEL_NODE (pn->prev, SHARED2);	/* outgoing is shared2 */
		  pn->prev->shared = l->parent;
		}
	    }
	}
      else
	{
	  VNODE *v;
	  if (l->side == 'P')
	    v = l->parent->prev;
	  else
	    v = l->parent;
	  if (NODE_LABEL (v) != SHARED && NODE_LABEL (v) != SHARED2)
	    {
#ifdef DEBUG_LABEL
	      /* debugging */
	      if (NODE_LABEL (v) != UNKNWN && NODE_LABEL (v) != region)
		{
		  CVCList *x = l;
		  LABEL_NODE (v, region);
		  pline_dump (v);
		  do
		    {
		      fprintf (stderr, "poly %c\n", x->poly);
		      pline_dump (x->parent);
		    }
		  while ((x = x->next) != l);
		}
#endif
	      assert (NODE_LABEL (v) == UNKNWN || NODE_LABEL (v) == region);
	      LABEL_NODE (v, region);
	    }
	}
    }
  while ((l = l->prev) != pn->cvc_prev);
#ifdef DEBUG_LABEL
  DEBUGP ("\n");
#endif
  assert (NODE_LABEL (pn) != UNKNWN && NODE_LABEL (pn->prev) != UNKNWN);
  if (NODE_LABEL (pn) == UNKNWN || NODE_LABEL (pn->prev) == UNKNWN)
    return UNKNWN;
  if (NODE_LABEL (pn) == INSIDE || NODE_LABEL (pn) == OUTSIDE)
    return NODE_LABEL (pn);
  return UNKNWN;
}				/* node_label */

/*
 add_descriptors
 (C) 2006 harry eaton
*/
static CVCList *
add_descriptors (PLINE * pl, char poly, CVCList * list)
{
  VNODE *node = &pl->head;

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
  while ((node = node->next) != &pl->head);
  return list;
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
} seg;

typedef struct info
{
  double m, b;
  rtree_t *tree;
  VNODE *v;
  struct seg *s;
  jmp_buf *env, sego, *touch;
} info;

typedef struct contour_info
{
  PLINE *pa;
  jmp_buf restart;
  jmp_buf *getout;
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

  q = malloc (sizeof (struct seg));
  if (!q)
    return 1;
  q->v = s->v;
  q->p = s->p;
  q->box.X1 = min (q->v->point[0], q->v->next->point[0]);
  q->box.X2 = max (q->v->point[0], q->v->next->point[0]) + 1;
  q->box.Y1 = min (q->v->point[1], q->v->next->point[1]);
  q->box.Y2 = max (q->v->point[1], q->v->next->point[1]) + 1;
  r_insert_entry (tree, (const BoxType *) q, 1);
  q = malloc (sizeof (struct seg));
  if (!q)
    return 1;
  q->v = s->v->next;
  q->p = s->p;
  q->box.X1 = min (q->v->point[0], q->v->next->point[0]);
  q->box.X2 = max (q->v->point[0], q->v->next->point[0]) + 1;
  q->box.Y1 = min (q->v->point[1], q->v->next->point[1]);
  q->box.Y2 = max (q->v->point[1], q->v->next->point[1]) + 1;
  r_insert_entry (tree, (const BoxType *) q, 1);
  r_delete_entry (tree, (const BoxType *) s);
  return 0;
}

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
  struct seg *s = (struct seg *) b;
  Vector s1, s2;
  int cnt, res;

  cnt = vect_inters2 (s->v->point, s->v->next->point,
		      i->v->point, i->v->next->point, s1, s2);
  if (!cnt)
    return 0;
  if (i->touch)			/* if checking touches one find and we're done */
    longjmp (*i->touch, TOUCHES);
  i->s->p->Flags.status = ISECTED;
  s->p->Flags.status = ISECTED;
  for (; cnt; cnt--)
    {
      res = node_add_point (i->v, s->v, cnt > 1 ? s2 : s1);
      if (res < 0)
	return 1;		/* error */
      /* adjust the bounding box and tree if necessary */
      if (res & 2)
	{
	  cntrbox_adjust (i->s->p, cnt > 1 ? s2 : s1);
	  if (adjust_tree (i->s->p->tree, i->s))
	    return 1;
	}
      /* if we added a node in the tree we need to change the tree */
      if (res & 1)
	{
	  cntrbox_adjust (s->p, cnt > 1 ? s2 : s1);
	  if (adjust_tree (i->tree, s))
	    return 1;
	}
      if (res & 3)		/* if a point was inserted start over */
	{
#ifdef DEBUG_INTERSECT
	  DEBUGP ("new intersection at (%d, %d)\n", cnt > 1 ? s2[0] : s1[0],
		  cnt > 1 ? s2[1] : s1[1]);
#endif
	  longjmp (*i->env, 1);
	}
    }
  return 0;
}

static void *
make_edge_tree (PLINE * pb)
{
  struct seg *s;
  VNODE *bv;
  rtree_t *ans = r_create_tree (NULL, 0, 0);
  bv = &pb->head;
  do
    {
      s = malloc (sizeof (struct seg));
      if (bv->point[0] < bv->next->point[0])
	{
	  s->box.X1 = bv->point[0];
	  s->box.X2 = bv->next->point[0] + 1;
	}
      else
	{
	  s->box.X2 = bv->point[0] + 1;
	  s->box.X1 = bv->next->point[0];
	}
      if (bv->point[1] < bv->next->point[1])
	{
	  s->box.Y1 = bv->point[1];
	  s->box.Y2 = bv->next->point[1] + 1;
	}
      else
	{
	  s->box.Y2 = bv->point[1] + 1;
	  s->box.Y1 = bv->next->point[1];
	}
      s->v = bv;
      s->p = pb;
      r_insert_entry (ans, (const BoxType *) s, 1);
    }
  while ((bv = bv->next) != &pb->head);
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
  contour_info *c_info = (contour_info *)cl;
  PLINE *pa = c_info->pa;
  PLINE *pb = (PLINE *)b;
  PLINE *rtree_over;
  PLINE *looping_over;
  VNODE *av;			/* node iterators */
  struct info info;
  BoxType box;

  /* Have seg_in_seg return to our desired location if it touches */
  info.env = &c_info->restart;
  info.touch = c_info->getout;

  /* Pick which contour has the fewer points, and do the loop
   * over that. The r_tree makes hit-testing against a contour
   * faster, so we want to do that on the bigger contour.
   */
  if (pa->Count < pb->Count)
    {
      rtree_over   = pb;
      looping_over = pa;
    }
  else
    {
      rtree_over   = pa;
      looping_over = pb;
    }

  av = &looping_over->head;
  do  /* Loop over the nodes in the smaller contour */
    {
      /* check this edge for any insertions */
      double dx;
      info.v = av;
      /* compute the slant for region trimming */
      dx = av->next->point[0] - av->point[0];
      if (dx == 0)
        info.m = 0;
      else
        {
          info.m = (av->next->point[1] - av->point[1]) / dx;
          info.b = av->point[1] - info.m * av->point[0];
        }
      box.X2 = (box.X1 = av->point[0]) + 1;
      box.Y2 = (box.Y1 = av->point[1]) + 1;

      /* fill in the segment in info corresponding to this node */
      if (setjmp (info.sego) == 0)
        {
          r_search (looping_over->tree, &box, NULL, get_seg, &info);
          assert (0);
        }

        /* NB: If this actually hits anything, we are teleported back to the beginning */
        info.tree = rtree_over->tree;
        if (info.tree)
          if (UNLIKELY (r_search (info.tree, &info.s->box,
                                  seg_in_region, seg_in_seg, &info)))
            return err_no_memory;	/* error */
    }
  while ((av = av->next) != &looping_over->head);
  return 0;
}

static int
intersect (jmp_buf * jb, POLYAREA * b, POLYAREA * a, int add)
{
  POLYAREA *t;
  PLINE *pa;
  contour_info c_info;

  /* Search the r-tree of the object with most contours
   * We loop over the contours of "a". Swap if necessary.
   */
  if (a->contour_tree->size > b->contour_tree->size)
    {
      t = b;
      b = a;
      a = t;
    }

  setjmp (c_info.restart);		/* we loop back here whenever a vertex is inserted */

  for (pa = a->contours; pa; pa = pa->next)     /* Loop over the contours of POLYAREA "a" */
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
    }

  return 0;
}

static void
M_POLYAREA_intersect2 (jmp_buf * e, POLYAREA * afst, POLYAREA * bfst, int add)
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
	      if (intersect (e, a, b, add))
		error (err_no_memory);
	    }
	}
      while ((a = a->f) != afst);
      for (curcB = b->contours; curcB != NULL; curcB = curcB->next)
	if (curcB->Flags.status == ISECTED)
	  if (!(the_list = add_descriptors (curcB, 'B', the_list)))
	    error (err_no_memory);
    }
  while ((b = b->f) != bfst);
  do
    {
      for (curcA = a->contours; curcA != NULL; curcA = curcA->next)
	if (curcA->Flags.status == ISECTED)
	  if (!(the_list = add_descriptors (curcA, 'A', the_list)))
	    error (err_no_memory);
    }
  while ((a = a->f) != afst);
}				/* M_POLYAREA_intersect */

static void
M_POLYAREA_intersect (jmp_buf * e, POLYAREA * afst, POLYAREA * bfst, int add)
{
  POLYAREA *a = afst, *b = bfst;
  PLINE *curcA, *curcB;
  CVCList *the_list = NULL;

  if (a == NULL || b == NULL) {
    printf ("a or b is null in M_POLYAREA_intersect\n");
    error (err_bad_parm);
  }
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

/* cntr_in_M_PLINE
returns poly is inside outfst ? TRUE : FALSE */
static int
cntr_in_M_PLINE (PLINE * poly, PLINE * outfst, BOOLp test)
{
  PLINE *curc;
  PLINE *outer = outfst;
  heap_t *heap;

  assert (poly != NULL);
  assert (outer != NULL);

  heap = heap_create ();
//  do {
    if (cntrbox_inside (poly, outer))
      heap_insert (heap, outer->area, (void *) outer);
//  /* if checking touching, use only the first polygon */
//  } while (!test && (outer = outer->f) != outfst);

  /* we need only check the smallest poly container
   * but we must loop in case the box containter is not
   * the poly container */
  while (1) {
    if (heap_is_empty (heap))
      break;
    outer = (PLINE *) heap_remove_smallest (heap);
    if (poly_ContourInContour (outer, poly)) {
      for (curc = outer->next; curc != NULL; curc = curc->next)
        if (poly_ContourInContour (curc, poly)) {
          /* it's inside a hole in the smallest polygon 
           * no need to check the other polygons */
          heap_destroy (&heap);
          return FALSE;
        }
      heap_destroy (&heap);
      return TRUE;
    }
  }
  heap_destroy (&heap);
  return FALSE;
}				/* cntr_in_M_PLINE */

static int
count_contours_i_am_inside (const BoxType *b, void *cl)
{
  PLINE *me = cl;
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

      switch (r_search (outer->contour_tree, (BoxType *)poly, NULL, count_contours_i_am_inside, poly)) {
        case 0: /* Didn't find anything in this piece, Keep looking */
          break;
        case 1: /* Found we are inside this piece, and not any of its holes */
          heap_destroy (&heap);
          return TRUE;
        case 2: /* Found inside a hole in the smallest polygon so far. No need to check the other polygons */
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

#ifdef DEBUG

static char *
theState (VNODE * v)
{
  static char u[] = "UNKNOWN";
  static char i[] = "INSIDE";
  static char o[] = "OUTSIDE";
  static char s[] = "SHARED";
  static char s2[] = "SHARED2";

  switch (NODE_LABEL (v))
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

static void
print_labels (PLINE * a)
{
  VNODE *c = &a->head;

  do
    {
      DEBUGP ("(%ld,%ld)->(%ld,%ld) labeled %s\n", c->point[0], c->point[1],
	      c->next->point[0], c->next->point[1], theState (c));
    }
  while ((c = c->next) != &a->head);
}
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
  VNODE *cur = &a->head;
  int did_label = FALSE, label = UNKNWN;

  do
    {
      if (cur == &a->head)
	did_label = FALSE;
      if (NODE_LABEL (cur) != UNKNWN)
	{
	  label = NODE_LABEL (cur);
	  continue;
	}
      if (cur->cvc_next)	/* examine cross vertex */
	{
	  label = node_label (cur);
	  did_label = TRUE;
	}
      else if (label == INSIDE || label == OUTSIDE)
	{
	  LABEL_NODE (cur, label);
	  did_label = TRUE;
	}
    }
  while ((cur = cur->next) != &a->head || did_label);
#ifdef DEBUG_ALL_LABELS
  print_labels (a);
  DEBUGP ("\n\n");
#endif
  return FALSE;
}				/* label_contour */

static BOOLp
cntr_label_PLINE (PLINE * poly, PLINE * pl, BOOLp test)
{
  assert (ppl != NULL);
  if (poly->Flags.status == ISECTED) {
    printf ("cntr_label_PLINE: Labelling intersected contour\n");
    label_contour (poly);	/* should never get here when BOOLp is true */

  } else {
 
    fprintf (stderr, "*********** SOMETHING ODD GOING ON HERE?\n");
//    return FALSE;

    if (cntr_in_M_PLINE (poly, pl, test)) {

      if (test)
        return TRUE;
      poly->Flags.status = INSIDE;

    } else {

      if (test)
        return False;
      if (poly->Flags.status == UNKNWN) {
        printf ("cntr_label_PLINE: Changing UNKNWN to OUTSIDE\n");
        poly->Flags.status = OUTSIDE;
      }

    }
  }
  return FALSE;
}				/* cntr_label_PLINE */

static BOOLp
cntr_label_POLYAREA_non_isected (PLINE * poly, POLYAREA * ppl, BOOLp test)
{
  assert (ppl != NULL && ppl->contours != NULL);
  if (poly->Flags.status == ISECTED)
    {
      printf ("cntr_label_POLYAREA_non_isected: Skipping labelling intersected contour\n");
      //label_contour (poly);	/* should never get here when BOOLp is true */
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
	return False;
      poly->Flags.status = OUTSIDE;
    }
  return FALSE;
}				/* cntr_label_POLYAREA */

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
	return False;
      poly->Flags.status = OUTSIDE;
    }
  return FALSE;
}				/* cntr_label_POLYAREA */

static BOOLp
M_POLYAREA_label_separated (PLINE * afst, POLYAREA * b, BOOLp touch)
{
  PLINE *curc = afst;

  for (curc = afst; curc != NULL; curc = curc->next) {
//    printf ("Labelling separated contour\n");
    if (cntr_label_POLYAREA (curc, b, touch) && touch)
      return TRUE;
  }
  return FALSE;
}

static BOOLp
M_POLYAREA_label_isected (POLYAREA * afst, PLINE * b, BOOLp touch)
{
  POLYAREA *a = afst;
  PLINE *curc;

  if (b == NULL) {
    printf ("M_POLYAREA_label_isected: No PLINE to test against\n");
    return FALSE;
  }

  assert (a != NULL);
  do {
    for (curc = a->contours; curc != NULL; curc = curc->next)
      if (cntr_label_PLINE (curc, b, touch) && touch)
        return TRUE;
  } while (!touch && (a = a->f) != afst);
  return FALSE;
}


static BOOLp
M_POLYAREA_label_non_isected (POLYAREA * afst, POLYAREA * b, BOOLp touch)
{
  POLYAREA *a = afst;
  PLINE *curc;

  assert (a != NULL);
  do
    {
      for (curc = a->contours; curc != NULL; curc = curc->next)
	if (cntr_label_POLYAREA_non_isected (curc, b, touch))
	  {
	    if (touch)
	      return TRUE;
	  }
    }
  while (!touch && (a = a->f) != afst);
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
  while ((a = a->f) != afst);
  return FALSE;
}

/****************************************************************/

#warning are contour r-trees needed for the temporary polyareas?
/* routines for temporary storing resulting contours */
static void
InsCntr (jmp_buf * e, PLINE * c, POLYAREA ** dst)
{
  POLYAREA *newp;

  if (*dst == NULL)
    {
      MemGet (*dst, POLYAREA);
      (*dst)->f = (*dst)->b = *dst;
      newp = *dst;
    }
  else
    {
      MemGet (newp, POLYAREA);
      newp->f = *dst;
      newp->b = (*dst)->b;
      newp->f->b = newp->b->f = newp;
    }
  newp->contours = c;
  newp->contour_tree = r_create_tree (NULL, 0, 0);
  r_insert_entry (newp->contour_tree, (BoxTypePtr) c, 0);
  c->next = NULL;
}				/* InsCntr */

static void
PutContour (jmp_buf * e, PLINE * cntr, POLYAREA ** contours, PLINE ** holes,
            POLYAREA *owner, POLYAREA * parent, PLINE * parent_contour)
{
  assert (cntr != NULL);
  assert (cntr->Count > 2);
  cntr->next = NULL;

//  printf ("PutContour %p, %p, %p, %p, %p, %p\n",
//          cntr, contours, holes, owner, parent, parent_contour);

  if (cntr->Flags.orient == PLF_DIR)
    {
      if (owner != NULL) {
//        printf ("PATH 1\n");
        r_delete_entry (owner->contour_tree, (BoxType *)cntr);
      }
//      printf ("Put contour adding a new contour\n");
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
//          printf ("Put Contour adding a hole directly to its parent\n");
          if (owner != parent)
            {
              if (owner != NULL) {
//                printf ("PATH 2\n");
                r_delete_entry (owner->contour_tree, (BoxType *)cntr);
              }
              r_insert_entry (parent->contour_tree, (BoxType *)cntr, 0);
            }
	}
      else
	{
	  cntr->next = *holes;
	  *holes = cntr;	/* let cntr be 1st hole in list */
//          printf ("Put Contour adding a hole\n");
          /* We don't insert the holes into an r-tree,
           * they just form a linked list */
          if (owner != NULL) {
//            printf ("PATH 3\n");
            r_delete_entry (owner->contour_tree, (BoxType *)cntr);
          }
	}
    }
}				/* PutContour */

static inline void
remove_contour (POLYAREA *piece, PLINE *prev_contour, PLINE *contour,
                int remove_rtree_entry)
{
  if (piece->contours == contour)
    piece->contours = contour->next;

  if (prev_contour != NULL)
    prev_contour->next = contour->next;

  contour->next = NULL;

  if (remove_rtree_entry)
    r_delete_entry (piece->contour_tree, (BoxType *)contour);
}

  static int
heap_it (const BoxType * b, void *cl)
{
  heap_t *heap = (heap_t *) cl;
  PLINE *p = (PLINE *) b;
  if (p->Count == 0)
    return 0;  /* how did this happen? */
  heap_insert (heap, p->area, (void *) p);
  return 1;
}

struct find_inside_info {
  jmp_buf jb;
  PLINE *want_inside;
  PLINE *result;
};

static int
find_inside (const BoxType *b, void *cl)
{
  struct find_inside_info *info = cl;
  PLINE *check = (PLINE *) b;
  /* Do test on check to see if it inside info->want_inside */
  /* If it is: */
//  printf ("find_inside: Checking a possible hole\n");
  if (check->Flags.orient == PLF_DIR) {
//    printf ("Think we just got the main contour?\n");
    return 0;
  }
  if (poly_ContourInContour (info->want_inside, check)) {
//    printf ("find_inside: Found a hole inside the one we're checking\n");
    info->result = check;
    longjmp (info->jb, 1);
  }
  return 0;
}

static void
InsertHoles (jmp_buf * e, POLYAREA * dest, PLINE ** src)
{
  POLYAREA *curc;
  PLINE *curh, *container, *tmp;
  heap_t *heap;
  rtree_t *tree;

  if (*src == NULL)
    return;			/* empty hole list */
  if (dest == NULL) {
    printf ("dest is null in InsertHoles\n");
    error (err_bad_parm);	/* empty contour list */
  }

#warning IF Passed a PourType, we would get this r-tree for free??
  /* make an rtree of contours */
  tree = r_create_tree (NULL, 0, 0);
  curc = dest;
  do
    {
      r_insert_entry (tree, (const BoxType *) curc->contours, 0);
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
    printf ("Badparm hi there\n");
	  error (err_bad_parm);
	}
      /* Now search the heap for the container. If there was only one item
       * in the heap, assume it is the container without the expense of
       * proving it.
       */
      tmp = (PLINE *) heap_remove_smallest (heap);
      if (heap_is_empty (heap))
	{			/* only one possibility it must be the right one */
	  assert (poly_ContourInContour (tmp, curh));
	  container = tmp;
	}
      else
	{
	  do
	    {
	      if (poly_ContourInContour (tmp, curh))
		{
		  container = tmp;
		  break;
		}
	      if (heap_is_empty (heap))
		break;
	      tmp = (PLINE *) heap_remove_smallest (heap);
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
    printf ("Howdy\n");
	  error (err_bad_parm);
	}
      else
	{
#warning WHICH POLYAREA GOT THIS - STUPID LONG SEARCH - STORE IN HEAP STRUCTURE INSTEAD!!
          curc = dest;
          do {
            if (curc->contours == container)
              break;
          } while ((curc = curc->f) != dest);
          if (curc->contours != container) {
            curc = NULL;
            printf ("Badness\n");
          }

          /* Need to check if this new hole means we need to kick out any old ones for reprocessing */
          while (1) {
            struct find_inside_info info;
            PLINE *prev;

            info.want_inside = curh;

            /* Set jump return */
            if (setjmp (info.jb)) {
              /* Returned here! */
            } else {
              info.result = NULL;
              /* Rtree search, calling back a routine to longjmp back with data about any hole inside the added one */
              /*   Be sure not to bother jumping back to report the main contour! */
              r_search (curc->contour_tree, (BoxType *)curh, NULL, find_inside, &info);

              /* Nothing found? */
              break;
            }

            /* We need to find the contour before it, so we can update its next pointer */
            prev = container;
            while (prev->next != info.result) {
              prev = prev->next;
            }

            /* Remove hole from the contour */
            remove_contour (curc, prev, info.result, TRUE);

            /* Add hole as the next on the list to be processed in this very function */
            info.result->next = *src;
            *src = info.result;
          }
          /* End check for kicked out holes */

	  /* link at front of hole list */
	  tmp = container->next;
	  container->next = curh;
	  curh->next = tmp;
          if (curc != NULL)
            r_insert_entry (curc->contour_tree, (BoxTypePtr) curh, 0);

	}
    }
  r_destroy_tree (&tree);
}				/* InsertHoles */


/****************************************************************/
/* routines for collecting result */

typedef enum
{
  FORW, BACKW
} DIRECTION;

/* Start Rule */
typedef int (*S_Rule) (VNODE *, DIRECTION *);

/* Jump Rule  */
typedef int (*J_Rule) (char, VNODE *, DIRECTION *);

static int
UniteS_Rule (VNODE * cur, DIRECTION * initdir)
{
  *initdir = FORW;
  return (NODE_LABEL (cur) == OUTSIDE) || (NODE_LABEL (cur) == SHARED);
}

static int
IsectS_Rule (VNODE * cur, DIRECTION * initdir)
{
  *initdir = FORW;
  return (NODE_LABEL (cur) == INSIDE) || (NODE_LABEL (cur) == SHARED);
}

static int
SubS_Rule (VNODE * cur, DIRECTION * initdir)
{
  *initdir = FORW;
  return (NODE_LABEL (cur) == OUTSIDE) || (NODE_LABEL (cur) == SHARED2);
}

static int
XorS_Rule (VNODE * cur, DIRECTION * initdir)
{
  if (cur->Flags.status == INSIDE)
    {
      *initdir = BACKW;
      return TRUE;
    }
  if (cur->Flags.status == OUTSIDE)
    {
      *initdir = FORW;
      return TRUE;
    }
  return FALSE;
}

static int
IsectJ_Rule (char p, VNODE * v, DIRECTION * cdir)
{
  assert (*cdir == FORW);
  return (v->Flags.status == INSIDE || v->Flags.status == SHARED);
}

static int
UniteJ_Rule (char p, VNODE * v, DIRECTION * cdir)
{
  assert (*cdir == FORW);
  return (v->Flags.status == OUTSIDE || v->Flags.status == SHARED);
}

static int
XorJ_Rule (char p, VNODE * v, DIRECTION * cdir)
{
  if (v->Flags.status == INSIDE)
    {
      *cdir = BACKW;
      return TRUE;
    }
  if (v->Flags.status == OUTSIDE)
    {
      *cdir = FORW;
      return TRUE;
    }
  return FALSE;
}

static int
SubJ_Rule (char p, VNODE * v, DIRECTION * cdir)
{
  if (p == 'B' && v->Flags.status == INSIDE)
    {
      *cdir = BACKW;
      return TRUE;
    }
  if (p == 'A' && v->Flags.status == OUTSIDE)
    {
      *cdir = FORW;
      return TRUE;
    }
  if (v->Flags.status == SHARED2)
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
static int
jump (VNODE ** cur, DIRECTION * cdir, J_Rule rule)
{
  CVCList *d, *start;
  VNODE *e;
  DIRECTION new;

  if (!(*cur)->cvc_prev)	/* not a cross-vertex */
    {
      if (*cdir == FORW ? (*cur)->Flags.mark : (*cur)->prev->Flags.mark)
	return FALSE;
      return TRUE;
    }
#ifdef DEBUG_JUMP
  DEBUGP ("jump entering node at (%ld, %ld)\n", (*cur)->point[0],
	  (*cur)->point[1]);
#endif
  if (*cdir == FORW)
    d = (*cur)->cvc_prev->prev;
  else
    d = (*cur)->cvc_next->prev;
  start = d;
  do
    {
      e = d->parent;
      if (d->side == 'P')
	e = e->prev;
      new = *cdir;
      if (!e->Flags.mark && rule (d->poly, e, &new))
	{
	  if ((d->side == 'N' && new == FORW) ||
	      (d->side == 'P' && new == BACKW))
	    {
#ifdef DEBUG_JUMP
	      if (new == FORW)
		DEBUGP ("jump leaving node at (%ld, %ld)\n",
			e->next->point[0], e->next->point[1]);
	      else
		DEBUGP ("jump leaving node at (%ld, %ld)\n",
			e->point[0], e->point[1]);
#endif
	      *cur = d->parent;
	      *cdir = new;
	      return TRUE;
	    }
	}
    }
  while ((d = d->prev) != start);
  return FALSE;
}

static int
Gather (VNODE * start, PLINE ** result, J_Rule v_rule, DIRECTION initdir)
{
  VNODE *cur = start, *newn;
  DIRECTION dir = initdir;
#ifdef DEBUG_GATHER
  DEBUGP ("gather direction = %d\n", dir);
#endif
  assert (*result == NULL);
  do
    {
      /* see where to go next */
      if (!jump (&cur, &dir, v_rule))
	break;
      /* add edge to polygon */
      if (!*result)
	{
	  *result = poly_NewContour (cur->point);
	  if (*result == NULL)
	    return err_no_memory;
	}
      else
	{
	  if ((newn = poly_CreateNode (cur->point)) == NULL)
	    return err_no_memory;
	  poly_InclVertex ((*result)->head.prev, newn);
	}
#ifdef DEBUG_GATHER
      DEBUGP ("gather vertex at (%ld, %ld)\n", cur->point[0], cur->point[1]);
#endif
      /* Now mark the edge as included.  */
      newn = (dir == FORW ? cur : cur->prev);
      newn->Flags.mark = 1;
      /* for SHARED edge mark both */
      if (newn->shared)
	newn->shared->Flags.mark = 1;

      /* Advance to the next edge.  */
      cur = (dir == FORW ? cur->next : cur->prev);
    }
  while (1);
  return err_ok;
}				/* Gather */

static void
Collect1 (jmp_buf * e, VNODE *cur, DIRECTION dir, POLYAREA **contours, PLINE ** holes, J_Rule j_rule)
{
  PLINE *p = NULL;		/* start making contour */
  int errc = err_ok;
	if ((errc =
	     Gather (dir == FORW ? cur : cur->next, &p, j_rule,
		     dir)) != err_ok)
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
//            printf ("1: ");
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
Collect (jmp_buf * e, PLINE * a, POLYAREA ** contours, PLINE ** holes,
	 S_Rule s_rule, J_Rule j_rule)
{
  VNODE *cur, *other;
  DIRECTION dir = FORW; /* Not sure, but stops valgrind complaining */

  cur = &a->head;
  do
   {
    if (cur->Flags.mark == 0 && s_rule (cur, &dir))
        Collect1(e, cur, dir, contours, holes, j_rule);
    other = cur;
    if ((other->cvc_prev && jump(&other, &dir, j_rule)))
        Collect1(e, other, dir, contours, holes, j_rule);
   }
  while ((cur = cur->next) != &a->head);
}				/* Collect */


static int
cntr_Collect (jmp_buf * e, PLINE ** A, POLYAREA ** contours, PLINE ** holes,
	      int action, POLYAREA *owner, POLYAREA * parent, PLINE *parent_contour)
{
  PLINE *tmprev;

//  printf ("cntr_Collect %p, %p, %p, %i, %p, %p, %p\n",
//          A, contours, holes, action, owner, parent, parent_contour);

  if ((*A)->Flags.status == ISECTED)
    {
      switch (action)
	{
	case PBO_UNITE:
	  Collect (e, *A, contours, holes, UniteS_Rule, UniteJ_Rule);
	  break;
	case PBO_ISECT:
	  Collect (e, *A, contours, holes, IsectS_Rule, IsectJ_Rule);
	  break;
	case PBO_XOR:
	  Collect (e, *A, contours, holes, XorS_Rule, XorJ_Rule);
	  break;
	case PBO_SUB:
	  Collect (e, *A, contours, holes, SubS_Rule, SubJ_Rule);
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
	      /* disappear this contour (rtree entry remove int PutContour) */
	      *A = tmprev->next;
	      tmprev->next = NULL;
//              printf ("2: ");
	      PutContour (e, tmprev, contours, holes, owner, NULL, NULL);
	      return TRUE;
	    }
	  break;
	case PBO_XOR:
	  if ((*A)->Flags.status == INSIDE)
	    {
	      tmprev = *A;
	      /* disappear this contour (rtree entry remove int PutContour) */
	      *A = tmprev->next;
	      tmprev->next = NULL;
	      poly_InvContour (tmprev);
//              printf ("3: ");
	      PutContour (e, tmprev, contours, holes, owner, NULL, NULL);
	      return TRUE;
	    }
          /* BUG? Should we put this contour non-inverted if it is outside B? */
	  /* break; */ /* Fall through */
	case PBO_UNITE:
	case PBO_SUB:
	  if ((*A)->Flags.status == OUTSIDE)
	    {
	      tmprev = *A;
	      /* disappear this contour (rtree entry remove int PutContour) */
	      *A = tmprev->next;
	      tmprev->next = NULL;
//              printf ("4: ");
	      PutContour (e, tmprev, contours, holes, owner, parent, parent_contour);
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
	    continue;

	  if ((*cur)->Flags.status == INSIDE)
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
//                printf ("5: ");
		PutContour (e, tmp, contours, holes, b, NULL, NULL); /* b */
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
//                printf ("6: ");
		PutContour (e, tmp, contours, holes, b, NULL, NULL); /* b */
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
contour_is_first (POLYAREA *a, PLINE *cur)
{
  return (a->contours == cur);
}


static inline int
contour_is_last (PLINE *cur)
{
  return (cur->next == NULL);
}


static inline void
remove_polyarea (POLYAREA **list, POLYAREA *piece)
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
}

static void
M_POLYAREA_separate_isected (jmp_buf * e, POLYAREA ** pieces,
                             PLINE ** holes, PLINE ** isected)
{
  POLYAREA *a = *pieces;
  POLYAREA *anext;
  PLINE *curc, *next, *prev;
  int finished;

  if (a == NULL) {
//    printf ("M_POLYAREA_separate_isected: No polygon pieces to play with\n");
    return;
  }

  /* TODO: STASH ENOUGH INFORMATION EARLIER ON, SO WE CAN REMOVE THE INTERSECTED
           CONTOURS WITHOUT HAVING TO WALK THE FULL DATA-STRUCTURE LOOKING FOR THEM. */

  do {
    int hole_contour = 0;
    int is_outline = 1;

    anext = a->f;
    finished = (anext == *pieces);

    prev = NULL;
    for (curc = a->contours; curc != NULL; curc = next, is_outline = 0) {
      int is_first = contour_is_first (a, curc);
      int is_last = contour_is_last (curc);
      int isect_contour = (curc->Flags.status == ISECTED);

      next = curc->next;

      if (isect_contour || hole_contour) {

        /* Reset the intersection flags, since we keep these pieces */
        if (curc->Flags.status != ISECTED)
          curc->Flags.status = UNKNWN;

        remove_contour (a, prev, curc, !(is_first && is_last));

        if (isect_contour) {
          /* Link into the list of intersected contours */
          curc->next = *isected;
          *isected = curc;
        } else if (hole_contour) {
//          printf ("Hole contour, perhaps would have been saved the trouble if we mass-evicted children\n");
          /* Link into the list of holes */
          curc->next = *holes;
          *holes = curc;
        }

        if (is_first && is_last) {
//          printf ("M_POLYAREA_separate_isected: Deleted / removed the whole polygon piece\n");
          remove_polyarea (pieces, a);
          poly_Free (&a); /* NB: Sets a to NULL */
        }

      } else {
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
  } while ((a = anext), *pieces !=NULL && !finished);
}


struct find_inside_m_pa_info {
  jmp_buf jb;
  POLYAREA *want_inside;
  PLINE *result;
};

static int
find_inside_m_pa (const BoxType *b, void *cl)
{
  struct find_inside_m_pa_info *info = cl;
  PLINE *check = (PLINE *) b;
  /* Don't report for the main contour */
  if (check->Flags.orient == PLF_DIR)
    return 0;
  /* Don't look at contours marked as being intersected */
  if (check->Flags.status == ISECTED)
    return 0;
  if (cntr_in_M_POLYAREA (check, info->want_inside, FALSE)) {
//    printf ("find_inside_m_pa: Found a hole inside the one we're checking\n");
    info->result = check;
    longjmp (info->jb, 1);
  }
  return 0;
}


static void
M_POLYAREA_update_primary (jmp_buf * e, POLYAREA ** pieces,
                           PLINE ** holes, int action, POLYAREA *bpa)
{
  POLYAREA *a = *pieces;
  POLYAREA *b;
  POLYAREA *anext;
  PLINE *curc, *next, *prev;
  BoxType box;
  int inv_inside = 0;
  int del_inside = 0;
  int del_outside = 0;
  int finished;

//  printf ("M_POLYAREA_update_primary %p, %p, %i\n", pieces, holes, action);

  if (a == NULL) {
//    printf ("M_POLYAREA_update_primary: No polygon pieces to play with\n");
    return;
  }

  switch (action) {
    case PBO_ISECT:
      del_outside = 1;
      break;
    case PBO_UNITE:
      del_inside = 1;
      break;
    case PBO_SUB:
      del_inside = 1;
      break;
    case PBO_XOR: /* NOT IMPLEMENTED OR USED */
      inv_inside = 1;
      break;
  }

  box = *((BoxType *)bpa->contours);
  b = bpa;
  while ((b = b->f) != bpa) {
    BoxType *b_box = (BoxType *)b->contours;
    MAKEMIN (box.X1, b_box->X1);
    MAKEMIN (box.Y1, b_box->Y1);
    MAKEMAX (box.X2, b_box->X2);
    MAKEMAX (box.Y2, b_box->Y2);
  }

#if 1
  if (del_inside) {

    do {
      anext = a->f;
      finished = (anext == *pieces);

      /* Test the outer contour first, as we may need to remove all children */

      /* We've not yet split intersected contours out, just ignore them */
      if (a->contours->Flags.status != ISECTED &&
          /* Pre-filter on bounding box */
          ((a->contours->xmin >= box.X1) && (a->contours->ymin >= box.Y1) &&
           (a->contours->xmax <= box.X2) && (a->contours->ymax <= box.Y2)) &&
          /* Then test properly */
          cntr_in_M_POLYAREA (a->contours, bpa, FALSE)) {

        /* Delete this contour, all children -> holes queue */
//        printf ("Outer contour needs to be deleted, and children moved to hole queue\n");

        /* Delete the outer contour */
        curc = a->contours;
        remove_contour (a, NULL, curc, FALSE); /* Rtree deleted in poly_Free below */
        /* a->contours now points to the remaining holes */
        poly_DelContour (&curc);

        if (a->contours != NULL) {
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
        poly_Free (&a); /* NB: Sets a to NULL */

        continue;
      }

      /* Loop whilst we find INSIDE contours to delete */
      while (1) {
        struct find_inside_m_pa_info info;
        PLINE *prev;

        info.want_inside = bpa;

        /* Set jump return */
        if (setjmp (info.jb)) {
          /* Returned here! */
        } else {
          info.result = NULL;
          /* Rtree search, calling back a routine to longjmp back with data about any hole inside the B polygon */
          /*   Be sure not to bother jumping back to report the main contour! */
          r_search (a->contour_tree, &box, NULL, find_inside_m_pa, &info);

          /* Nothing found? */
          break;
        }

        /* We need to find the contour before it, so we can update its next pointer */
        prev = a->contours;
        while (prev->next != info.result) {
          prev = prev->next;
        }

        /* Remove hole from the contour */
        remove_contour (a, prev, info.result, TRUE);
        poly_DelContour (&info.result);
      }
      /* End check for deleted holes */

    /* If we deleted all the pieces of the polyarea, *pieces is NULL */
    } while ((a = anext), *pieces != NULL && !finished);

    return;
  } else {
    printf ("Sorry, this isn't optimised for speed.....\n");
  }
#endif

  do {
    int hole_contour = 0;
    int is_outline = 1;

    anext = a->f;
    finished = (anext == *pieces);

    prev = NULL;
    for (curc = a->contours; curc != NULL; curc = next, is_outline = 0) {
      int is_first = contour_is_first (a, curc);
      int is_last = contour_is_last (curc);

      int del_contour = 0;
//      int inv_contour = 0;

      next = curc->next;

#if 0
      switch (curc->Flags.status) {
        case ISECTED:
          printf ("Found intersected contour!! BADNESS\n");
          break;
        case INSIDE:
          if (del_inside) del_contour = 1;
          if (inv_inside) inv_contour = 1;
          break;
        case OUTSIDE:
          if (del_outside) del_contour = 1;
          break;
      }
#endif

      if (del_outside)
        del_contour = curc->Flags.status != ISECTED &&
                     !cntr_in_M_POLYAREA (curc, bpa, FALSE);

      /* Reset the intersection flags, since we keep these pieces */
      if (curc->Flags.status != ISECTED)
        curc->Flags.status = UNKNWN;

      if (del_contour || hole_contour) {

        remove_contour (a, prev, curc, !(is_first && is_last));

        if (del_contour) {
          /* Delete the contour */
          poly_DelContour (&curc); /* NB: Sets curc to NULL */
//          printf ("Deleting contour we don't want in the result\n");
        } else if (hole_contour) {
          /* Link into the list of holes */
          curc->next = *holes;
          *holes = curc;
//          printf ("Separating a hole (belonging to a moved contour)\n");
        } else {
          assert (0);
        }

        if (is_first && is_last) {
//          printf ("M_POLYAREA_update_primary: Deleted / removed the whole polygon piece\n");
          remove_polyarea (pieces, a);
          poly_Free (&a); /* NB: Sets a to NULL */
        }

      } else {
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
  } while ((a = anext), *pieces != NULL && !finished);
}



static void
M_POLYAREA_update_primary_old (jmp_buf * e, POLYAREA ** pieces,
                           PLINE ** holes, PLINE ** isected, int action)
{
  POLYAREA *a = *pieces;
  POLYAREA *anext;
  PLINE *curc, *next, *prev;
  int inv_inside = 0;
  int del_inside = 0;
  int del_outside = 0;
  int finished;

//  printf ("M_POLYAREA_update_primary %p, %p, %i\n", pieces, holes, action);

  if (a == NULL) {
    printf ("M_POLYAREA_update_primary: No polygon pieces to play with\n");
    return;
  }

  switch (action) {
    case PBO_ISECT:
//      printf ("  PBO_ISECT: Delete any contours OUTSIDE b\n");
      del_outside = 1;
      break;
    case PBO_UNITE:
//      printf ("  PBO_UNITE: Delete any contours INSIDE B (B's contour replaces it)\n");
      del_inside = 1;
      break;
    case PBO_SUB:
//      printf ("  PBO_SUB: Delete any contours INSIDE B (B's contour deletes it)\n");
      del_inside = 1;
      break;
    case PBO_XOR: /* NOT IMPLEMENTED OR USED */
//      printf ("  PBO_XOR: Invert any which are INSIDE B  *** NOT IMPLEMENTED ***\n");
      inv_inside = 1;
      break;
  }

  /* now the non-intersect parts are collected in temp/holes */
  do {
    int hole_contour = 0;
    int is_outline = 1;

    anext = a->f;
    finished = (anext == *pieces);

//    printf ("Inspecting a piece of polygon\n");

    prev = NULL;
    for (curc = a->contours; curc != NULL; curc = next, is_outline = 0) {
      int is_first = contour_is_first (a, curc);
      int is_last = contour_is_last (curc);

      int del_contour = 0;
      int inv_contour = 0;
      int isect_contour = 0;

      next = curc->next;

      switch (curc->Flags.status) {
        case ISECTED:
          isect_contour = 1;
//          printf ("Found intersected contour\n");
          break;
        case INSIDE:
          if (del_inside) del_contour = 1;
          if (inv_inside) inv_contour = 1;
          break;
        case OUTSIDE:
          if (del_outside) del_contour = 1;
          break;
      }

      /* Reset the intersection flags, since we keep these pieces */
      if (curc->Flags.status != ISECTED)
        curc->Flags.status = UNKNWN;

      if (del_contour || isect_contour || hole_contour) {

        remove_contour (a, prev, curc, !(is_first && is_last));

        if (del_contour) {
          /* Delete the contour */
          poly_DelContour (&curc); /* NB: Sets curc to NULL */
//          printf ("Deleting contour we don't want in the result\n");
        } else if (isect_contour) {
          /* Link into the list of intersected contours */
          curc->next = *isected;
          *isected = curc;
//          printf ("Separating intersected contour.\n");
        } else if (hole_contour) {
          /* Link into the list of holes */
          curc->next = *holes;
          *holes = curc;
//          printf ("Separating a hole (belonging to a moved contour)\n");
        } else {
          assert (0);
        }

        if (is_first && is_last) {
//          printf ("M_POLYAREA_update_primary: Deleted / removed the whole polygon piece\n");
          remove_polyarea (pieces, a);
          poly_Free (&a); /* NB: Sets a to NULL */
        }

      } else {
        /* Note the item we just didn't delete as the next
           candidate for having its "next" pointer adjusted.
           Saves walking the contour list when we delete one. */
        prev = curc;
      }

      /* If we move or delete an outer contour, we need to move any holes
         we wish to keep within that contour to the holes list. */
      if (is_outline && (del_contour || isect_contour))
        hole_contour = 1;

    }

    /* If we deleted all the pieces of the polyarea, *pieces is NULL and
       we don't want to continue */
    if (*pieces == NULL) {
//      printf ("M_POLYAREA_update_primary: Deleted / removed _all_"
//              "of the existing polygon pieces\n");
      finished = TRUE;
    }

  } while ((a = anext), !finished);
}


static void
M_POLYAREA_Collect_separated (jmp_buf * e, PLINE * afst, POLYAREA ** contours,
                              PLINE ** holes, int action, BOOLp maybe)
{
  PLINE **cur, **next;

//  printf ("M_POLYAREA_Collect_separated %p, %p, %p, %i, %i\n", afst, contours, holes, action, maybe);

  assert (a != NULL);

  for (cur = &afst; *cur != NULL; cur = next) {
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
  POLYAREA *parent = NULL; /* Quiet GCC warning */
  PLINE **cur, **next, *parent_contour;

//  printf ("M_POLYAREA_Collect %p, %p, %p, %i, %i\n", afst, contours, holes, action, maybe);

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
//          printf ("1: ");
          if (cntr_Collect (e, cur, contours, holes, action, a, NULL, NULL))
            {
              parent = (*contours)->b; /* InsCntr inserts behind the head */
              next = cur;
            }
          else
            parent = a;
          cur = next;
        }
      for ( ; *cur != NULL; cur = next)
        {
          next = &((*cur)->next);
          /* if we disappear a contour, don't advance twice */
          if (*cur == parent_contour)
            printf ("WTF??\n");
//          printf ("2: ");
          if (cntr_Collect (e, cur, contours, holes, action, a, parent,
                            (*cur == parent_contour) ? NULL : parent_contour))
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
//      M_POLYAREA_intersect (&e, a, b, False);
      M_POLYAREA_intersect2 (&e, a, b, False);

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

  if (!poly_M_Copy0 (&a, a_org) || !poly_M_Copy0 (&b, b_org))
    return err_no_memory;

  return poly_Boolean_free (a, b, res, action);
}				/* poly_Boolean */

/* just like poly_Boolean but frees the input polys */
int
poly_Boolean_free (POLYAREA * ai, POLYAREA * bi, POLYAREA ** res, int action)
{
  POLYAREA *a = ai, *b = bi;
  PLINE *a_isected = NULL;
  PLINE *p, *holes = NULL;
  jmp_buf e;
  int code;

  *res = NULL;

  if (!a)
    {
      switch (action)
	{
	case PBO_XOR:
	case PBO_UNITE:
	  *res = bi;
	case PBO_SUB:
	case PBO_ISECT:
	  return err_ok;
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
	case PBO_ISECT:
	  return err_ok;
	}
    }

  if (a->contours == NULL) {
    fprintf (stderr, "A has no contours bye!\n");
    return -1;
  }

  if ((code = setjmp (e)) == 0)
    {
#ifdef DEBUG
      assert (poly_Valid (a));
      assert (poly_Valid (b));
#endif

#if 0
/* SANITY CHECK */
  {
    POLYAREA *apa;
    PLINE *curc;

    apa = a;
    do {
      for (curc = apa->contours; curc != NULL; curc = curc->next) {
        if (curc->Flags.status != UNKNWN) {
          curc->Flags.status = UNKNWN;
          printf ("SOMETHING DIDN'T CLEAR THE FLAGS\n");
        }
      }
      /* If we deleted all the pieces of the polyarea, *pieces is NULL */
    } while ((apa = apa->f) != a);
  }
  {
    POLYAREA *apa = a;
    PLINE *curc;
    do {
      int count = 0;
      for (curc = apa->contours; curc != NULL; curc = curc->next)
        count ++;
      if (apa->contour_tree->size != count)
        printf ("A: Contour rtree has %i elements, counted %i\n",
                apa->contour_tree->size, count);
    } while ((apa = apa->f) != a);
  }
  {
    POLYAREA *bpa = b;
    PLINE *curc;
    do {
      int count = 0;
      for (curc = bpa->contours; curc != NULL; curc = curc->next)
        count ++;
      if (bpa->contour_tree->size != count)
        printf ("B: Contour rtree has %i elements, counted %i\n",
                bpa->contour_tree->size, count);
    } while ((bpa = bpa->f) != b);
  }
/* END SANITY CHECK */
#endif

      /* intersect needs to make a list of the contours in a and b which are intersected */
      M_POLYAREA_intersect (&e, a, b, TRUE);

      /* We could speed things up a lot here if we only processed the relevant contours */
//      M_POLYAREA_label (a, b, FALSE);
      M_POLYAREA_label (b, a, FALSE);

      *res = a;
      M_POLYAREA_update_primary (&e, res, &holes, action, b);
//      M_POLYAREA_update_primary_old (&e, res, &holes, &a_isected, action);
      M_POLYAREA_separate_isected (&e, res, &holes, &a_isected);
      M_POLYAREA_label_separated (a_isected, b, FALSE);
      M_POLYAREA_Collect_separated (&e, a_isected, res, &holes, action, FALSE);
      /* delete of a_isected which is left */
      while ((p = a_isected) != NULL)
      {
        a_isected = p->next;
        poly_DelContour (&p);
      }
      M_B_AREA_Collect (&e, b, res, &holes, action);
      poly_Free (&b);

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
  assert (!*res || poly_Valid (*res));
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
	  while ((v = v->next) != &c->head);
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

//      printf ("3:");
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
//      printf ("4:");
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

VNODE *
poly_CreateNode (Vector v)
{
  VNODE *res;
  register int *c;

  assert (v);
  res = (VNODE *) calloc (1, sizeof (VNODE));
  if (res == NULL)
    return NULL;
  // bzero (res, sizeof (VNODE) - sizeof(Vector));
  c = res->point;
  *c++ = *v++;
  *c = *v;
  return res;
}

void
poly_IniContour (PLINE * c)
{
  if (c == NULL)
    return;
  /* bzero (c, sizeof(PLINE)); */
  c->head.next = c->head.prev = &c->head;
  c->xmin = c->ymin = 0x7fffffff;
  c->xmax = c->ymax = 0x80000000;
}

PLINE *
poly_NewContour (Vector v)
{
  PLINE *res;

  res = (PLINE *) calloc (1, sizeof (PLINE));
  if (res == NULL)
    return NULL;

  poly_IniContour (res);

  if (v != NULL)
    {
      Vcopy (res->head.point, v);
      cntrbox_adjust (res, v);
    }

  return res;
}

void
poly_ClrContour (PLINE * c)
{
  VNODE *cur;

  assert (c != NULL);
  while ((cur = c->head.next) != &c->head)
    {
      poly_ExclVertex (cur);
      free (cur);
    }
  poly_IniContour (c);
}

void
poly_DelContour (PLINE ** c)
{
  VNODE *cur;

  if (*c == NULL)
    return;
  for (cur = (*c)->head.prev; cur != &(*c)->head; free (cur->next))
    cur = cur->prev;
  /* FIXME -- strict aliasing violation.  */
  if ((*c)->tree)
    {
      rtree_t *r = (*c)->tree;
      r_destroy_tree (&r);
    }
  free (*c), *c = NULL;
}

void
poly_PreContour (PLINE * C, BOOLp optimize)
{
  double area = 0;
  VNODE *p, *c;
  Vector p1, p2;

  assert (C != NULL);

  if (optimize)
    {
      for (c = (p = &C->head)->next; c != &C->head; c = (p = c)->next)
	{
	  /* if the previous node is on the same line with this one, we should remove it */
	  Vsub2 (p1, c->point, p->point);
	  Vsub2 (p2, c->next->point, c->point);
	  /* If the product below is zero then
	   * the points on either side of c 
	   * are on the same line!
	   * So, remove the point c
	   */

	  if (vect_det2 (p1, p2) == 0)
	    {
	      poly_ExclVertex (c);
	      free (c);
	      c = p;
	    }
	}
    }
  C->Count = 0;
  C->xmin = C->xmax = C->head.point[0];
  C->ymin = C->ymax = C->head.point[1];

  p = (c = &C->head)->prev;
  if (c != p)
    {
      do
	{
	  /* calculate area for orientation */
	  area +=
	    (double) (p->point[0] - c->point[0]) * (p->point[1] +
						    c->point[1]);
	  cntrbox_adjust (C, c->point);
	  C->Count++;
	}
      while ((c = (p = c)->next) != &C->head);
    }
  C->area = ABS (area);
  if (C->Count > 2)
    C->Flags.orient = ((area < 0) ? PLF_INV : PLF_DIR);
  C->tree = make_edge_tree (C);
}				/* poly_PreContour */

static int
flip_cb (const BoxType * b, void *cl)
{
  struct seg *s = (struct seg *) b;
  s->v = s->v->prev;
  return 1;
}

void
poly_InvContour (PLINE * c)
{
  VNODE *cur, *next;
  int r;

  assert (c != NULL);
  cur = &c->head;
  do
    {
      next = cur->next;
      cur->next = cur->prev;
      cur->prev = next;
      /* fix the segment tree */
    }
  while ((cur = next) != &c->head);
  c->Flags.orient ^= 1;
  if (c->tree)
    {
      r = r_search (c->tree, NULL, NULL, flip_cb, NULL);
      assert (r == c->Count);
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
  node->prev->next = node->next;
  node->next->prev = node->prev;
}

void
poly_InclVertex (VNODE * after, VNODE * node)
{
  double a, b;
  assert (after != NULL);
  assert (node != NULL);

  node->prev = after;
  node->next = after->next;
  after->next = after->next->prev = node;
  /* remove points on same line */
  if (node->prev->prev == node)
    return;			/* we don't have 3 points in the poly yet */
  a = (node->point[1] - node->prev->prev->point[1]);
  a *= (node->prev->point[0] - node->prev->prev->point[0]);
  b = (node->point[0] - node->prev->prev->point[0]);
  b *= (node->prev->point[1] - node->prev->prev->point[1]);
  if (fabs (a - b) < EPSILON)
    {
      VNODE *t = node->prev;
      t->prev->next = node;
      node->prev = t->prev;
      free (t);
    }
}

BOOLp
poly_CopyContour (PLINE ** dst, PLINE * src)
{
  VNODE *cur, *newnode;

  assert (src != NULL);
  *dst = NULL;
  *dst = poly_NewContour (src->head.point);
  if (*dst == NULL)
    return FALSE;

  (*dst)->Count = src->Count;
  (*dst)->Flags.orient = src->Flags.orient;
  (*dst)->xmin = src->xmin, (*dst)->xmax = src->xmax;
  (*dst)->ymin = src->ymin, (*dst)->ymax = src->ymax;
  (*dst)->area = src->area;

  for (cur = src->head.next; cur != &src->head; cur = cur->next)
    {
      if ((newnode = poly_CreateNode (cur->point)) == NULL)
	return FALSE;
      // newnode->Flags = cur->Flags;
      poly_InclVertex ((*dst)->head.prev, newnode);
    }
  (*dst)->tree = make_edge_tree (*dst);
  return TRUE;
}

/**********************************************************************/
/* polygon routines */

BOOLp
poly_Copy0 (POLYAREA ** dst, const POLYAREA * src)
{
  *dst = NULL;
  if (src != NULL)
    *dst = calloc (1, sizeof (POLYAREA));
  if (*dst == NULL)
    return FALSE;
  (*dst)->contour_tree = r_create_tree (NULL, 0, 0);

  return poly_Copy1 (*dst, src);
}

BOOLp
poly_Copy1 (POLYAREA * dst, const POLYAREA * src)
{
  PLINE *cur, **last = &dst->contours;

  *last = NULL;
  dst->f = dst->b = dst;

  for (cur = src->contours; cur != NULL; cur = cur->next)
    {
      if (!poly_CopyContour (last, cur))
	return FALSE;
      r_insert_entry (dst->contour_tree, (BoxTypePtr) *last, 0);
      last = &(*last)->next;
    }
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
    return FALSE;
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
      r_insert_entry (p->contour_tree, (BoxTypePtr) c, 0);
    }
  else
    {
      if (p->contours == NULL)
	return FALSE;
      /* link at front of hole list */
      tmp = p->contours->next;
      p->contours->next = c;
      c->next = tmp;
      r_insert_entry (p->contour_tree, (BoxTypePtr) c, 0);
    }
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

  if (s->v->point[1] <= p->p[1])
    {
      if (s->v->next->point[1] > p->p[1])
	{
	  Vector v1, v2;
	  long long cross;
	  Vsub2 (v1, s->v->next->point, s->v->point);
	  Vsub2 (v2, p->p, s->v->point);
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
      if (s->v->next->point[1] <= p->p[1])
	{
	  Vector v1, v2;
	  long long cross;
	  Vsub2 (v1, s->v->next->point, s->v->point);
	  Vsub2 (v2, p->p, s->v->point);
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
  ray.X2 = 0x7fffffff;
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

int
poly_ContourInContour (PLINE * poly, PLINE * inner)
{
  assert (poly != NULL);
  assert (inner != NULL);
  if (cntrbox_inside (inner, poly))
    return poly_InsideContour (poly, inner->head.point);
  return 0;
}

void
poly_Init (POLYAREA * p)
{
  p->f = p->b = p;
  p->contours = NULL;
  p->contour_tree = r_create_tree (NULL, 0, 0);
}

POLYAREA *
poly_Create (void)
{
  POLYAREA *res;

  if ((res = malloc (sizeof (POLYAREA))) != NULL)
    poly_Init (res);
  return res;
}

void
poly_Clear (POLYAREA * P)
{
  PLINE *p;

  assert (P != NULL);
  while ((p = P->contours) != NULL)
    {
      P->contours = p->next;
      poly_DelContour (&p);
    }
  r_destroy_tree (&P->contour_tree);
}

void
poly_Free (POLYAREA ** p)
{
  POLYAREA *cur;
  if (*p == NULL)
    return;
  for (cur = (*p)->f; cur != *p; cur = (*p)->f)
    {
      poly_Clear (cur);
      cur->f->b = cur->b;
      cur->b->f = cur->f;
      free (cur);
    }
  poly_Clear (cur);
  free (*p), *p = NULL;
}

static BOOLp
inside_sector (VNODE * pn, Vector p2)
{
  Vector cdir, ndir, pdir;
  int p_c, n_c, p_n;

  assert (pn != NULL);
  vect_sub (cdir, p2, pn->point);
  vect_sub (pdir, pn->point, pn->prev->point);
  vect_sub (ndir, pn->next->point, pn->point);

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
  VNODE *a1, *a2, *a2_start, *hit1, *hit2;
  Vector i1, i2;
  int icnt;
  double d1,d2;

#warning FIXME Later: Deliberately disabled this test - seems something strange is going on
  return FALSE;

  assert (a != NULL);
  a1 = &a->head;
  do
    {
      a2_start = a2 = a1;
      do
	{
	  if (!node_neighbours (a1, a2) &&
	      (icnt = vect_inters2 (a1->point, a1->next->point,
				    a2->point, a2->next->point, i1, i2)) > 0)
	    {
	      if (icnt > 1)
		  return TRUE;

              d1 = -1; d2 = -1;
	      if ((d1=vect_dist2 (i1, a1->point)) < EPSILON)
		hit1 = a1;
	      else if ((d2=vect_dist2 (i1, a1->next->point)) < EPSILON)
		hit1 = a1->next;
	      else
		  return TRUE;

	      if (vect_dist2 (i1, a2->point) < EPSILON)
		hit2 = a2;
	      else if (vect_dist2 (i1, a2->next->point) < EPSILON)
		hit2 = a2->next;
	      else
		  return TRUE;

#if 1
	      /* now check if they are inside each other */
	      if (inside_sector (hit1, hit2->prev->point) ||
		  inside_sector (hit1, hit2->next->point) ||
		  inside_sector (hit2, hit1->prev->point) ||
		  inside_sector (hit2, hit1->next->point))
		  return TRUE;
#endif
	    }
	}
      while ((a2 = a2->next) != a2_start);
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
      VNODE *v;
      DEBUGP ("Invalid Outer PLINE\n");
      if (p->contours->Flags.orient == PLF_INV)
	DEBUGP ("failed orient\n");
      if (poly_ChkContour (p->contours))
	DEBUGP ("failed self-intersection\n");
      v = &p->contours->head;
      do
	{
	  fprintf (stderr, "%d %d 100 100 \"\"]\n", v->point[0], v->point[1]);
	  fprintf (stderr, "Line [%d %d ", v->point[0], v->point[1]);
	}
      while ((v = v->next) != &p->contours->head);
#endif
      return FALSE;
    }
  for (c = p->contours->next; c != NULL; c = c->next)
    {
      if (c->Flags.orient == PLF_DIR ||
	  poly_ChkContour (c) || !poly_ContourInContour (p->contours, c))
	{
#ifndef NDEBUG
	  VNODE *v;
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
	      fprintf (stderr, "%d %d 100 100 \"\"]\n", v->point[0],
		       v->point[1]);
	      fprintf (stderr, "Line [%d %d ", v->point[0], v->point[1]);
	    }
	  while ((v = v->next) != &c->head);
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

/*
vect_inters2
 (C) 1993 Klamer Schutte
 (C) 1997 Michael Leonov, Alexey Nikitin
*/

int
vect_inters2 (Vector p1, Vector p2, Vector q1, Vector q2,
	      Vector S1, Vector S2)
{
  double s, t, deel;
  double rpx, rpy, rqx, rqy;

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
   * if the lines are parallel
   */

  if (deel == 0)		/* parallel */
    {
      double dc1, dc2, d1, d2, h;	/* Check to see whether p1-p2 and q1-q2 are on the same line */
      Vector hp1, hq1, hp2, hq2, q1p1, q1q2;

      Vsub2 (q1p1, q1, p1);
      Vsub2 (q1q2, q1, q2);


      /* If this product is not zero then p1-p2 and q1-q2 are not on same line! */
      if (vect_det2 (q1p1, q1q2) != 0)
	return 0;
      dc1 = 0;			/* m_len(p1 - p1) */

      dc2 = vect_m_dist (p1, p2);
      d1 = vect_m_dist (p1, q1);
      d2 = vect_m_dist (p1, q2);

/* Sorting the independent points from small to large */
      Vcpy2 (hp1, p1);
      Vcpy2 (hp2, p2);
      Vcpy2 (hq1, q1);
      Vcpy2 (hq2, q2);
      if (dc1 > dc2)
	{			/* hv and h are used as help-variable. */
	  Vswp2 (hp1, hp2);
	  h = dc1, dc1 = dc2, dc2 = h;
	}
      if (d1 > d2)
	{
	  Vswp2 (hq1, hq2);
	  h = d1, d1 = d2, d2 = h;
	}

/* Now the line-pieces are compared */

      if (dc1 < d1)
	{
	  if (dc2 < d1)
	    return 0;
	  if (dc2 < d2)
	    {
	      Vcpy2 (S1, hp2);
	      Vcpy2 (S2, hq1);
	    }
	  else
	    {
	      Vcpy2 (S1, hq1);
	      Vcpy2 (S2, hq2);
	    };
	}
      else
	{
	  if (dc1 > d2)
	    return 0;
	  if (dc2 < d2)
	    {
	      Vcpy2 (S1, hp1);
	      Vcpy2 (S2, hp2);
	    }
	  else
	    {
	      Vcpy2 (S1, hp1);
	      Vcpy2 (S2, hq2);
	    };
	}
      return (Vequ2 (S1, S2) ? 1 : 2);
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

      if (Vequ2 (q1, p1) || Vequ2 (q1, p2))
	{
	  S1[0] = q1[0];
	  S1[1] = q1[1];
	}
      else if (Vequ2 (q2, p1) || Vequ2 (q2, p2))
	{
	  S1[0] = q2[0];
	  S1[1] = q2[1];
	}
      else
	{
	  s = (rqy * (p1[0] - q1[0]) + rqx * (q1[1] - p1[1])) / deel;
	  if (s < 0 || s > 1.)
	    return 0;
	  t = (rpy * (p1[0] - q1[0]) + rpx * (q1[1] - p1[1])) / deel;
	  if (t < 0 || t > 1.)
	    return 0;

	  S1[0] = q1[0] + ROUND (t * rqx);
	  S1[1] = q1[1] + ROUND (t * rqy);
	}
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
