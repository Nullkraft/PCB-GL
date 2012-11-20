#include "global.h"
#include "hid.h"
#include "hid_draw.h"
#include "polygon.h"
#include "draw_api.h"
#include "data.h"    /* <-- For nasty global "PCB" */

static void
fill_contour (DrawAPI *dapi, PLINE *pl)
{
  Coord *x, *y, n, i = 0;
  VNODE *v;

  n = pl->Count;
  x = (Coord *)malloc (n * sizeof (*x));
  y = (Coord *)malloc (n * sizeof (*y));

  for (v = &pl->head; i < n; v = v->next)
    {
      x[i] = v->point[0];
      y[i++] = v->point[1];
    }

  dapi->graphics->fill_polygon (dapi->fg_gc, n, x, y);

  free (x);
  free (y);
}

static void
thindraw_contour (DrawAPI *dapi, PLINE *pl)
{
  VNODE *v;
  Coord last_x, last_y;
  Coord this_x, this_y;

  dapi->graphics->set_line_width (dapi->fg_gc, 0);
  dapi->graphics->set_line_cap (dapi->fg_gc, Round_Cap);

  /* If the contour is round, use an arc drawing routine. */
  if (pl->is_round)
    {
      dapi->graphics->draw_arc (dapi->fg_gc, pl->cx, pl->cy, pl->radius, pl->radius, 0, 360);
      return;
    }

  /* Need at least two points in the contour */
  if (pl->head.next == NULL)
    return;

  last_x = pl->head.point[0];
  last_y = pl->head.point[1];
  v = pl->head.next;

  do
    {
      this_x = v->point[0];
      this_y = v->point[1];

      dapi->graphics->draw_line (dapi->fg_gc, last_x, last_y, this_x, this_y);
      // dapi->graphics->fill_circle (dapi->fg_gc, this_x, this_y, 30);

      last_x = this_x;
      last_y = this_y;
    }
  while ((v = v->next) != pl->head.next);
}

static void
fill_contour_cb (PLINE *pl, void *user_data)
{
  DrawAPI *dapi = user_data;
  PLINE *local_pl = pl;

  fill_contour (dapi, pl);
  poly_FreeContours (&local_pl);
}

static void
fill_clipped_contour (DrawAPI *dapi, PLINE *pl)
{
  PLINE *pl_copy;
  POLYAREA *clip_poly;
  POLYAREA *piece_poly;
  POLYAREA *clipped_pieces;
  POLYAREA *draw_piece;
  int x;

  clip_poly = RectPoly (dapi->clip_box->X1, dapi->clip_box->X2,
                        dapi->clip_box->Y1, dapi->clip_box->Y2);
  poly_CopyContour (&pl_copy, pl);
  piece_poly = poly_Create ();
  poly_InclContour (piece_poly, pl_copy);
  x = poly_Boolean_free (piece_poly, clip_poly,
                         &clipped_pieces, PBO_ISECT);
  if (x != err_ok || clipped_pieces == NULL)
    return;

  draw_piece = clipped_pieces;
  do
    {
      /* NB: The polygon won't have any holes in it */
      fill_contour (dapi, draw_piece->contours);
    }
  while ((draw_piece = draw_piece->f) != clipped_pieces);
  poly_Free (&clipped_pieces);
}

/* If at least 50% of the bounding box of the polygon is on the screen,
 * lets compute the complete no-holes polygon.
 */
#define BOUNDS_INSIDE_CLIP_THRESHOLD 0.5
static int
should_compute_no_holes (PolygonType *poly, const BoxType *clip_box)
{
  Coord x1, x2, y1, y2;
  double poly_bounding_area;
  double clipped_poly_area;

  /* If there is no passed clip box, compute the whole thing */
  if (clip_box == NULL)
    return 1;

  x1 = MAX (poly->BoundingBox.X1, clip_box->X1);
  x2 = MIN (poly->BoundingBox.X2, clip_box->X2);
  y1 = MAX (poly->BoundingBox.Y1, clip_box->Y1);
  y2 = MIN (poly->BoundingBox.Y2, clip_box->Y2);

  /* Check if the polygon is outside the clip box */
  if ((x2 <= x1) || (y2 <= y1))
    return 0;

  poly_bounding_area = (double)(poly->BoundingBox.X2 - poly->BoundingBox.X1) *
                       (double)(poly->BoundingBox.Y2 - poly->BoundingBox.Y1);

  clipped_poly_area = (double)(x2 - x1) * (double)(y2 - y1);

  if (clipped_poly_area / poly_bounding_area >= BOUNDS_INSIDE_CLIP_THRESHOLD)
    return 1;

  return 0;
}
#undef BOUNDS_INSIDE_CLIP_THRESHOLD

