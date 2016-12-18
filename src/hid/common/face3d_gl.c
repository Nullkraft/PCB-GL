#include <glib.h>
#include <stdbool.h>

#include "step_id.h"
#include "quad.h"
#include "contour3d.h"
#include "appearance.h"

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

static void
plane_uv_to_xyz (face3d *face, float u, float v, float *x, float *y, float *z)
{
  float ortx, orty, ortz;

  ortx = face->ny * face->rz - face->nz * face->ry;
  orty = face->nz * face->rx - face->nx * face->rz;
  ortz = face->nx * face->ry - face->ny * face->rx;

  *x = STEP_X_TO_COORD(PCB, u * face->rx + v * ortx);
  *y = STEP_Y_TO_COORD(PCB, u * face->ry + v * orty);
  *z = STEP_Z_TO_COORD(PCB, u * face->rz + v * ortz);
}

static void
ensure_tristrip (face3d *face)
{
  int num_uv_points;
  float *uv_points;
  int i;
  int vertex_comp = 0;

  /* Nothing to do if vertices are already cached */
  if (face->tristrip_vertices != NULL)
    return;

  num_uv_points = 3;
  uv_points = g_new0 (float, 2 * num_uv_points);

  uv_points[0] = 0.0f;
  uv_points[1] = 0.0f;

  uv_points[2] = 10.0f;
  uv_points[3] = 0.0f;

  uv_points[4] = 0.0f;
  uv_points[5] = 10.0f;

  face->tristrip_num_vertices = num_uv_points;
  face->tristrip_vertices = g_new0 (float, 3 * num_uv_points);

  for (i = 0; i < num_uv_points; i++)
    {
      plane_uv_to_xyz(face,
                      uv_points[2 * i + 0],
                      uv_points[2 * i + 1],
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

  glColor3f (0.0, 1.0, 1.0);

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
