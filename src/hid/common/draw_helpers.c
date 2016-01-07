#include "global.h"
#include "hid.h"
#include "hid_draw.h"
#include "data.h" /* For global "PCB" variable */
#include "rotate.h" /* For RotateLineLowLevel() */
#include "polygon.h"
#include "draw_helpers.h"


/* Takes a copy of clip_box so we own it */
void
common_set_clip_box (HID_DRAW *hid_draw, BoxType *clip_box)
{
  free (hid_draw->clip_box);
  hid_draw->clip_box = NULL;

  if (clip_box == NULL)
    return;

  hid_draw->clip_box = malloc (sizeof (BoxType));
  *hid_draw->clip_box = *clip_box;
}

static void
common_draw_pcb_line (hidGC gc, LineType *line)
{
  hid_draw_set_line_cap (gc, Trace_Cap);
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    hid_draw_set_line_width (gc, 0);
  else
    hid_draw_set_line_width (gc, line->Thickness);

  hid_draw_line (gc, line->Point1.X, line->Point1.Y,
                     line->Point2.X, line->Point2.Y);
}

static void
common_draw_pcb_arc (hidGC gc, ArcType *arc)
{
  if (!arc->Thickness)
    return;

  if (TEST_FLAG (THINDRAWFLAG, PCB))
    hid_draw_set_line_width (gc, 0);
  else
    hid_draw_set_line_width (gc, arc->Thickness);
  hid_draw_set_line_cap (gc, Trace_Cap);

  hid_draw_arc (gc, arc->X, arc->Y, arc->Width, arc->Height, arc->StartAngle, arc->Delta);
}

/* ---------------------------------------------------------------------------
 * drawing routine for text objects
 */
static void
common_draw_pcb_text (hidGC gc, TextType *Text, Coord min_line_width)
{
  Coord x = 0;
  unsigned char *string = (unsigned char *) Text->TextString;
  Cardinal n;
  FontType *font = &PCB->Font;

  while (string && *string)
    {
      /* draw lines if symbol is valid and data is present */
      if (*string <= MAX_FONTPOSITION && font->Symbol[*string].Valid)
        {
          LineType *line = font->Symbol[*string].Line;
          LineType newline;

          for (n = font->Symbol[*string].LineN; n; n--, line++)
            {
              /* create one line, scale, move, rotate and swap it */
              newline = *line;
              newline.Point1.X = SCALE_TEXT (newline.Point1.X + x, Text->Scale);
              newline.Point1.Y = SCALE_TEXT (newline.Point1.Y, Text->Scale);
              newline.Point2.X = SCALE_TEXT (newline.Point2.X + x, Text->Scale);
              newline.Point2.Y = SCALE_TEXT (newline.Point2.Y, Text->Scale);
              newline.Thickness = SCALE_TEXT (newline.Thickness, Text->Scale / 2);
              if (newline.Thickness < min_line_width)
                newline.Thickness = min_line_width;

              RotateLineLowLevel (&newline, 0, 0, Text->Direction);

              /* the labels of SMD objects on the bottom
               * side haven't been swapped yet, only their offset
               */
              if (TEST_FLAG (ONSOLDERFLAG, Text))
                {
                  newline.Point1.X = SWAP_SIGN_X (newline.Point1.X);
                  newline.Point1.Y = SWAP_SIGN_Y (newline.Point1.Y);
                  newline.Point2.X = SWAP_SIGN_X (newline.Point2.X);
                  newline.Point2.Y = SWAP_SIGN_Y (newline.Point2.Y);
                }
              /* add offset and draw line */
              newline.Point1.X += Text->X;
              newline.Point1.Y += Text->Y;
              newline.Point2.X += Text->X;
              newline.Point2.Y += Text->Y;
              hid_draw_pcb_line (gc, &newline);
            }

          /* move on to next cursor position */
          x += (font->Symbol[*string].Width + font->Symbol[*string].Delta);
        }
      else
        {
          /* the default symbol is a filled box */
          BoxType defaultsymbol = PCB->Font.DefaultSymbol;
          Coord size = (defaultsymbol.X2 - defaultsymbol.X1) * 6 / 5;

          defaultsymbol.X1 = SCALE_TEXT (defaultsymbol.X1 + x, Text->Scale);
          defaultsymbol.Y1 = SCALE_TEXT (defaultsymbol.Y1, Text->Scale);
          defaultsymbol.X2 = SCALE_TEXT (defaultsymbol.X2 + x, Text->Scale);
          defaultsymbol.Y2 = SCALE_TEXT (defaultsymbol.Y2, Text->Scale);

          RotateBoxLowLevel (&defaultsymbol, 0, 0, Text->Direction);

          /* add offset and draw box */
          defaultsymbol.X1 += Text->X;
          defaultsymbol.Y1 += Text->Y;
          defaultsymbol.X2 += Text->X;
          defaultsymbol.Y2 += Text->Y;
          hid_draw_fill_rect (gc, defaultsymbol.X1, defaultsymbol.Y1,
                                  defaultsymbol.X2, defaultsymbol.Y2);

          /* move on to next cursor position */
          x += size;
        }
      string++;
    }
}

