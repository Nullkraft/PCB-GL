#include <glib.h>
#include <stdbool.h>

#include "step_id.h"
#include "quad.h"
#include "edge3d.h"
#include "vertex3d.h"
#include "contour3d.h"
#include "appearance.h"

#include "data.h"
#include "hid.h"
#include "sweep.h"
#include "polygon.h"

#define REVERSED_PCB_CONTOURS 1 /* PCB Contours are reversed from the expected CCW for outer ordering - once the Y-coordinate flip is taken into account */
//#undef REVERSED_PCB_CONTOURS

#ifdef REVERSED_PCB_CONTOURS
#define COORD_TO_STEP_X(pcb, x) (COORD_TO_MM(                   (x)))
#define COORD_TO_STEP_Y(pcb, y) (COORD_TO_MM((pcb)->MaxHeight - (y)))
#define COORD_TO_STEP_Z(pcb, z) (COORD_TO_MM(                   (z)))

#define STEP_X_TO_COORD(pcb, x) (MM_TO_COORD((x)))
#define STEP_Y_TO_COORD(pcb, y) ((pcb)->MaxHeight - MM_TO_COORD((y)))
#define STEP_Z_TO_COORD(pcb, z) (MM_TO_COORD((z)))
#else
/* XXX: BROKEN UPSIDE DOWN OUTPUT */
#define COORD_TO_STEP_X(pcb, x) (COORD_TO_MM((x)))
#define COORD_TO_STEP_Y(pcb, y) (COORD_TO_MM((y)))
#define COORD_TO_STEP_Z(pcb, z) (COORD_TO_MM((z)))

#define STEP_X_TO_COORD(pcb, x) (MM_TO_COORD((x)))
#define STEP_Y_TO_COORD(pcb, y) (MM_TO_COORD((y)))
#define STEP_Z_TO_COORD(pcb, z) (MM_TO_COORD((z)))
#endif


#ifndef WIN32
/* The Linux OpenGL ABI 1.0 spec requires that we define
 * GL_GLEXT_PROTOTYPES before including gl.h or glx.h for extensions
 * in order to get prototypes:
 *   http://www.opengl.org/registry/ABI/
 */
#   define GL_GLEXT_PROTOTYPES 1
#endif

#ifdef HAVE_OPENGL_GL_H
#   include <OpenGL/gl.h>
#else
#   include <GL/gl.h>
#endif

#include "data.h"
#include "hid_draw.h"
#include "hidgl.h"

#include "face3d.h"
#include "face3d_gl.h"

//#define MEMCPY_VERTEX_DATA

#define CIRC_SEGS_D 64.0

static void
plane_xyz_to_uv (face3d *face, float x, float y, float z, float *u, float *v)
{
  double ortx, orty, ortz;

  ortx = face->ny * face->rz - face->nz * face->ry;
  orty = face->nz * face->rx - face->nx * face->rz;
  ortz = face->nx * face->ry - face->ny * face->rx;

  *u = (x - face->ox) * face->rx +
       (y - face->oy) * face->ry +
       (z - face->oz) * face->rz;

  *v = (x - face->ox) * ortx +
       (y - face->oy) * orty +
       (z - face->oz) * ortz;
}

static void
plane_uv_to_xyz (face3d *face, float u, float v, float *x, float *y, float *z)
{
  float ortx, orty, ortz;

  ortx = face->ny * face->rz - face->nz * face->ry;
  orty = face->nz * face->rx - face->nx * face->rz;
  ortz = face->nx * face->ry - face->ny * face->rx;

  *x = STEP_X_TO_COORD(PCB, face->ox + u * face->rx + v * ortx);
  *y = STEP_Y_TO_COORD(PCB, face->oy + u * face->ry + v * orty);
  *z = STEP_Z_TO_COORD(PCB, face->oz + u * face->rz + v * ortz);
}