static void
common_fill_pcb_polygon (DrawAPI *dapi, LayerType *layer, PolygonType *poly)
{
  if (!poly->NoHolesValid)
    {
      /* If enough of the polygon is on-screen, compute the entire
       * NoHoles version and cache it for later rendering, otherwise
       * just compute what we need to render now.
       */
      if (should_compute_no_holes (poly, dapi->clip_box))
        ComputeNoHoles (poly);
      else
        NoHolesPolygonDicer (poly, dapi->clip_box, fill_contour_cb, dapi);
    }
  if (poly->NoHolesValid && poly->NoHoles)
    {
      PLINE *pl;

      for (pl = poly->NoHoles; pl != NULL; pl = pl->next)
        {
          if (dapi->clip_box == NULL)
            fill_contour (dapi, pl);
          else
            fill_clipped_contour (dapi, pl);
        }
    }

  /* Draw other parts of the polygon if fullpoly flag is set */
  /* NB: No "NoHoles" cache for these */
  if (TEST_FLAG (FULLPOLYFLAG, poly))
    {
      PolygonType p = *poly;

      for (p.Clipped = poly->Clipped->f;
           p.Clipped != poly->Clipped;
           p.Clipped = p.Clipped->f)
        NoHolesPolygonDicer (&p, dapi->clip_box, fill_contour_cb, dapi);
    }
}

static int
thindraw_hole_cb (PLINE *pl, void *user_data)
{
  DrawAPI *dapi = user_data;
  thindraw_contour (dapi, pl);
  return 0;
}

static void
common_thindraw_pcb_polygon (DrawAPI *dapi, LayerType *layer,
                             PolygonType *poly)
{
  thindraw_contour (dapi, poly->Clipped->contours);
  PolygonHoles (poly, dapi->clip_box, thindraw_hole_cb, dapi);
}

static void
thindraw_pcb_pad (DrawAPI *dapi, PadType *pad, Coord w)
{
  Coord x1, y1, x2, y2;
  Coord t = w / 2;
  x1 = pad->Point1.X;
  y1 = pad->Point1.Y;
  x2 = pad->Point2.X;
  y2 = pad->Point2.Y;
  if (x1 > x2 || y1 > y2)
    {
      Coord temp_x = x1;
      Coord temp_y = y1;
      x1 = x2; x2 = temp_x;
      y1 = y2; y2 = temp_y;
    }
  dapi->graphics->set_line_cap (dapi->fg_gc, Round_Cap);
  dapi->graphics->set_line_width (dapi->fg_gc, 0);
  if (TEST_FLAG (SQUAREFLAG, pad))
    {
      /* slanted square pad */
      double tx, ty, theta;

      if (x1 == x2 && y1 == y2)
        theta = 0;
      else
        theta = atan2 (y2 - y1, x2 - x1);

      /* T is a vector half a thickness long, in the direction of
         one of the corners.  */
      tx = t * cos (theta + M_PI / 4) * sqrt (2.0);
      ty = t * sin (theta + M_PI / 4) * sqrt (2.0);

      dapi->graphics->draw_line (dapi->fg_gc, x1 - tx, y1 - ty, x2 + ty, y2 - tx);
      dapi->graphics->draw_line (dapi->fg_gc, x2 + ty, y2 - tx, x2 + tx, y2 + ty);
      dapi->graphics->draw_line (dapi->fg_gc, x2 + tx, y2 + ty, x1 - ty, y1 + tx);
      dapi->graphics->draw_line (dapi->fg_gc, x1 - ty, y1 + tx, x1 - tx, y1 - ty);
    }
  else if (x1 == x2 && y1 == y2)
    {
      dapi->graphics->draw_arc (dapi->fg_gc, x1, y1, t, t, 0, 360);
    }
  else
    {
      /* Slanted round-end pads.  */
      Coord dx, dy, ox, oy;
      double h;

      dx = x2 - x1;
      dy = y2 - y1;
      h = t / sqrt (SQUARE (dx) + SQUARE (dy));
      ox = dy * h + 0.5 * SGN (dy);
      oy = -(dx * h + 0.5 * SGN (dx));

      dapi->graphics->draw_line (dapi->fg_gc, x1 + ox, y1 + oy, x2 + ox, y2 + oy);

      if (abs (ox) >= pixel_slop || abs (oy) >= pixel_slop)
        {
          Angle angle = atan2 (dx, dy) * 57.295779;
          dapi->graphics->draw_line (dapi->fg_gc, x1 - ox, y1 - oy, x2 - ox, y2 - oy);
          dapi->graphics->draw_arc (dapi->fg_gc, x1, y1, t, t, angle - 180, 180);
          dapi->graphics->draw_arc (dapi->fg_gc, x2, y2, t, t, angle, 180);
        }
    }
}

