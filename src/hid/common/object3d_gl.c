/* TODO ITEMS:
 *
 * Add PLINE simplification operation to consolidate co-circular segments for reduced geometry output.
 * Look at whether arc-* intersections can be re-constructed back to original geometry, not fall back to line-line.
 * Work on snap-rounding any edge which passes through the pixel square containing an any vertex (or intersection).
 * Avoid self-touching output in contours, where that self-touching instance creates two otherwise distinct contours or holes.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <glib.h>

#include "data.h"
#include "step_id.h"
#include "quad.h"
#include "vertex3d.h"
#include "contour3d.h"
#include "appearance.h"
#include "face3d.h"
#include "edge3d.h"
#include "object3d.h"
#include "polygon.h"
#include "rats.h"

#include "rtree.h"
#include "rotate.h"

#include "pcb-printf.h"
#include "misc.h"
#include "hid/hidint.h"

#define PERFECT_ROUND_CONTOURS
#define SUM_PINS_VIAS_ONCE
#define HASH_OBJECTS

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

#include "hid_draw.h"
#include "hidgl.h"
#include "face3d_gl.h"
#include "object3d_gl.h"

//static Coord board_thickness;
#define HACK_BOARD_THICKNESS board_thickness
//#define HACK_BOARD_THICKNESS MM_TO_COORD(1.6)
#define HACK_COPPER_THICKNESS MM_TO_COORD(0.035)
#define HACK_PLATED_BARREL_THICKNESS MM_TO_COORD(0.08)
#define HACK_MASK_THICKNESS MM_TO_COORD(0.01)
#define HACK_SILK_THICKNESS MM_TO_COORD(0.01)

static GList *object3d_test_objects = NULL;

void
object3d_test_init (void)
{
  object3d_test_objects = object3d_from_board_outline ();
}

float colors[12][3] = {{1., 0., 0.},
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


#define CIRC_SEGS_D 64.0


struct draw_info {
  hidGC gc;
  bool selected;
  bool debug_face;
};


static void
evaluate_bspline (edge_info *info, double u, double *x, double *y, double *z)
{
//  info->
}

static void
draw_bspline (edge_ref e)
{
  edge_info *info = UNDIR_DATA(e);
#if 0
  double x1, y1, z1;
  double x2, y2, z2;
#endif
  double lx, ly, lz;
  double x, y, z;
  int i;

#if 0
  x1 = ((vertex3d *)ODATA(e))->x;
  y1 = ((vertex3d *)ODATA(e))->y;
  z1 = ((vertex3d *)ODATA(e))->z;

  x2 = ((vertex3d *)DDATA(e))->x;
  y2 = ((vertex3d *)DDATA(e))->y;
  z2 = ((vertex3d *)DDATA(e))->z;
#endif

  glBegin (GL_LINES);

#if 0
  for (i = 0; i < 20; i++, lx = x, ly = y, lz = z) /* Pieces */
    {
      evaluate_bspline (edge_info, i / 20.0, &x, &y, &z);

      if (i > 0)
        {
          glVertex3f (STEP_X_TO_COORD (PCB, lx), STEP_Y_TO_COORD (PCB, ly), STEP_Z_TO_COORD (PCB, lz));
          glVertex3f (STEP_X_TO_COORD (PCB,  x), STEP_Y_TO_COORD (PCB,  y), STEP_Z_TO_COORD (PCB,  z));
        }
    }
#endif

  /* Just draw the control points for now... */
  for (i = 0; i < info->num_control_points; i++, lx = x, ly = y, lz = z) /* Pieces */
    {
      x = info->control_points[i * 3 + 0];
      y = info->control_points[i * 3 + 1];
      z = info->control_points[i * 3 + 2];

      if (i > 0)
        {
          glVertex3f (STEP_X_TO_COORD (PCB, lx), STEP_Y_TO_COORD (PCB, ly), STEP_Z_TO_COORD (PCB, lz));
          glVertex3f (STEP_X_TO_COORD (PCB,  x), STEP_Y_TO_COORD (PCB,  y), STEP_Z_TO_COORD (PCB,  z));
        }
    }

  glEnd ();
}