static void
fill_contour (hidGC gc, PLINE *pl)
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

  hid_draw_fill_polygon (gc, n, x, y);

  free (x);
  free (y);
}

static void
thindraw_contour (hidGC gc, PLINE *pl)
{
  VNODE *v;
  Coord last_x, last_y;
  Coord this_x, this_y;

  hid_draw_set_line_width (gc, 0);
  hid_draw_set_line_cap (gc, Round_Cap);

  /* If the contour is round, use an arc drawing routine. */
  if (pl->is_round)
    {
      hid_draw_arc (gc, pl->cx, pl->cy, pl->radius, pl->radius, 0, 360);
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

      hid_draw_line (gc, last_x, last_y, this_x, this_y);
      // hid_draw_fill_circle (gc, this_x, this_y, 30);

      last_x = this_x;
      last_y = this_y;
    }
  while ((v = v->next) != pl->head.next);
}

static void
fill_contour_cb (PLINE *pl, void *user_data)
{
  hidGC gc = (hidGC)user_data;
  PLINE *local_pl = pl;

  fill_contour (gc, pl);
  poly_FreeContours (&local_pl);
}

static void
fill_clipped_contour (hidGC gc, PLINE *pl)
{
  HID_DRAW *hid_draw = gc->hid_draw;
  PLINE *pl_copy;
  POLYAREA *clip_poly;
  POLYAREA *piece_poly;
  POLYAREA *clipped_pieces;
  POLYAREA *draw_piece;
  int x;

  clip_poly = RectPoly (hid_draw->clip_box->X1, hid_draw->clip_box->X2,
                        hid_draw->clip_box->Y1, hid_draw->clip_box->Y2);
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
      fill_contour (gc, draw_piece->contours);
    }
  while ((draw_piece = draw_piece->f) != clipped_pieces);
  poly_Free (&clipped_pieces);
}

/* If at least 50% of the bounding box of the polygon is on the screen,
 * lets compute the complete no-holes polygon.
 */