static void
common_thindraw_pcb_pad (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  thindraw_pcb_pad (dapi, pad, pad->Thickness);
}

static void
common_thindraw_pcb_pad_mask (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  if (pad->Mask > 0)
    thindraw_pcb_pad (dapi, pad, pad->Mask);
}

static void
common_thindraw_pcb_pad_paste (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  if (!TEST_FLAG (NOPASTEFLAG, pad) && pad->Mask > 0)
    thindraw_pcb_pad (dapi, pad, MIN (pad->Thickness, pad->Mask));
}

static void
fill_pcb_pad (DrawAPI *dapi, PadType *pad, Coord w)
{
  if (pad->Point1.X == pad->Point2.X &&
      pad->Point1.Y == pad->Point2.Y)
    {
      if (TEST_FLAG (SQUAREFLAG, pad))
        {
          Coord l, r, t, b;
          l = pad->Point1.X - w / 2;
          b = pad->Point1.Y - w / 2;
          r = l + w;
          t = b + w;
          dapi->graphics->fill_rect (dapi->fg_gc, l, b, r, t);
        }
      else
        {
          dapi->graphics->fill_circle (dapi->fg_gc, pad->Point1.X, pad->Point1.Y, w / 2);
        }
    }
  else
    {
      dapi->graphics->set_line_cap (dapi->fg_gc, TEST_FLAG (SQUAREFLAG, pad) ?
                                                                  Square_Cap : Round_Cap);
      dapi->graphics->set_line_width (dapi->fg_gc, w);

      dapi->graphics->draw_line (dapi->fg_gc, pad->Point1.X, pad->Point1.Y,
                                              pad->Point2.X, pad->Point2.Y);
    }
}

static void
common_fill_pcb_pad (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  fill_pcb_pad (dapi, pad, pad->Thickness);
}

static void
common_fill_pcb_pad_mask (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  if (pad->Mask > 0)
    fill_pcb_pad (dapi, pad, pad->Mask);
}

static void
common_fill_pcb_pad_paste (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  if (!TEST_FLAG (NOPASTEFLAG, pad) && pad->Mask > 0)
    fill_pcb_pad (dapi, pad, MIN (pad->Thickness, pad->Mask));
}


/* ---------------------------------------------------------------------------
 * draws one polygon
 * x and y are already in display coordinates
 * the points are numbered:
 *
 *          5 --- 6
 *         /       \
 *        4         7
 *        |         |
 *        3         0
 *         \       /
 *          2 --- 1
 */

typedef struct
{
  double X, Y;
}
FloatPolyType;

static void
draw_octagon_poly (DrawAPI *dapi, Coord X, Coord Y,
                   Coord Thickness, bool thin_draw)
{
  static FloatPolyType p[8] = {
    { 0.5,               -TAN_22_5_DEGREE_2},
    { TAN_22_5_DEGREE_2, -0.5              },
    {-TAN_22_5_DEGREE_2, -0.5              },
    {-0.5,               -TAN_22_5_DEGREE_2},
    {-0.5,                TAN_22_5_DEGREE_2},
    {-TAN_22_5_DEGREE_2,  0.5              },
    { TAN_22_5_DEGREE_2,  0.5              },
    { 0.5,                TAN_22_5_DEGREE_2}
  };
  static int special_size = 0;
  static int scaled_x[8];
  static int scaled_y[8];
  Coord polygon_x[9];
  Coord polygon_y[9];
  int i;

  if (Thickness != special_size)
    {
      special_size = Thickness;
      for (i = 0; i < 8; i++)
        {
          scaled_x[i] = p[i].X * special_size;
          scaled_y[i] = p[i].Y * special_size;
        }
    }
  /* add line offset */
  for (i = 0; i < 8; i++)
    {
      polygon_x[i] = X + scaled_x[i];
      polygon_y[i] = Y + scaled_y[i];
    }

  if (thin_draw)
    {
      int i;
      dapi->graphics->set_line_cap (dapi->fg_gc, Round_Cap);
      dapi->graphics->set_line_width (dapi->fg_gc, 0);
      polygon_x[8] = X + scaled_x[0];
      polygon_y[8] = Y + scaled_y[0];
      for (i = 0; i < 8; i++)
        dapi->graphics->draw_line (dapi->fg_gc, polygon_x[i    ], polygon_y[i    ],
                                                polygon_x[i + 1], polygon_y[i + 1]);
    }
  else
    dapi->graphics->fill_polygon (dapi->fg_gc, 8, polygon_x, polygon_y);
}