static void
ensure_tristrip (face3d *face)
{
  GList *c_iter;
  int num_uv_points;
  float *uv_points;
  int i;
  int vertex_comp;
  contour3d *contour;
  edge_ref e;
  int x1, x2, x3, x4, y_top, y_bot;
  Vector p_v;
  VNODE *node;
  PLINE *p_contour = NULL;
  POLYAREA *poly;
  borast_traps_t traps;

  /* Nothing to do if vertices are already cached */
  if (face->tristrip_vertices != NULL)
    return;

  if (face->is_cylindrical)
    return;

//  if (face->is_b_spline)
//    return;

  /* Outer contour */ /* XXX: NOT ALWAYS IT WOULD SEEM! */
//  contour = &ace->contours->data;

  poly = poly_Create ();
  if (poly == NULL)
    return;

  for (c_iter = face->contours; c_iter != NULL; c_iter = g_list_next (c_iter))
    {
      contour = c_iter->data;
      bool hole = false; /* XXX ??? */

      e = contour->first_edge;

      do
        {
          edge_info *info = UNDIR_DATA (e);
          double ex, ey, ez;
          double x, y, z;
          float u, v;

          ex = ((vertex3d *)DDATA(e))->x;
          ey = ((vertex3d *)DDATA(e))->y;
          ez = ((vertex3d *)DDATA(e))->z;

          if (info->is_round)
            {
              int i;
              double sx, sy, sz;
              double cx, cy, cz;
              double nx, ny, nz;
              double refx, refy, refz;
              double endx, endy, endz;
              double ortx, orty, ortz;
              double cosa, sina;
              double recip_length;
              double da;
              int segs;
              double angle_step;

              sx = ((vertex3d *)ODATA(e))->x;
              sy = ((vertex3d *)ODATA(e))->y;
              sz = ((vertex3d *)ODATA(e))->z;

              cx = ((edge_info *)UNDIR_DATA(e))->cx;
              cy = ((edge_info *)UNDIR_DATA(e))->cy;
              cz = ((edge_info *)UNDIR_DATA(e))->cz;

              nx = ((edge_info *)UNDIR_DATA(e))->nx;
              ny = ((edge_info *)UNDIR_DATA(e))->ny;
              nz = ((edge_info *)UNDIR_DATA(e))->nz;

              /* XXX: Do this without breaking abstraction? */
              /* Detect SYM edges, reverse the circle normal */
              if ((e & 2) == 2)
                {
                  nx = -nx;
                  ny = -ny;
                  nz = -nz;
                }

              /* STEP MAY ACTUALLY SPECIFY A DIFFERENT REF DIRECTION, BUT FOR NOW, LETS ASSUME IT POINTS
               * TOWARDS THE FIRST POINT. (We don't record the STEP ref direction in our data-structure at the moment).
               */
              refx = sx - cx;
              refy = sy - cy;
              refz = sz - cz;

              /* Normalise refx */
              recip_length = 1. / hypot (hypot (refx, refy), refz);
              refx *= recip_length;
              refy *= recip_length;
              refz *= recip_length;

              endx = ex - cx;
              endy = ey - cy;
              endz = ez - cz;

              /* Normalise endx */
              recip_length = 1. / hypot (hypot (endx, endy), endz);
              endx *= recip_length;
              endy *= recip_length;
              endz *= recip_length;

              /* ref cross normal */
              /* ort will be orthogonal to normal and ref vector */
              ortx = ny * refz - nz * refy;
              orty = nz * refx - nx * refz;
              ortz = nx * refy - ny * refx;

              /* Cosine is dot product of ref (normalised) and end (normalised) */
              cosa = refx * endx + refy * endy + refz * endz; // cos (phi)
              /* Sine is dot product of ort (normalised) and end (normalised) */
              sina = ortx * endx + orty * endy + ortz * endz; // sin (phi) = cos (phi - 90)

              if (sx == ex &&
                  sy == ey &&
                  sz == ez)
                {
                  da = 2.0 * M_PI;
                }
              else
                {
                  /* Delta angled */
                  da = atan2 (sina, cosa);

                  if (da < 0.0)
                    da += 2.0 * M_PI;
                }

              /* Scale up ref and ort to the actual vector length */
              refx *= info->radius;
              refy *= info->radius;
              refz *= info->radius;

              ortx *= info->radius;
              orty *= info->radius;
              ortz *= info->radius;

              segs = CIRC_SEGS_D * da / (2.0 * M_PI);
              segs = MAX(segs, 1);
              angle_step = da / (double)segs;

              for (i = 0; i < segs; i++)
                {
                  cosa = cos ((i + 1) * angle_step);
                  sina = sin ((i + 1) * angle_step);
                  x = info->cx + refx * cosa + ortx * sina;
                  y = info->cy + refy * cosa + orty * sina;
                  z = info->cz + refz * cosa + ortz * sina;

                  plane_xyz_to_uv (face, x, y, z, &u, &v);

                  /* XXX: Arbitrary scaling from parameter space to coords, assuming parameter space approx mm (which is likely wrong */
                  p_v[0] = MM_TO_COORD (u);
                  p_v[1] = MM_TO_COORD (v);
                  node = poly_CreateNode (p_v);

                  if (p_contour == NULL)
                    {
                      if ((p_contour = poly_NewContour (node)) == NULL)
                        return;
                    }
                  else
                    {
                      poly_InclVertex (p_contour->head.prev, node);
                    }
                }
            }
          else
            {
              /* Straight line case */
              plane_xyz_to_uv (face, ex, ey, ez, &u, &v);

              /* XXX: Arbitrary scaling from parameter space to coords, assuming parameter space approx mm (which is likely wrong */
              p_v[0] = MM_TO_COORD (u);
              p_v[1] = MM_TO_COORD (v);
              node = poly_CreateNode (p_v);

              if (p_contour == NULL)
                {
                  if ((p_contour = poly_NewContour (node)) == NULL)
                    return;
                }
              else
                {
                  poly_InclVertex (p_contour->head.prev, node);
                }
            }
        }
      while ((e = LNEXT(e)) != contour->first_edge);

      poly_PreContour (p_contour, FALSE);

      /* make sure it is a positive contour (outer) or negative (hole) */
      if (p_contour->Flags.orient != (hole ? PLF_INV : PLF_DIR))
        poly_InvContour (p_contour);

      poly_InclContour (poly, p_contour);
      contour = NULL;

    }

  /* XXX: Need to tesselate the polygon */
  _borast_traps_init (&traps);
  bo_poly_to_traps_no_draw (poly, &traps);

  num_uv_points = 0;

  for (i = 0; i < traps.num_traps; i++) {
    y_top = traps.traps[i].top;
    y_bot = traps.traps[i].bottom;

    x1 = _line_compute_intersection_x_for_y (&traps.traps[i].left,  y_top);
    x2 = _line_compute_intersection_x_for_y (&traps.traps[i].right, y_top);
    x3 = _line_compute_intersection_x_for_y (&traps.traps[i].right, y_bot);
    x4 = _line_compute_intersection_x_for_y (&traps.traps[i].left,  y_bot);

    if ((x1 == x2) || (x3 == x4)) {
      num_uv_points += 5; /* Three vertices + repeated start and end */
    } else {
      num_uv_points += 6; /* Four vertices + repeated start and end */
    }
  }

  if (num_uv_points == 0) {
//    printf ("Strange, contour didn't tesselate\n");
    return;
  }

  printf ("Tesselated with %i uv points\n", num_uv_points);

  uv_points = g_new0 (float, 2 * num_uv_points);

  vertex_comp = 0;
  for (i = 0; i < traps.num_traps; i++) {
    y_top = traps.traps[i].top;
    y_bot = traps.traps[i].bottom;

    x1 = _line_compute_intersection_x_for_y (&traps.traps[i].left,  y_top);
    x2 = _line_compute_intersection_x_for_y (&traps.traps[i].right, y_top);
    x3 = _line_compute_intersection_x_for_y (&traps.traps[i].right, y_bot);
    x4 = _line_compute_intersection_x_for_y (&traps.traps[i].left,  y_bot);

    if (x1 == x2) {
      /* NB: Repeated first virtex to separate from other tri-strip */
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x3;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x4;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x4;  uv_points[vertex_comp++] = y_bot;
      /* NB: Repeated last virtex to separate from other tri-strip */
    } else if (x3 == x4) {
      /* NB: Repeated first virtex to separate from other tri-strip */
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x2;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x3;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x3;  uv_points[vertex_comp++] = y_bot;
      /* NB: Repeated last virtex to separate from other tri-strip */
    } else {
      /* NB: Repeated first virtex to separate from other tri-strip */
      uv_points[vertex_comp++] = x2;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x2;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x3;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x4;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x4;  uv_points[vertex_comp++] = y_bot;
      /* NB: Repeated last virtex to separate from other tri-strip */
    }
  }

  _borast_traps_fini (&traps);

  /* XXX: Would it be better to use the original vertices?
   *      Rather than converting to u-v coordinates and back.
   *      Probably at least need to use the u-v points to
   *      perform the triangulation.
   */

  face->tristrip_num_vertices = num_uv_points;
  face->tristrip_vertices = g_new0 (float, 3 * num_uv_points);

  vertex_comp = 0;
  for (i = 0; i < num_uv_points; i++)
    {
      plane_uv_to_xyz(face,
                      COORD_TO_MM (uv_points[2 * i + 0]), /* Inverse of arbitrary transformation above */
                      COORD_TO_MM (uv_points[2 * i + 1]), /* Inverse of arbitrary transformation above */
                      &face->tristrip_vertices[vertex_comp + 0],
                      &face->tristrip_vertices[vertex_comp + 1],
                      &face->tristrip_vertices[vertex_comp + 2]);
      vertex_comp += 3;
    }
}