#define BOUNDS_INSIDE_CLIP_THRESHOLD 0.5
static int
should_compute_no_holes (hidGC gc, PolygonType *poly)
{
  HID_DRAW *hid_draw = gc->hid_draw;
  Coord x1, x2, y1, y2;
  double poly_bounding_area;
  double clipped_poly_area;

  /* If there is no passed clip box, compute the whole thing */
  if (hid_draw->clip_box == NULL)
    return 1;

  x1 = MAX (poly->BoundingBox.X1, hid_draw->clip_box->X1);
  x2 = MIN (poly->BoundingBox.X2, hid_draw->clip_box->X2);
  y1 = MAX (poly->BoundingBox.Y1, hid_draw->clip_box->Y1);
  y2 = MIN (poly->BoundingBox.Y2, hid_draw->clip_box->Y2);

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

void
common_gui_draw_pcb_polygon (hidGC gc, PolygonType *polygon)
{
  if (polygon->Clipped == NULL)
    return;

  if (TEST_FLAG (THINDRAWFLAG, PCB) || TEST_FLAG (THINDRAWPOLYFLAG, PCB))
    hid_draw__thin_pcb_polygon (gc, polygon);
  else
    hid_draw__fill_pcb_polygon (gc, polygon);

  /* If checking planes, thin-draw any pieces which have been clipped away */
  if (TEST_FLAG (CHECKPLANESFLAG, PCB) && !TEST_FLAG (FULLPOLYFLAG, polygon))
    {
      PolygonType poly = *polygon;

      for (poly.Clipped = polygon->Clipped->f;
           poly.Clipped != polygon->Clipped;
           poly.Clipped = poly.Clipped->f)
        hid_draw__thin_pcb_polygon (gc, &poly);
    }
}

void
common_fill_pcb_polygon (hidGC gc, PolygonType *poly)
{
  HID_DRAW *hid_draw = gc->hid_draw;

  if (poly->Clipped == NULL)
    return;

  if (!poly->NoHolesValid)
    {
      /* If enough of the polygon is on-screen, compute the entire
       * NoHoles version and cache it for later rendering, otherwise
       * just compute what we need to render now.
       */
      if (should_compute_no_holes (gc, poly))
        ComputeNoHoles (poly);
      else
        NoHolesPolygonDicer (poly, hid_draw->clip_box, fill_contour_cb, gc);
    }
  if (poly->NoHolesValid && poly->NoHoles)
    {
      PLINE *pl;

      for (pl = poly->NoHoles; pl != NULL; pl = pl->next)
        {
          if (hid_draw->clip_box == NULL)
            fill_contour (gc, pl);
          else
            fill_clipped_contour (gc, pl);
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
        NoHolesPolygonDicer (&p, hid_draw->clip_box, fill_contour_cb, gc);
    }
}

static int
thindraw_hole_cb (PLINE *pl, void *user_data)
{
  hidGC gc = (hidGC)user_data;
  thindraw_contour (gc, pl);
  return 0;
}

void
common_thindraw_pcb_polygon (hidGC gc, PolygonType *poly)
{
  HID_DRAW *hid_draw = gc->hid_draw;

  if (poly->Clipped == NULL)
    return;

  thindraw_contour (gc, poly->Clipped->contours);
  PolygonHoles (poly, hid_draw->clip_box, thindraw_hole_cb, gc);

  /* Draw other parts of the polygon if fullpoly flag is set */
  if (TEST_FLAG (FULLPOLYFLAG, poly))
    {
      PolygonType p = *poly;

      for (p.Clipped = poly->Clipped->f;
           p.Clipped != poly->Clipped;
           p.Clipped = p.Clipped->f)
        {
          thindraw_contour (gc, p.Clipped->contours);
          PolygonHoles (&p, hid_draw->clip_box, thindraw_hole_cb, gc);
        }
    }
}

void
common_thindraw_pcb_pad (hidGC gc, PadType *pad, bool clear, bool mask)
{
  Coord w = clear ? (mask ? pad->Mask
                          : pad->Thickness + pad->Clearance)
                  : pad->Thickness;
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
  hid_draw_set_line_cap (gc, Round_Cap);
  hid_draw_set_line_width (gc, 0);
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

      hid_draw_line (gc, x1 - tx, y1 - ty, x2 + ty, y2 - tx);
      hid_draw_line (gc, x2 + ty, y2 - tx, x2 + tx, y2 + ty);
      hid_draw_line (gc, x2 + tx, y2 + ty, x1 - ty, y1 + tx);
      hid_draw_line (gc, x1 - ty, y1 + tx, x1 - tx, y1 - ty);
    }
  else if (x1 == x2 && y1 == y2)
    {
      hid_draw_arc (gc, x1, y1, t, t, 0, 360);
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

      hid_draw_line (gc, x1 + ox, y1 + oy, x2 + ox, y2 + oy);

      if (abs (ox) >= pixel_slop || abs (oy) >= pixel_slop)
        {
          Angle angle = atan2 (dx, dy) * 57.295779;
          hid_draw_line (gc, x1 - ox, y1 - oy, x2 - ox, y2 - oy);
          hid_draw_arc (gc, x1, y1, t, t, angle - 180, 180);
          hid_draw_arc (gc, x2, y2, t, t, angle, 180);
        }
    }
}

/* Computes the coordinates of the corners of a squared pad.  */
static void
common_get_pad_polygon(Coord x[4], Coord y[4], const PadType *l, int w)
{
  Coord dX, dY;
  double dwx, dwy;

  dX = l->Point2.X - l->Point1.X;
  dY = l->Point2.Y - l->Point1.Y;

  if (dY == 0)
    {
      dwx = w / 2;
      dwy = 0;
    }
  else if (dX == 0)
    {
      dwx = 0;
      dwy = w / 2;
    }
  else
    {
      double r;

      r = sqrt (dX * (double) dX + dY * (double) dY) * 2;
      dwx = w / r * dX;
      dwy =  w / r * dY;
    }

  x[0] = l->Point1.X - dwx + dwy;
  y[0] = l->Point1.Y - dwy - dwx;

  x[1] = l->Point1.X - dwx - dwy;
  y[1] = l->Point1.Y - dwy + dwx;

  x[2] = l->Point2.X + dwx - dwy;
  y[2] = l->Point2.Y + dwy + dwx;

  x[3] = l->Point2.X + dwx + dwy;
  y[3] = l->Point2.Y + dwy - dwx;
}

void
common_gui_draw_pcb_pad (hidGC gc, PadType *pad, bool clip, bool mask)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    hid_draw__thin_pcb_pad (gc, pad, clip, mask);
  else
    hid_draw__fill_pcb_pad (gc, pad, clip, mask);
}