static void
fill_pcb_pv (DrawAPI *dapi, PinType *pv, Coord w)
{
  Coord r = w / 2;

  if (TEST_FLAG (SQUAREFLAG, pv))
    {
      Coord l = pv->X - r;
      Coord b = pv->Y - r;
      Coord r = l + w;
      Coord t = b + w;

      dapi->graphics->fill_rect (dapi->fg_gc, l, b, r, t);
    }
  else if (TEST_FLAG (OCTAGONFLAG, pv))
    draw_octagon_poly (dapi, pv->X, pv->Y, w, false);
  else /* draw a round pin or via */
    dapi->graphics->fill_circle (dapi->fg_gc, pv->X, pv->Y, r);
}

static void
common_fill_pcb_pv (DrawAPI *dapi, PinType *pv)
{
  if (!TEST_FLAG (HOLEFLAG, pv))
    fill_pcb_pv (dapi, pv, pv->Thickness);
}

static void
common_fill_pcb_pv_mask (DrawAPI *dapi, PinType *pv)
{
  if (pv->Mask > 0)
    fill_pcb_pv (dapi, pv, pv->Mask);
}

static void
common_fill_pcb_pv_hole (DrawAPI *dapi, PinType *pv)
{
  Coord r = pv->DrillingHole / 2;

  dapi->graphics->fill_circle (dapi->bg_gc, pv->X, pv->Y, r);

  /* Outline the hole if we are just drawing a hole */
  if (TEST_FLAG (HOLEFLAG, pv))
    {
      dapi->graphics->set_line_cap (dapi->fg_gc, Round_Cap);
      dapi->graphics->set_line_width (dapi->fg_gc, 0);
      dapi->graphics->draw_arc (dapi->fg_gc, pv->X, pv->Y, r, r, 0, 360);
    }
}

static void
thindraw_pcb_pv (DrawAPI *dapi, PinType *pv, Coord w)
{
  Coord r = w / 2;

  /* NB: This code-path allows a square or octagonal mask for a hole,
   *     whereas our pervious drawing routines would always force a
   *     round mask aperture for a hole.
   *     This new code should be ok, as the SQUARE or OCTAGONAL flags
   *     are highly unlikely to be set on a hole. (But can be now if
   *     the user desires).
   */
  if (TEST_FLAG (SQUAREFLAG, pv))
    {
      Coord l = pv->X - r;
      Coord b = pv->Y - r;
      Coord r = l + w;
      Coord t = b + w;

      dapi->graphics->set_line_cap (dapi->fg_gc, Round_Cap);
      dapi->graphics->set_line_width (dapi->fg_gc, 0);
      dapi->graphics->draw_line (dapi->fg_gc, r, t, r, b);
      dapi->graphics->draw_line (dapi->fg_gc, l, t, l, b);
      dapi->graphics->draw_line (dapi->fg_gc, r, t, l, t);
      dapi->graphics->draw_line (dapi->fg_gc, r, b, l, b);

    }
  else if (TEST_FLAG (OCTAGONFLAG, pv))
    {
      draw_octagon_poly (dapi, pv->X, pv->Y, w, true);
    }
  else /* draw a round pin or via */
    {
      dapi->graphics->set_line_cap (dapi->fg_gc, Round_Cap);
      dapi->graphics->set_line_width (dapi->fg_gc, 0);
      dapi->graphics->draw_arc (dapi->fg_gc, pv->X, pv->Y, r, r, 0, 360);
    }
}

static void
common_thindraw_pcb_pv (DrawAPI *dapi, PinType *pv)
{
  if (!TEST_FLAG (HOLEFLAG, pv))
    thindraw_pcb_pv (dapi, pv, pv->Thickness);
}

static void
common_thindraw_pcb_pv_mask (DrawAPI *dapi, PinType *pv)
{
  if (pv->Mask > 0)
    thindraw_pcb_pv (dapi, pv, pv->Mask);
}

static void
common_thindraw_pcb_pv_hole (DrawAPI *dapi, PinType *pv)
{
  Coord r = pv->DrillingHole / 2;

  dapi->graphics->set_line_cap (dapi->bg_gc, Round_Cap);
  dapi->graphics->set_line_width (dapi->bg_gc, 0);
  dapi->graphics->draw_arc (dapi->bg_gc, pv->X, pv->Y, r, r, 0, 360);
}