static void
draw_quad_edge (edge_ref e, void *data)
{
  struct draw_info *d_info = data;
  double x1, y1, z1;
  double x2, y2, z2;
  int i;
  bool debug = GPOINTER_TO_INT (data);

#if 0
  int id = ID(e) % 12;

  glColor3f (colors[id][0], colors[id][1], colors[id][2]);
#else
  if (d_info->selected)
    glColor4f (0.0, 1.0, 1., 0.5);
  else
    glColor4f (1., 1., 1., 0.3);
#endif

  x1 = ((vertex3d *)ODATA(e))->x;
  y1 = ((vertex3d *)ODATA(e))->y;
  z1 = ((vertex3d *)ODATA(e))->z;

  x2 = ((vertex3d *)DDATA(e))->x;
  y2 = ((vertex3d *)DDATA(e))->y;
  z2 = ((vertex3d *)DDATA(e))->z;

  if (UNDIR_DATA(e) != NULL)
    {
      edge_info *info = UNDIR_DATA(e);


//        if (info->is_placeholder)
        if (d_info->debug_face)
          {
            glColor4f (1.0, 0.0, 0.0, 1.0);
            glDepthMask (TRUE);
          }

//      if (info->is_stitch)
//        return;

      if (info->is_bspline)
        {
          draw_bspline (e);
          return;
        }

      if (info->is_round)
        {
          int i;
          double cx, cy, cz;
          double nx, ny, nz;
          double refx, refy, refz;
          double endx, endy, endz;
          double ortx, orty, ortz;
          double cosa;
          double sina;
          double recip_length;
          double da;
          int segs;
          double angle_step;

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
#if 0
              /* Option 1, just draw the forward copy, which agrees with the normal */
              x1 = ((vertex3d *)ODATA(SYM(e)))->x;
              y1 = ((vertex3d *)ODATA(SYM(e)))->y;
              z1 = ((vertex3d *)ODATA(SYM(e)))->z;

              x2 = ((vertex3d *)DDATA(SYM(e)))->x;
              y2 = ((vertex3d *)DDATA(SYM(e)))->y;
              z2 = ((vertex3d *)DDATA(SYM(e)))->z;
#else
              /* Option 2, flip the normal */
              nx = -nx;
              ny = -ny;
              nz = -nz;
#endif
            }

          /* STEP MAY ACTUALLY SPECIFY A DIFFERENT REF DIRECTION, BUT FOR NOW, LETS ASSUME IT POINTS
           * TOWARDS THE FIRST POINT. (We don't record the STEP ref direction in our data-structure at the moment).
           */
          refx = x1 - cx;
          refy = y1 - cy;
          refz = z1 - cz;

          /* Normalise refx */
          recip_length = 1. / hypot (hypot (refx, refy), refz);
          refx *= recip_length;
          refy *= recip_length;
          refz *= recip_length;

          endx = x2 - cx;
          endy = y2 - cy;
          endz = z2 - cz;

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

          if (x1 == x2 &&
              y1 == y2 &&
              z1 == z2)
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

#if 0
          printf ("(%f, %f, %f)  (%f, %f, %f)\n", x1, y1, z1, x2, y2, z2);
          printf ("ref (%f, %f, %f)\n", refx, refy, refz);
          printf ("end (%f, %f, %f)\n", endx, endy, endz);
          printf ("ort (%f, %f, %f)\n", ortx, orty, ortz);
          printf ("n (%f, %f, %f)\n", nx, ny, nz);
          printf ("cosa %f, sina %f\n", cosa, sina);
          printf ("Got an arc with angle %f\n", da * 180. / M_PI);
#endif

          /* Scale up ref and ort to the actual vector length */
          refx *= info->radius;
          refy *= info->radius;
          refz *= info->radius;

          ortx *= info->radius;
          orty *= info->radius;
          ortz *= info->radius;

          /* XXX: NEED TO COMPUTE WHICH SEGMENT OF THE CURVE TO ACTUALLY DRAW! */
          segs = CIRC_SEGS_D * da / (2.0 * M_PI);
          segs = MAX(segs, 1);
          angle_step = da / (double)segs;

          glBegin (GL_LINES);

          for (i = 0; i < segs; i++)
            {
              cosa = cos (i * angle_step);
              sina = sin (i * angle_step);
              glVertex3f (STEP_X_TO_COORD (PCB, info->cx + refx * cosa + ortx * sina),
                          STEP_Y_TO_COORD (PCB, info->cy + refy * cosa + orty * sina),
                          STEP_Z_TO_COORD (PCB, info->cz + refz * cosa + ortz * sina));

              cosa = cos ((i + 1) * angle_step);
              sina = sin ((i + 1) * angle_step);
              glVertex3f (STEP_X_TO_COORD (PCB, info->cx + refx * cosa + ortx * sina),
                          STEP_Y_TO_COORD (PCB, info->cy + refy * cosa + orty * sina),
                          STEP_Z_TO_COORD (PCB, info->cz + refz * cosa + ortz * sina));
            }

          glEnd ();

          glDepthMask (FALSE);
          return;
        }
    }