void
common_fill_pcb_pad (hidGC gc, PadType *pad, bool clear, bool mask)
{
  Coord w = clear ? (mask ? pad->Mask
                          : pad->Thickness + pad->Clearance)
                  : pad->Thickness;

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
          hid_draw_fill_rect (gc, l, b, r, t);
        }
      else
        {
          hid_draw_fill_circle (gc, pad->Point1.X, pad->Point1.Y, w / 2);
        }
    }
  else
    {
      hid_draw_set_line_cap (gc, TEST_FLAG (SQUAREFLAG, pad) ? Square_Cap : Round_Cap);
      hid_draw_set_line_width (gc, w);

      if (TEST_FLAG (SQUAREFLAG, pad))
        {
          Coord x[4], y[4];
          common_get_pad_polygon (x, y, pad, w);
          hid_fill_polygon (gc, 4, x, y);
        }
      else
        hid_draw_line (gc, pad->Point1.X, pad->Point1.Y,
                           pad->Point2.X, pad->Point2.Y);
    }
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
draw_octagon_poly (hidGC gc, Coord X, Coord Y,
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
      hid_draw_set_line_cap (gc, Round_Cap);
      hid_draw_set_line_width (gc, 0);
      polygon_x[8] = X + scaled_x[0];
      polygon_y[8] = Y + scaled_y[0];
      for (i = 0; i < 8; i++)
        hid_draw_line (gc, polygon_x[i    ], polygon_y[i    ],
                           polygon_x[i + 1], polygon_y[i + 1]);
    }
  else
    hid_draw_fill_polygon (gc, 8, polygon_x, polygon_y);
}

void
common_gui_draw_pcb_pv (hidGC gc, PinType *pv, bool mask)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    hid_draw__thin_pcb_pv (gc, pv, mask);
  else
    hid_draw__fill_pcb_pv (gc, pv, mask);
}

void
common_gui_draw_pcb_pv_hole (hidGC gc, PinType *pv)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    hid_draw__thin_pcb_pv_hole (gc, pv);
  else
    hid_draw__fill_pcb_pv_hole (gc, pv);
}

void
common_fill_pcb_pv (hidGC gc, PinType *pv, bool mask)
{
  Coord w = mask ? pv->Mask : pv->Thickness;
  Coord r = w / 2;

  if (TEST_FLAG (HOLEFLAG, pv))
    {
      if (mask)
        hid_draw_fill_circle (gc, pv->X, pv->Y, r);
      return;
    }

  if (TEST_FLAG (SQUAREFLAG, pv))
    {
      Coord l = pv->X - r;
      Coord b = pv->Y - r;
      Coord r = l + w;
      Coord t = b + w;

      hid_draw_fill_rect (gc, l, b, r, t);
    }
  else if (TEST_FLAG (OCTAGONFLAG, pv))
    draw_octagon_poly (gc, pv->X, pv->Y, w, false);
  else /* draw a round pin or via */
    hid_draw_fill_circle (gc, pv->X, pv->Y, r);
}

