#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include <glib.h>

#include "step_id.h"
#include "quad.h"
#include "vertex3d.h"
#include "contour3d.h"
#include "appearance.h"
#include "face3d.h"
#include "edge3d.h"
#include "object3d.h"
#include "polygon.h"
#include "data.h"

#include "pcb-printf.h"

#define REVERSED_PCB_CONTOURS 1 /* PCB Contours are reversed from the expected CCW for outer ordering - once the Y-coordinate flip is taken into account */

#define EPSILON 1e-5 /* XXX: Unknown  what this needs to be */

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


#define HACK_BOARD_THICKNESS MM_TO_COORD(1.6)

static GList *object3d_test_objects = NULL;

void
object3d_test_init (void)
{
  object3d_test_objects = object3d_from_board_outline ();
  object3d_test_board_outline ();
}

object3d *
make_object3d (char *name)
{
  static int object3d_count = 0;
  object3d *object;

  object = g_new0 (object3d, 1);
  object->id = object3d_count++;
  name = g_strdup (name);

  return object;
}

void
destroy_object3d (object3d *object)
{
  g_list_free_full (object->vertices, (GDestroyNotify)destroy_vertex3d);
  g_list_free_full (object->edges, (GDestroyNotify)destroy_edge);
  g_list_free_full (object->faces, (GDestroyNotify)destroy_face3d);
  g_free (object->name);
  g_free (object);
}

void
object3d_set_appearance (object3d *object, appearance *appear)
{
  object->appear = appear;
}

void
object3d_add_edge (object3d *object, edge_ref edge)
{
  object->edges = g_list_append (object->edges, (void *)edge);
}

void
object3d_add_vertex (object3d *object, vertex3d *vertex)
{
  object->vertices = g_list_append (object->vertices, vertex);
}

void
object3d_add_face (object3d *object, face3d *face)
{
  object->faces = g_list_append (object->faces, face);
}


double colors[12][3] = {{1., 0., 0.},
                       {1., 1., 0.},
                       {0., 1., 0.},
                       {0., 1., 1.},
                       {0.5, 0., 0.},
                       {0.5, 0.5, 0.},
                       {0., 0.5, 0.},
                       {0., 0.5, 0.5},
                       {1., 0.5, 0.5},
                       {1., 1., 0.5},
                       {0.5, 1., 0.5},
                       {0.5, 1., 1.}};


#define CIRC_SEGS 64

static void
draw_quad_edge (edge_ref e, void *data)
{
#if 0
  int id = ID(e) % 12;

  glColor3f (colors[id][0], colors[id][1], colors[id][2]);
#else
  glColor3f (1., 1., 1.);
#endif

  if (UNDIR_DATA(e) != NULL) {
    edge_info *info = UNDIR_DATA(e);
//    if (info->is_stitch)
//      return;
    if (info->is_round) {
      int i;
      glBegin (GL_LINES);
      for (i = 0; i < CIRC_SEGS; i++) {
        /* XXX: THIS ASSUMES THE CIRCLE LIES IN THE X-Y PLANE */
        glVertex3f (STEP_X_TO_COORD (PCB, info->cx + info->radius * cos (i * 2. * M_PI / (double)CIRC_SEGS)),
                    STEP_Y_TO_COORD (PCB, info->cy + info->radius * sin (i * 2. * M_PI / (double)CIRC_SEGS)),
                    STEP_Z_TO_COORD (PCB, info->cz));
        glVertex3f (STEP_X_TO_COORD (PCB, info->cx + info->radius * cos ((i + 1) * 2. * M_PI / (double)CIRC_SEGS)),
                    STEP_Y_TO_COORD (PCB, info->cy + info->radius * sin ((i + 1) * 2. * M_PI / (double)CIRC_SEGS)),
                    STEP_Z_TO_COORD (PCB, info->cz));
      }
      glEnd ();
      return;
    }
  }

  glBegin (GL_LINES);
  glVertex3f (STEP_X_TO_COORD (PCB, ((vertex3d *)ODATA(e))->x),
              STEP_Y_TO_COORD (PCB, ((vertex3d *)ODATA(e))->y),
              STEP_X_TO_COORD (PCB, ((vertex3d *)ODATA(e))->z));
  glVertex3f (STEP_X_TO_COORD (PCB, ((vertex3d *)DDATA(e))->x),
              STEP_Y_TO_COORD (PCB, ((vertex3d *)DDATA(e))->y),
              STEP_X_TO_COORD (PCB, ((vertex3d *)DDATA(e))->z));
  glEnd ();
}

static void
object3d_draw_debug_single (object3d *object, void *user_data)
{
  g_return_if_fail (object->edges != NULL);

//  quad_enum ((edge_ref)object->edges->data, draw_quad_edge, NULL);
  g_list_foreach (object->edges, (GFunc)draw_quad_edge, NULL);
}

void
object3d_draw_debug (void)
{
  g_list_foreach (object3d_test_objects, (GFunc)object3d_draw_debug_single, NULL);
}

/*********************************************************************************************************/

static int
get_contour_npoints (PLINE *contour)
{
  /* HACK FOR ROUND CONTOURS */
  if (contour->is_round)
    return 1;

  return contour->Count;
}

static void
get_contour_coord_n_in_step_mm (PLINE *contour, int n, double *x, double *y)
{
  VNODE *vertex = &contour->head;

  if (contour->is_round)
    {
      /* HACK SPECIAL CASE FOR ROUND CONTOURS */

      /* We define an arbitrary point on the contour. This is used, for example,
       * to define a coordinate system along the contour, and coincides with where
       * we add a straight edge down the side of an extruded cylindrical shape.
       */
      *x = COORD_TO_STEP_X (PCB, contour->cx - contour->radius);
      *y = COORD_TO_STEP_Y (PCB, contour->cy);

      return;
    }

  while (n > 0) {
    vertex = vertex->next; /* The VNODE structure is circularly linked, so wrapping is OK */
    n--;
  }

  *x = COORD_TO_STEP_X (PCB, vertex->point[0]);
  *y = COORD_TO_STEP_Y (PCB, vertex->point[1]);
}