static void
common_draw_pcb_polygon (DrawAPI *dapi, LayerType *layer, PolygonType *poly)
{
  if (TEST_FLAG (THINDRAWFLAG,     PCB) ||
      TEST_FLAG (THINDRAWPOLYFLAG, PCB))
    common_thindraw_pcb_polygon (dapi, layer, poly);
  else
    common_fill_pcb_polygon (dapi, layer, poly);

}

static void
common_draw_pcb_pad (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    common_thindraw_pcb_pad (dapi, layer, pad);
  else
    common_fill_pcb_pad (dapi, layer, pad);
}

static void
common_draw_pcb_pad_mask (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  if (TEST_FLAG (THINDRAWFLAG,     PCB) ||
      TEST_FLAG (THINDRAWPOLYFLAG, PCB))
    common_thindraw_pcb_pad_mask (dapi, layer, pad);
  else
    common_fill_pcb_pad_mask (dapi, layer, pad);
}

static void
common_draw_pcb_pad_paste (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    common_thindraw_pcb_pad_paste (dapi, layer, pad);
  else
    common_fill_pcb_pad_paste (dapi, layer, pad);
}

static void
common_draw_pcb_pv (DrawAPI *dapi, PinType *pv)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    common_thindraw_pcb_pv (dapi, pv);
  else
    common_fill_pcb_pv (dapi, pv);
}

static void
common_draw_pcb_pv_mask (DrawAPI *dapi, PinType *pv)
{
  if (TEST_FLAG (THINDRAWFLAG,     PCB) ||
      TEST_FLAG (THINDRAWPOLYFLAG, PCB))
    common_thindraw_pcb_pv_mask (dapi, pv);
  else
    common_fill_pcb_pv_mask (dapi, pv);
}

static void
common_draw_pcb_pv_hole (DrawAPI *dapi, PinType *pv)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    common_thindraw_pcb_pv_hole (dapi, pv);
  else
    common_fill_pcb_pv_hole (dapi, pv);
}

void
common_draw_helpers_init (DrawAPI *dapi)
{
  dapi->draw_pcb_polygon   = common_draw_pcb_polygon;
  dapi->draw_pcb_pad       = common_draw_pcb_pad;
  dapi->draw_pcb_pad_mask  = common_draw_pcb_pad_mask;
  dapi->draw_pcb_pad_paste = common_draw_pcb_pad_paste;
  dapi->draw_pcb_pin       = common_draw_pcb_pv;
  dapi->draw_pcb_pin_mask  = common_draw_pcb_pv_mask;
  dapi->draw_pcb_pin_hole  = common_draw_pcb_pv_hole;
  dapi->draw_pcb_via       = common_draw_pcb_pv;
  dapi->draw_pcb_via_mask  = common_draw_pcb_pv_mask;
}

void
common_fill_helpers_init (DrawAPI *dapi)
{
  dapi->draw_pcb_polygon   = common_fill_pcb_polygon;
  dapi->draw_pcb_pad       = common_fill_pcb_pad;
  dapi->draw_pcb_pad_mask  = common_fill_pcb_pad_mask;
  dapi->draw_pcb_pad_paste = common_fill_pcb_pad_paste;
  dapi->draw_pcb_pin       = common_fill_pcb_pv;
  dapi->draw_pcb_pin_mask  = common_fill_pcb_pv_mask;
  dapi->draw_pcb_pin_hole  = common_fill_pcb_pv_hole;
  dapi->draw_pcb_via       = common_fill_pcb_pv;
  dapi->draw_pcb_via_mask  = common_fill_pcb_pv_mask;
}

void
common_thindraw_helpers_init (DrawAPI *dapi)
{
  dapi->draw_pcb_polygon   = common_thindraw_pcb_polygon;
  dapi->draw_pcb_pad       = common_thindraw_pcb_pad;
  dapi->draw_pcb_pad_mask  = common_thindraw_pcb_pad_mask;
  dapi->draw_pcb_pad_paste = common_thindraw_pcb_pad_paste;
  dapi->draw_pcb_pin       = common_thindraw_pcb_pv;
  dapi->draw_pcb_pin_mask  = common_thindraw_pcb_pv_mask;
  dapi->draw_pcb_pin_hole  = common_thindraw_pcb_pv_hole;
  dapi->draw_pcb_via       = common_thindraw_pcb_pv;
  dapi->draw_pcb_via_mask  = common_thindraw_pcb_pv_mask;
}