void
common_fill_pcb_pv_hole (hidGC gc, PinType *pv)
{
  Coord r = pv->DrillingHole / 2;

  /* XXX */ hidGC bg_gc = gc;
  /* XXX */ hidGC fg_gc = gc;
  /* XXX: Old code had an opportunity to outline the foreground of a pure hole after clearing away the background... we now cannot */

  hid_draw_fill_circle (gc, pv->X, pv->Y, r);

  if (TEST_FLAG (HOLEFLAG, pv))
    {
      g_return_if_fail (pv->Thickness == pv->DrillingHole);
      hid_draw_set_line_cap (fg_gc, Round_Cap);
      hid_draw_set_line_width (fg_gc, 0);
      hid_draw_arc (fg_gc, pv->X, pv->Y, r, r, 0, 360);
    }
}

void
common_thindraw_pcb_pv (hidGC gc, PinType *pv, bool mask)
{
  Coord w = mask ? pv->Mask : pv->Thickness;
  Coord r = w / 2;

  if (TEST_FLAG (HOLEFLAG, pv))
    {
      if (mask)
        /* XXX: NOT SETTING LINE CAP OR WIDTH!! */
	hid_draw_arc (gc, pv->X, pv->Y, r, r, 0, 360);
      return;
    }

  if (TEST_FLAG (SQUAREFLAG, pv))
    {
      Coord l = pv->X - r;
      Coord b = pv->Y - r;
      Coord r = l + w;
      Coord t = b + w;

      hid_draw_set_line_cap (gc, Round_Cap);
      hid_draw_set_line_width (gc, 0);
      hid_draw_line (gc, r, t, r, b);
      hid_draw_line (gc, l, t, l, b);
      hid_draw_line (gc, r, t, l, t);
      hid_draw_line (gc, r, b, l, b);

    }
  else if (TEST_FLAG (OCTAGONFLAG, pv))
    {
      draw_octagon_poly (gc, pv->X, pv->Y, w, true);
    }
  else /* draw a round pin or via */
    {
      hid_draw_set_line_cap (gc, Round_Cap);
      hid_draw_set_line_width (gc, 0);
      hid_draw_arc (gc, pv->X, pv->Y, r, r, 0, 360);
    }
}

void
common_thindraw_pcb_pv_hole (hidGC gc, PinType *pv)
{
  Coord r = pv->DrillingHole / 2;

  hid_draw_set_line_cap (gc, Round_Cap);
  hid_draw_set_line_width (gc, 0);
  hid_draw_arc (gc, pv->X, pv->Y, r, r, 0, 360);
}

void
common_draw_helpers_class_init (HID_DRAW_CLASS *klass)
{
  klass->draw_pcb_line         = common_draw_pcb_line;
  klass->draw_pcb_arc          = common_draw_pcb_arc;
  klass->draw_pcb_text         = common_draw_pcb_text;
  klass->draw_pcb_polygon      = common_fill_pcb_polygon; /* Default is the non-GUI case */
  klass->draw_pcb_pad          = common_fill_pcb_pad;     /* Default is the non-GUI case */
  klass->draw_pcb_pv           = common_fill_pcb_pv;      /* Default is the non-GUI case */

  klass->_fill_pcb_polygon     = common_fill_pcb_polygon;
  klass->_thindraw_pcb_polygon = common_thindraw_pcb_polygon;
  klass->_fill_pcb_pad         = common_fill_pcb_pad;
  klass->_thindraw_pcb_pad     = common_thindraw_pcb_pad;
  klass->_fill_pcb_pv          = common_fill_pcb_pv;
  klass->_thindraw_pcb_pv      = common_thindraw_pcb_pv;
  klass->_fill_pcb_pv_hole     = common_fill_pcb_pv_hole;
  klass->_thindraw_pcb_pv_hole = common_thindraw_pcb_pv_hole;
}

void
common_draw_helpers_init (HID_DRAW *hid_draw)
{
}