//  printf ("Drawing line (%f, %f, %f)-(%f, %f, %f)\n", x1, y1, z1, x2, y2, z2);
  glBegin (GL_LINES);
  glVertex3f (STEP_X_TO_COORD (PCB, x1),
              STEP_Y_TO_COORD (PCB, y1),
              STEP_X_TO_COORD (PCB, z1));
  glVertex3f (STEP_X_TO_COORD (PCB, x2),
              STEP_Y_TO_COORD (PCB, y2),
              STEP_X_TO_COORD (PCB, z2));
  glEnd ();
  glDepthMask (FALSE);
}

static void
draw_contour (contour3d *contour, void *data)
{
//  struct draw_info *info = data;
  edge_ref e;
  int edge_no = 0;

  e = contour->first_edge;

//  printf ("Drawing contour\n");

  do
    {
      edge_info *info = UNDIR_DATA(e);
//      printf ("Edge %i: %p (%i%s)\n", edge_no++, e, info->edge_identifier, ((e & 2) == 2) ? "R" : "");
      draw_quad_edge (e, data);

      /* Stop if e was the only edge in a face - which we re-trace */
      /* XXX: Probably only a development bug until we get the quad-edge links correct */
//      if (LNEXT(e) == SYM(e))
//        break;

      /* LNEXT should take us counter-clockwise around the face */
      /* LPREV should take us clockwise around the face */
    }
  while ((e = LNEXT(e)) != contour->first_edge);
}

static int face_no;

static void
draw_face (face3d *face, void *data)
{
  struct draw_info *info = data;

  face3d_fill (info->gc, face, info->selected);

  info->debug_face = (face_no == debug_integer);

//  return;

//  if (face->contours != NULL)
//      draw_contour (face->contours->data, NULL);
//  printf ("Drawing face\n");
  g_list_foreach (face->contours, (GFunc)draw_contour, info);

  face_no++;
}

void
object3d_draw (hidGC gc, object3d *object, bool selected)
{
  struct draw_info info;

//  hidglGC hidgl_gc = (hidglGC)gc;
//  hidgl_instance *hidgl = hidgl_gc->hidgl;
//  hidgl_priv *priv = hidgl->priv;

  g_return_if_fail (object->edges != NULL);

  info.gc = gc;
  info.selected = selected;

//  quad_enum ((edge_ref)object->edges->data, draw_quad_edge, NULL);
//  printf ("BEGIN DRAW...\n");
//  g_list_foreach (object->edges, (GFunc)draw_quad_edge, NULL);

//  printf ("\nDrawing object\n");

  face_no = 0;

  g_list_foreach (object->faces, (GFunc)draw_face, &info);

//  printf ("....ENDED\n");
}

static void
object3d_draw_debug_single (object3d *object, void *user_data)
{
  hidGC gc = user_data;

  object3d_draw (gc, object, false);
}

void
object3d_draw_debug (hidGC gc)
{
  g_list_foreach (object3d_test_objects, (GFunc)object3d_draw_debug_single, gc);
}