void
face3d_fill(hidGC gc, face3d *face, bool selected)
{
  hidglGC hidgl_gc = (hidglGC)gc;
  hidgl_instance *hidgl = hidgl_gc->hidgl;
#ifdef MEMCPY_VERTEX_DATA
  hidgl_priv *priv = hidgl->priv;
#endif
  int i;
  int vertex_comp;

  if (selected)
    hidgl_flush_triangles (hidgl);

  ensure_tristrip (face);

//  glColor4f (1.0f, 0.0f, 0.0f, 0.3f);
  glColor4f (1.0f, 0.0f, 0.0f, 1.0f);

  hidgl_ensure_vertex_space (gc, face->tristrip_num_vertices);

#ifdef MEMCPY_VERTEX_DATA
  memcpy (&priv->buffer.triangle_array[priv->buffer.coord_comp_count],
          face->tristrip_vertices,
          sizeof (float) * 5 * face->tristrip_num_vertices);
  priv->buffer.coord_comp_count += 5 * face->tristrip_num_vertices;
  priv->buffer.vertex_count += face->tristrip_num_vertices;

#else
  vertex_comp = 0;
  for (i = 0; i < face->tristrip_num_vertices; i++) {
    float x, y, z;
    x = face->tristrip_vertices[vertex_comp++];
    y = face->tristrip_vertices[vertex_comp++];
    z = face->tristrip_vertices[vertex_comp++];
    hidgl_add_vertex_3D_tex (gc, x, y, z, 0.0, 0.0);
  }
#endif

  hidgl_flush_triangles (hidgl);
}
