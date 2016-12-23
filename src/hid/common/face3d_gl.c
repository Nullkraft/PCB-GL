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
  PLINE *dummy_contour;
  borast_traps_t traps;
  bool found_outer_contour = false;

  /* Nothing to do if vertices are already cached */
  if (face->tristrip_vertices != NULL)
    return;

  /* Don't waste time if we failed last time */
  if (face->triangulate_failed)
    return;

  if (face->is_cylindrical)
    return;

//  if (face->is_b_spline)
//    return;

  poly = poly_Create ();
  if (poly == NULL)
    return;

  /* Create a dummy outer contour (so we don't have to worry about the order below..
   * when we encounter the outer contour, we substitute this dummy one for it.
   */
  p_v[0] = 0;
  p_v[1] = 0;
  node = poly_CreateNode (p_v);
  dummy_contour = poly_NewContour (node);
  dummy_contour->Flags.orient = PLF_DIR;
  poly_InclContour (poly, dummy_contour);

  for (c_iter = face->contours; c_iter != NULL; c_iter = g_list_next (c_iter))
    {
      contour = c_iter->data;

      e = contour->first_edge;

      do
        {
          edge_info *info = UNDIR_DATA (e);
          float u, v;
          bool backwards_edge;

          /* XXX: Do this without breaking abstraction? */
          /* Detect SYM edges, reverse the circle normal */
          backwards_edge = ((e & 2) == 2);

          edge_ensure_linearised (e);

          for (i = 0; i < info->num_linearised_vertices - 1; i++)
            {
              int vertex_idx = i;

              if (backwards_edge)
                vertex_idx = info->num_linearised_vertices - 1 - i;

              plane_xyz_to_uv (face,
                               info->linearised_vertices[vertex_idx * 3 + 0],
                               info->linearised_vertices[vertex_idx * 3 + 1],
                               info->linearised_vertices[vertex_idx * 3 + 2],
                               &u, &v);

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
//      if (p_contour->Flags.orient != (hole ? PLF_INV : PLF_DIR))
//      poly_InvContour (p_contour);

      if (p_contour->Flags.orient == PLF_DIR)
        {
          PLINE *old_outer;

          /* Found the outer contour */
          if (found_outer_contour)
            {
              printf ("FOUND TWO OUTER CONTOURS FOR PLANAR FACE.. WILL END BADLY!\n");
#if 1
              face->triangulate_failed = true;
              return;
#endif
            }

          p_contour->next = poly->contours->next;
          old_outer = poly->contours;
          poly->contours = p_contour;

          found_outer_contour = true;
        }
      else
        {
          if (!poly_InclContour (poly, p_contour))
            {
              printf ("Contour dropped - oops!\n");
              poly_DelContour (&p_contour);
            }
        }
      p_contour = NULL;

      /* XXX: Assumption of outline first, holes second seems to be false! */
//      hole = true;
    }

  if (!found_outer_contour)
    {
      printf ("DID NOT FIND OUTER CONTOUR... BADNESS\n");
      face->triangulate_failed = true;
      return;
    }

  poly_DelContour (&dummy_contour);

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
      num_uv_points += 5 + 1; /* Three vertices + repeated start and end, extra repeat to sync backface culling */
    } else {
      num_uv_points += 6; /* Four vertices + repeated start and end */
    }
  }

  poly_Free (&poly);

  if (num_uv_points == 0) {
//    printf ("Strange, contour didn't tesselate\n");
    face->triangulate_failed = true;
    return;
  }

//  printf ("Tesselated with %i uv points\n", num_uv_points);

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
#if 1
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x4;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x3;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x3;  uv_points[vertex_comp++] = y_bot;
      /* NB: Repeated last virtex to separate from other tri-strip */
      uv_points[vertex_comp++] = x3;  uv_points[vertex_comp++] = y_bot;
      /* NB: Extra repeated vertex to keep backface culling in sync */
#endif
    } else if (x3 == x4) {
      /* NB: Repeated first virtex to separate from other tri-strip */
#if 1
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x3;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x2;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x2;  uv_points[vertex_comp++] = y_top;
      /* NB: Repeated last virtex to separate from other tri-strip */
      uv_points[vertex_comp++] = x2;  uv_points[vertex_comp++] = y_top;
      /* NB: Extra repeated vertex to keep backface culling in sync */
#endif
    } else {
      /* NB: Repeated first virtex to separate from other tri-strip */
      uv_points[vertex_comp++] = x3;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x3;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x2;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x4;  uv_points[vertex_comp++] = y_bot;
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
      uv_points[vertex_comp++] = x1;  uv_points[vertex_comp++] = y_top;
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

  g_free (uv_points);
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

  /* We only know how to deal with planar faces for now */
  if (!face->is_planar)
    return;

  hidgl_flush_triangles (hidgl);

  ensure_tristrip (face);

  glNormal3f (face->nx, -face->ny, face->nz); /* XXX: Note the -ny */

  if (face->is_debug)
    glColor4f (1.0f, 0.0f, 0.0f, 0.5f);
  else if (selected)
    glColor4f (0.0f, 1.0f, 1.0f, 0.5f);
  else
    glColor4f (0.8f, 0.8f, 0.8f, 0.5f);

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

  glDisable(GL_AUTO_NORMAL); /* Quick hack test */
}
