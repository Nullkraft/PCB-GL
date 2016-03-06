#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include <glib.h>

#include "quad.h"
#include "vertex3d.h"
#include "contour3d.h"
#include "appearance.h"
#include "face3d.h"
#include "edge3d.h"
#include "object3d.h"
#include "polygon.h"
#include "data.h"



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

static object3d *object3d_test_object = NULL;

static void
print_edge_id (edge_ref e)
{
  printf ("ID %i.%i", ID(e), (unsigned int)e & 3u);
}

static void
debug_print_edge (edge_ref e, void *data)
{
  printf ("Edge ID %i.%i\n", ID(e), (int)e & 3u);

  printf ("Edge ONEXT is "); print_edge_id (ONEXT(e)); printf ("\n");
  printf ("Edge OPREV is "); print_edge_id (OPREV(e)); printf ("\n");
  printf ("Edge DNEXT is "); print_edge_id (DNEXT(e)); printf ("\n");
  printf ("Edge DPREV is "); print_edge_id (DPREV(e)); printf ("\n");
  printf ("Edge RNEXT is "); print_edge_id (RNEXT(e)); printf ("\n");
  printf ("Edge RPREV is "); print_edge_id (RPREV(e)); printf ("\n");
  printf ("Edge LNEXT is "); print_edge_id (LNEXT(e)); printf ("\n");
  printf ("Edge LPREV is "); print_edge_id (LPREV(e)); printf ("\n");
}

void
object3d_test_init (void)
{
  //object3d_test_object = object3d_create_test_cube ();
  object3d_test_object = object3d_from_board_outline ();
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

#define XOFFSET 50
#define YOFFSET 50
#define ZOFFSET 0
#define SCALE  10
object3d *
object3d_create_test_cube (void)
{
  object3d *object;
  vertex3d *cube_vertices[8];
  edge_ref cube_edges[12];
  face3d *faces[6];
  int i;

  object = make_object3d ("TEST CUBE");

  cube_vertices[0] = make_vertex3d (XOFFSET + SCALE * 0., YOFFSET + SCALE * 0., ZOFFSET + SCALE *  0.);
  cube_vertices[1] = make_vertex3d (XOFFSET + SCALE * 1., YOFFSET + SCALE * 0., ZOFFSET + SCALE *  0.);
  cube_vertices[2] = make_vertex3d (XOFFSET + SCALE * 1., YOFFSET + SCALE * 0., ZOFFSET + SCALE * -1.);
  cube_vertices[3] = make_vertex3d (XOFFSET + SCALE * 0., YOFFSET + SCALE * 0., ZOFFSET + SCALE * -1.);
  cube_vertices[4] = make_vertex3d (XOFFSET + SCALE * 0., YOFFSET + SCALE * 1., ZOFFSET + SCALE *  0.);
  cube_vertices[5] = make_vertex3d (XOFFSET + SCALE * 1., YOFFSET + SCALE * 1., ZOFFSET + SCALE *  0.);
  cube_vertices[6] = make_vertex3d (XOFFSET + SCALE * 1., YOFFSET + SCALE * 1., ZOFFSET + SCALE * -1.);
  cube_vertices[7] = make_vertex3d (XOFFSET + SCALE * 0., YOFFSET + SCALE * 1., ZOFFSET + SCALE * -1.);

  for (i = 0; i < 8; i++)
    object3d_add_vertex (object, cube_vertices[i]);

  for (i = 0; i < 12; i++)
    {
      cube_edges[i] = make_edge ();
      UNDIR_DATA (cube_edges[i]) = make_edge_info ();
      object3d_add_edge (object, cube_edges[i]);
    }

  for (i = 0; i < 6; i++)
    {
      faces[i] = make_face3d ();
      /* XXX: Face normal */
      /* XXX: Face contours */
      object3d_add_face (object, faces[i]);
    }

  for (i = 0; i < 4; i++)
    {
      int next_vertex = (i + 1) % 4;
      int prev_vertex = (i + 3) % 4;

      /* Assign bottom edge endpoints */
      ODATA (cube_edges[i]) = cube_vertices[i];
      DDATA (cube_edges[i]) = cube_vertices[next_vertex];

      /* Assign top edge endpoints */
      ODATA (cube_edges[4 + i]) = cube_vertices[4 + i];
      DDATA (cube_edges[4 + i]) = cube_vertices[4 + next_vertex];

      /* Assign side edge endpoints */
      ODATA (cube_edges[8 + i]) = cube_vertices[i];
      DDATA (cube_edges[8 + i]) = cube_vertices[4 + i];

      /* Link up edges orbiting around each bottom vertex */
      splice (cube_edges[i], cube_edges[8 + i]);
      splice (cube_edges[8 + i], SYM(cube_edges[prev_vertex]));

      /* Link up edges orbiting around each bottom top */
      splice (cube_edges[4 + i], SYM(cube_edges[4 + prev_vertex]));
      splice (SYM(cube_edges[4 + prev_vertex]), SYM(cube_edges[8 + i]));

    }

  quad_enum (cube_edges[0], debug_print_edge, NULL);


  return object;
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

  if (UNDIR_DATA(e) != NULL)
    {
      edge_info *info = UNDIR_DATA(e);
      if (info->is_stitch)
        return;
      if (info->is_round)
        {
          int i;
          glBegin (GL_LINES);
          for (i = 0; i < CIRC_SEGS; i++)
            {
              /* XXX: THIS ASSUMES THE CIRCLE LIES IN THE X-Y PLANE */
              glVertex3f (MM_TO_COORD (info->cx + info->radius * cos (i * 2. * M_PI / (double)CIRC_SEGS)),
                          MM_TO_COORD (info->cy + info->radius * sin (i * 2. * M_PI / (double)CIRC_SEGS)),
                          MM_TO_COORD (info->cz));
              glVertex3f (MM_TO_COORD (info->cx + info->radius * cos ((i + 1) * 2. * M_PI / (double)CIRC_SEGS)),
                          MM_TO_COORD (info->cy + info->radius * sin ((i + 1) * 2. * M_PI / (double)CIRC_SEGS)),
                          MM_TO_COORD (info->cz));
            }
          glEnd ();
          return;
        }
    }

  glBegin (GL_LINES);
  glVertex3f (MM_TO_COORD (((vertex3d *)ODATA(e))->x), MM_TO_COORD (((vertex3d *)ODATA(e))->y), MM_TO_COORD (((vertex3d *)ODATA(e))->z));
  glVertex3f (MM_TO_COORD (((vertex3d *)DDATA(e))->x), MM_TO_COORD (((vertex3d *)DDATA(e))->y), MM_TO_COORD (((vertex3d *)DDATA(e))->z));
  glEnd ();
}

void
object3d_draw_debug (void)
{
  g_return_if_fail (object3d_test_object->edges != NULL);

  quad_enum ((edge_ref)object3d_test_object->edges->data, draw_quad_edge, NULL);
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
get_contour_coord_n_in_mm (PLINE *contour, int n, double *x, double *y)
{
  VNODE *vertex = &contour->head;

  if (contour->is_round)
    {
      /* HACK SPECIAL CASE FOR ROUND CONTOURS */

      /* We define an arbitrary point on the contour. This is used, for example,
       * to define a coordinate system along the contour, and coincides with where
       * we add a straight edge down the side of an extruded cylindrical shape.
       */
      *x = COORD_TO_MM (contour->cx - contour->radius);
      *y = COORD_TO_MM (contour->cy); /* FIXME: PCB's coordinate system has y increasing downwards */

      return;
    }

  while (n > 0)
    {
      vertex = vertex->next; /* The VNODE structure is circularly linked, so wrapping is OK */
      n--;
    }

  *x = COORD_TO_MM (vertex->point[0]);
  *y = COORD_TO_MM (vertex->point[1]); /* FIXME: PCB's coordinate system has y increasing downwards */
}

void
object3d_export_to_step (object3d *object, const char *filename)
{
  FILE *f;
  time_t currenttime;
  struct tm utc;
  int next_step_identifier;
  int geometric_representation_context_identifier;
  int brep_identifier;
  int pcb_shell_identifier;
  int brep_style_identifier;
  GList *styled_item_identifiers = NULL;
  GList *styled_item_iter;
  GList *face_iter;
  GList *edge_iter;
  GList *vertex_iter;
  GList *contour_iter;

  f = fopen (filename, "w");
  if (f == NULL)
    {
      perror (filename);
      return;
    }

  currenttime = time (NULL);
  gmtime_r (&currenttime, &utc);

  fprintf (f, "ISO-10303-21;\n");
  fprintf (f, "HEADER;\n");
  fprintf (f, "FILE_DESCRIPTION (\n"
              "/* description */ ('STEP AP214 export of circuit board'),\n"
              "/* implementation level */ '1');\n");
  fprintf (f, "FILE_NAME (/* name */ '%s',\n"
              "/* time_stamp */ '%.4d-%.2d-%.2dT%.2d:%.2d:%.2d',\n"
              "/* author */ ( '' ),\n"
              "/* organisation */ ( '' ),\n"
              "/* preprocessor_version */ 'PCB STEP EXPORT',\n"
              "/* originating system */ '%s " VERSION "',\n"
              "/* authorisation */ '' );\n",
           filename,
           1900 + utc.tm_year, 1 + utc.tm_mon, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec,
           Progname);
  fprintf (f, "FILE_SCHEMA (( 'AUTOMOTIVE_DESIGN' ));\n");
  fprintf (f, "ENDSEC;\n");
  fprintf (f, "\n");
  fprintf (f, "DATA;\n");

  /* TEST */

  /* Setup the context of the "product" we are defining", and that it is a 'part' */

  fprintf (f, "#1 = APPLICATION_CONTEXT ( 'automotive_design' ) ;\n"
              "#2 = APPLICATION_PROTOCOL_DEFINITION ( 'draft international standard', 'automotive_design', 1998, #1 );\n"
              "#3 = PRODUCT_CONTEXT ( 'NONE', #1, 'mechanical' ) ;\n"
              "#4 = PRODUCT ('%s', '%s', '%s', (#3)) ;\n"
              "#5 = PRODUCT_RELATED_PRODUCT_CATEGORY ('part', $, (#4)) ;\n",
              "test_pcb_id", "test_pcb_name", "test_pcb_description");

  /* Setup the specific definition of the product we are defining */
  fprintf (f, "#6 = PRODUCT_DEFINITION_CONTEXT ( 'detailed design', #1, 'design' ) ;\n"
              "#7 = PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE ( 'ANY', '', #4, .NOT_KNOWN. ) ;\n"
              "#8 = PRODUCT_DEFINITION ( 'UNKNOWN', '', #7, #6 ) ;\n"
              "#9 = PRODUCT_DEFINITION_SHAPE ( 'NONE', 'NONE',  #8 ) ;\n");

  /* Need an anchor in 3D space to orient the shape */
  fprintf (f, "#10 =    CARTESIAN_POINT ( 'NONE',  ( 0.0, 0.0, 0.0 ) ) ;\n"
              "#11 =          DIRECTION ( 'NONE',  ( 0.0, 0.0, 1.0 ) ) ;\n"
              "#12 =          DIRECTION ( 'NONE',  ( 1.0, 0.0, 0.0 ) ) ;\n"
              "#13 = AXIS2_PLACEMENT_3D ( 'NONE', #10, #11, #12 ) ;\n");

  /* Grr.. more boilerplate - this time unit definitions */

  fprintf (f, "#14 = UNCERTAINTY_MEASURE_WITH_UNIT (LENGTH_MEASURE( 1.0E-005 ), #15, 'distance_accuracy_value', 'NONE');\n"
              "#15 =( LENGTH_UNIT ( ) NAMED_UNIT ( * ) SI_UNIT ( .MILLI., .METRE. ) );\n"
              "#16 =( NAMED_UNIT ( * ) PLANE_ANGLE_UNIT ( ) SI_UNIT ( $, .RADIAN. ) );\n"
              "#17 =( NAMED_UNIT ( * ) SI_UNIT ( $, .STERADIAN. ) SOLID_ANGLE_UNIT ( ) );\n"
              "#18 =( GEOMETRIC_REPRESENTATION_CONTEXT ( 3 ) GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT ( ( #14 ) ) GLOBAL_UNIT_ASSIGNED_CONTEXT ( ( #15, #16, #17 ) ) REPRESENTATION_CONTEXT ( 'NONE', 'WORKASPACE' ) );\n");

  fprintf (f, "#19 = ADVANCED_BREP_SHAPE_REPRESENTATION ( '%s', ( /* Manifold_solid_brep */ #21, #13 ), #18 ) ;\n"
              "#20 = SHAPE_DEFINITION_REPRESENTATION ( #9, #19 ) ;\n",
              "test_pcb_absr_name");

  geometric_representation_context_identifier = 18;

  /* Save a place for the brep identifier */
  next_step_identifier = 21;
  brep_identifier = next_step_identifier++;

  /* Body style */
  fprintf (f, "#22 = COLOUR_RGB ( '', %f, %f, %f ) ;\n", object->appear->r, object->appear->g, object->appear->b);
  fprintf (f, "#23 = FILL_AREA_STYLE_COLOUR ( '', #22 ) ;\n"
              "#24 = FILL_AREA_STYLE ('', ( #23 ) ) ;\n"
              "#25 = SURFACE_STYLE_FILL_AREA ( #24 ) ;\n"
              "#26 = SURFACE_SIDE_STYLE ('', ( #25 ) ) ;\n"
              "#27 = SURFACE_STYLE_USAGE ( .BOTH. , #26 ) ;\n"
              "#28 = PRESENTATION_STYLE_ASSIGNMENT ( ( #27 ) ) ;\n");
  fprintf (f, "#29 = STYLED_ITEM ( 'NONE', ( #28 ), #%i ) ;\n", brep_identifier);
  brep_style_identifier = 29;
  fprintf (f, "#30 = PRESENTATION_LAYER_ASSIGNMENT (  '1', 'Layer 1', ( #%i ) ) ;\n", brep_style_identifier);

  next_step_identifier = 31;
  styled_item_identifiers = g_list_append (styled_item_identifiers, GINT_TO_POINTER (brep_style_identifier));

#define FWD 1
#define REV 2
#define ORIENTED_EDGE_IDENTIFIER(e) (((edge_info *)UNDIR_DATA (e))->edge_identifier + ((e & 2) ? REV : FWD))

  /* Define ininite planes corresponding to every planar face, and cylindrical surfaces for every cylindrical face */

  for (face_iter = object->faces; face_iter != NULL; face_iter = g_list_next (face_iter))
    {
      face3d *face = face_iter->data;

      if (face->is_cylindrical)
        {
          /* CYLINDRICAL SURFACE NORMAL POINTS OUTWARDS AWAY FROM ITS AXIS.
           * BECAUSE OUR ROUND CONTOURS ARE (CURRENTLY) ALWAYS HOLES IN THE SOLID,
           * THIS MEANS THE CYLINDER NORMAL POINTS INTO THE OBJECT
           */
          fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i, #%i, #%i ) ; "
                      "#%i = CYLINDRICAL_SURFACE ( 'NONE', #%i, %f ) ;\n",
                   next_step_identifier,     /* A point on the axis of the cylinder */ face->cx, face->cy, face->cz,
                   next_step_identifier + 1, /* Direction of the cylindrical axis */   face->ax, face->ay, face->az,
                   next_step_identifier + 2, /* A normal to the axis direction */      face->nx, face->ny, face->nz,
                   next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2,
                   next_step_identifier + 4, next_step_identifier + 3, face->radius);

          face->surface_orientation_reversed = true;
          face->surface_identifier = next_step_identifier + 4;
          next_step_identifier = next_step_identifier + 5;
        }
      else
        {
          contour3d *outer_contour = face->contours->data;
          edge_ref first_edge = outer_contour->first_edge;

          double ox, oy, oz;
          double nx, ny, nz;
          double rx, ry, rz;

          /* Define 0,0 of the face coordinate system to arbitraily correspond to the
             origin vertex of the edge this contour links to in the quad edge structure.
           */
          ox = ((vertex3d *)ODATA (first_edge))->x;
          oy = ((vertex3d *)ODATA (first_edge))->y;
          oz = ((vertex3d *)ODATA (first_edge))->z;

          nx = face->nx;
          ny = face->ny;
          nz = face->nz;

          /* Define the reference x-axis of the face coordinate system to be along the
             edge this contour links to in the quad edge data structure.
           */

          rx = ((vertex3d *)DDATA (first_edge))->x - ox;
          ry = ((vertex3d *)DDATA (first_edge))->y - oy;
          rz = ((vertex3d *)DDATA (first_edge))->z - oz;

          fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i, #%i, #%i ) ; "
                      "#%i = PLANE ( 'NONE',  #%i ) ;\n",
                   next_step_identifier,     /* A point on the plane. Forms 0,0 of its parameterised coords. */ ox, oy, oz,
                   next_step_identifier + 1, /* An axis direction normal to the the face - Gives z-axis */      nx, ny, nz,
                   next_step_identifier + 2, /* Reference x-axis, orthogonal to z-axis above */                 rx, ry, rz,
                   next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2,
                   next_step_identifier + 4, next_step_identifier + 3);

          face->surface_orientation_reversed = false;
          face->surface_identifier = next_step_identifier + 4;
          next_step_identifier = next_step_identifier + 5;
        }
    }

  /* Define the infinite lines corresponding to every edge (either lines or circles)*/
  for (edge_iter = object->edges; edge_iter != NULL; edge_iter = g_list_next (edge_iter))
    {
      edge_ref edge = (edge_ref)edge_iter->data;
      edge_info *info = UNDIR_DATA (edge);

      if (info->is_round)
        {
          fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i,  #%i,  #%i ) ; "
                      "#%i = CIRCLE ( 'NONE', #%i, %f ) ;\n",
                   next_step_identifier,     /* Center of the circle   */ info->cx, info->cy, info->cz, // <--- Center of coordinate placement
                   next_step_identifier + 1, /* Normal of circle?      */ 0.0, 0.0, -1.0, // <--- Z-axis direction of placement             /* XXX: PULL FROM FACE DATA */
                   next_step_identifier + 2, /* ??????                 */ -1.0, 0.0, 0.0, // <--- Approximate X-axis direction of placement /* XXX: PULL FROM FACE DATA */
                   next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2,
                   next_step_identifier + 4, next_step_identifier + 3, info->radius);
          info->infinite_line_identifier = next_step_identifier + 4;
          next_step_identifier = next_step_identifier + 5;
        }
      else
        {
          double  x,  y,  z;
          double dx, dy, dz;

          x = ((vertex3d *)ODATA (edge))->x;
          y = ((vertex3d *)ODATA (edge))->y;
          z = ((vertex3d *)ODATA (edge))->z;

          dx = ((vertex3d *)DDATA (edge))->x - x;
          dy = ((vertex3d *)DDATA (edge))->y - y;
          dz = ((vertex3d *)DDATA (edge))->z - z;

          fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i = VECTOR ( 'NONE', #%i, 1000.0 ) ; "
                      "#%i = LINE ( 'NONE', #%i, #%i ) ;\n",
                   next_step_identifier,     /* A point on the line         */  x,  y,  z,
                   next_step_identifier + 1, /* A direction along the line  */ dx, dy, dz,
                   next_step_identifier + 2, next_step_identifier + 1,
                   next_step_identifier + 3, next_step_identifier, next_step_identifier + 2);
          info->infinite_line_identifier = next_step_identifier + 3;
          next_step_identifier = next_step_identifier + 4;
        }
    }

  /* Define the vertices */
  for (vertex_iter = object->vertices; vertex_iter != NULL; vertex_iter = g_list_next (vertex_iter))
    {
      vertex3d *vertex = vertex_iter->data;

      fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; ", next_step_identifier, vertex->x, vertex->y, vertex->z); /* Vertex coordinate  */ 
      fprintf (f, "#%i = VERTEX_POINT ( 'NONE', #%i ) ;\n",             next_step_identifier + 1, next_step_identifier);
      vertex->vertex_identifier = next_step_identifier + 1;
      next_step_identifier = next_step_identifier + 2;
    }

  /* Define the Edges */
  for (edge_iter = object->edges; edge_iter != NULL; edge_iter = g_list_next (edge_iter))
    {
      edge_ref edge = (edge_ref)edge_iter->data;
      edge_info *info = UNDIR_DATA (edge);

      int sv = ((vertex3d *)ODATA (edge))->vertex_identifier;
      int ev = ((vertex3d *)DDATA (edge))->vertex_identifier;

      fprintf (f, "#%i = EDGE_CURVE ( 'NONE', #%i, #%i, #%i, .T. ) ; ", next_step_identifier, sv, ev, info->infinite_line_identifier);
      fprintf (f, "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .T. ) ; ",    next_step_identifier + 1, next_step_identifier);
      fprintf (f, "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .F. ) ;\n",   next_step_identifier + 2, next_step_identifier);
      info->edge_identifier = next_step_identifier; /* Add 1 for same oriented, add 2 for back oriented */
      next_step_identifier = next_step_identifier + 3;
    }

  /* Define the faces */
  for (face_iter = object->faces; face_iter != NULL; face_iter = g_list_next (face_iter))
    {
      face3d *face = face_iter->data;
      bool outer_contour = true;

      for (contour_iter = face->contours;
           contour_iter != NULL;
           contour_iter = g_list_next (contour_iter), outer_contour = false)
        {
          contour3d *contour = contour_iter->data;
          edge_ref edge;

          /* XXX: FWD / BWD NEEDS TO BE FUDGED IN HERE PERHAPS? */
          fprintf (f, "#%i = EDGE_LOOP ( 'NONE', ", next_step_identifier);

          /* Emit the edges.. */
          fprintf (f, "(");
          for (edge = contour->first_edge;
               edge != LPREV (contour->first_edge);
               edge = LNEXT (edge))
            {
              fprintf (f, "#%i, ", ORIENTED_EDGE_IDENTIFIER(edge)); /* XXX: IS ORIENTATION GOING TO BE CORRECT?? */
            }
          fprintf (f, "#%i)", ORIENTED_EDGE_IDENTIFIER(edge)); /* XXX: IS ORIENTATION GOING TO BE CORRECT?? */
          fprintf (f, " ) ; ");

          fprintf (f, "#%i = FACE_%sBOUND ( 'NONE', #%i, .T. ) ;\n", next_step_identifier + 1, outer_contour ? "OUTER_" : "", next_step_identifier);
          contour->face_bound_identifier = next_step_identifier + 1;
          next_step_identifier = next_step_identifier + 2;
        }

      fprintf (f, "#%i = ADVANCED_FACE ( 'NONE', ", next_step_identifier);
      fprintf (f, "(");
      for (contour_iter = face->contours;
           contour_iter != NULL && g_list_next (contour_iter) != NULL;
           contour_iter = g_list_next (contour_iter))
        {
          fprintf (f, "#%i, ", ((contour3d *)contour_iter->data)->face_bound_identifier);
        }
      fprintf (f, "#%i)", ((contour3d *)contour_iter->data)->face_bound_identifier);
      fprintf (f, ", #%i, %s ) ;\n", face->surface_identifier, face->surface_orientation_reversed ? ".F." : ".T.");
      face->face_identifier = next_step_identifier;
      next_step_identifier = next_step_identifier + 1;

      if (face->appear != NULL)
        {
          /* Face styles */
          fprintf (f, "#%i = COLOUR_RGB ( '', %f, %f, %f ) ;\n",             next_step_identifier, face->appear->r, face->appear->g, face->appear->b);
          fprintf (f, "#%i = FILL_AREA_STYLE_COLOUR ( '', #%i ) ;\n",        next_step_identifier + 1, next_step_identifier);
          fprintf (f, "#%i = FILL_AREA_STYLE ('', ( #%i ) ) ;\n",            next_step_identifier + 2, next_step_identifier + 1);
          fprintf (f, "#%i = SURFACE_STYLE_FILL_AREA ( #%i ) ;\n",           next_step_identifier + 3, next_step_identifier + 2);
          fprintf (f, "#%i = SURFACE_SIDE_STYLE ('', ( #%i ) ) ;\n",         next_step_identifier + 4, next_step_identifier + 3);
          fprintf (f, "#%i = SURFACE_STYLE_USAGE ( .BOTH. , #%i ) ;\n",      next_step_identifier + 5, next_step_identifier + 4);
          fprintf (f, "#%i = PRESENTATION_STYLE_ASSIGNMENT ( ( #%i ) ) ;\n", next_step_identifier + 6, next_step_identifier + 5);
          fprintf (f, "#%i = OVER_RIDING_STYLED_ITEM ( 'NONE', ( #%i ), #%i, #%i ) ;\n",
                   next_step_identifier + 7, next_step_identifier + 6, face->face_identifier, brep_style_identifier);
          styled_item_identifiers = g_list_append (styled_item_identifiers, GINT_TO_POINTER (next_step_identifier + 7));
          next_step_identifier = next_step_identifier + 8;
        }
    }

  /* Closed shell which bounds the brep solid */
  pcb_shell_identifier = next_step_identifier;
  next_step_identifier++;
  fprintf (f, "#%i = CLOSED_SHELL ( 'NONE', ", pcb_shell_identifier);
  /* Emit the faces.. */
  fprintf (f, "(");
  for (face_iter = object->faces;
       face_iter != NULL && g_list_next (face_iter) != NULL;
       face_iter = g_list_next (face_iter))
    {
      fprintf (f, "#%i, ", ((face3d *)face_iter->data)->face_identifier);
    }
  fprintf (f, "#%i)", ((face3d *)face_iter->data)->face_identifier);
  fprintf (f, " ) ;\n");

  /* Finally emit the brep solid definition */
  fprintf (f, "#%i = MANIFOLD_SOLID_BREP ( 'PCB outline', #%i ) ;\n", brep_identifier, pcb_shell_identifier);

  /* Emit references to the styled and over_ridden styled items */
  fprintf (f, "#%i = MECHANICAL_DESIGN_GEOMETRIC_PRESENTATION_REPRESENTATION (  '', ", next_step_identifier);
  fprintf (f, "(");
  for (styled_item_iter = styled_item_identifiers;
       styled_item_iter != NULL && g_list_next (styled_item_iter) != NULL;
       styled_item_iter = g_list_next (styled_item_iter))
    {
      fprintf (f, "#%i, ", GPOINTER_TO_INT (styled_item_iter->data));
    }
  fprintf (f, "#%i)", GPOINTER_TO_INT (styled_item_iter->data));
  fprintf (f, ", #%i ) ;\n", geometric_representation_context_identifier);
  next_step_identifier = next_step_identifier + 1;


#undef FWD
#undef REV
#undef ORIENTED_EDGE_IDENTIFIER

  fprintf (f, "ENDSEC;\n" );
  fprintf (f, "END-ISO-10303-21;\n" );

  fclose (f);
}

object3d *
object3d_from_board_outline (void)
{
  object3d *object;
  appearance *board_appearance;
  appearance *top_bot_appearance;
  POLYAREA *outline;
  PLINE *contour;
  PLINE *ct;
  int ncontours;
  int npoints;
  int i;
  vertex3d **vertices;
  edge_ref *edges;
  face3d **faces;
  int start_of_ct;
  int offset_in_ct;
  int ct_npoints;

  outline = board_outline_poly (true);
  //outline = board_outline_poly (false); /* (FOR NOW - just the outline, no holes) */
  ncontours = 0;
  npoints = 0;

  /* XXX: There can be more than one contour, but for now we restrict ourselves to the first one */
  contour = outline->contours;

  ct = contour;
  while (ct != NULL)
    {
      ncontours ++;
      npoints += get_contour_npoints (ct);
      ct = ct->next;
    }

  /* We know how many edges and vertices we need now...
   *
   * let n = npoints
   * bodies = 1             (FOR NOW - just the first board outline)
   * vertices = 2n          (n-top, n-bottom)
   * edges = 3n             (n-top, n-bottom, n-sides)
   * faces = 2 + n          (1-top, 1-bottom, n-sides)
   *
   * holes = 0              (FOR NOW - just the outline, no holes)
   * holes = ncontours - 1  (LATER)
   */

  object = make_object3d (PCB->Name);
  board_appearance = make_appearance ();
  top_bot_appearance = make_appearance ();
  appearance_set_color (board_appearance,   1.0, 1.0, 0.6);
  appearance_set_color (top_bot_appearance, 0.2, 0.8, 0.2);

  object3d_set_appearance (object, board_appearance);

  vertices = malloc (sizeof (vertex3d *) * 2 * npoints);
  edges    = malloc (sizeof (edge_ref  ) * 3 * npoints);
  faces    = malloc (sizeof (face3d *) * (2 + npoints));

  /* Define the vertices */
  ct = contour;
  start_of_ct = 0;
  offset_in_ct = 0;
  ct_npoints = get_contour_npoints (ct);

  for (i = 0; i < npoints; i++, offset_in_ct++)
    {
      double x1, y1;

      /* Update which contour we're looking at */
      if (offset_in_ct == ct_npoints)
        {
          offset_in_ct = 0;
          ct = ct->next;
          ct_npoints = get_contour_npoints (ct);
        }

      get_contour_coord_n_in_mm (ct, offset_in_ct, &x1, &y1);
      vertices[i]           = make_vertex3d (x1, y1, -COORD_TO_MM (HACK_BOARD_THICKNESS)); /* Bottom */
      vertices[npoints + i] = make_vertex3d (x1, y1, 0);                                   /* Top */

      object3d_add_vertex (object, vertices[i]);
      object3d_add_vertex (object, vertices[npoints + i]);
    }

  /* Define the edges */
  for (i = 0; i < 3 * npoints; i++)
    {
      edges[i] = make_edge ();
      UNDIR_DATA (edges[i]) = make_edge_info ();
      object3d_add_edge (object, edges[i]);
    }

  /* Define the faces */
  for (i = 0; i < npoints; i++)
    {
      faces[i] = make_face3d ();

      object3d_add_face (object, faces[i]);
      /* Pick one of the upright edges which is within this face outer contour loop, and link it to the face */
      face3d_add_contour (faces[i], make_contour3d (SYM(edges[2 * npoints + i])));
    }

  faces[npoints] = make_face3d (); /* bottom_face */
  faces[npoints]->nx =  0.;
  faces[npoints]->ny =  0.;
  faces[npoints]->nz = -1.;
  face3d_set_appearance (faces[npoints], top_bot_appearance);
  object3d_add_face (object, faces[npoints]);

  faces[npoints + 1] = make_face3d (); /* top_face */
  faces[npoints + 1]->nx = 0.;
  faces[npoints + 1]->ny = 0.;
  faces[npoints + 1]->nz = 1.;
  face3d_set_appearance (faces[npoints + 1], top_bot_appearance);
  object3d_add_face (object, faces[npoints + 1]);

  /* Pick the first bottom / top edge within the bottom / top face outer contour loop, and link it to the face */
  face3d_add_contour (faces[npoints], make_contour3d (SYM(edges[0])));
  face3d_add_contour (faces[npoints + 1], make_contour3d (edges[npoints]));

  ct = contour;
  start_of_ct = 0;
  offset_in_ct = 0;
  ct_npoints = get_contour_npoints (ct);

  for (i = 0; i < npoints; i++, offset_in_ct++)
  {
    int next_i_around_ct;
    int prev_i_around_ct;

    /* Update which contour we're looking at */
    if (offset_in_ct == ct_npoints)
      {
        start_of_ct = i;
        offset_in_ct = 0;
        ct = ct->next;
        ct_npoints = get_contour_npoints (ct);

        /* If there is more than one contour, it will be an inner contour of the bottom and top faces. Refer to it here */
        face3d_add_contour (faces[npoints], make_contour3d (SYM(edges[i])));
        face3d_add_contour (faces[npoints + 1], make_contour3d (edges[npoints + i]));
      }

    next_i_around_ct = start_of_ct + (offset_in_ct + 1) % ct_npoints;
    prev_i_around_ct = start_of_ct + (offset_in_ct + ct_npoints - 1) % ct_npoints;

    /* Setup the face normals for the edges along the contour extrusion (top and bottom are handled separaetely) */
    /* Define the (non-normalized) face normal to point to the outside of the contour */
    faces[i]->nx = vertices[next_i_around_ct]->y - vertices[i]->y;
    faces[i]->ny = vertices[i]->x - vertices[next_i_around_ct]->x;
    faces[i]->nz = 0.;

    /* Assign the appropriate vertex geometric data to each edge end */
    ODATA (edges[              i]) = vertices[0 * npoints + i];
    DDATA (edges[              i]) = vertices[0 * npoints + next_i_around_ct];
    ODATA (edges[1 * npoints + i]) = vertices[1 * npoints + i];
    DDATA (edges[1 * npoints + i]) = vertices[1 * npoints + next_i_around_ct];
    ODATA (edges[2 * npoints + i]) = vertices[0 * npoints + i];
    DDATA (edges[2 * npoints + i]) = vertices[1 * npoints + i];
    LDATA (edges[              i]) = faces[i];
    RDATA (edges[              i]) = faces[npoints];
    LDATA (edges[1 * npoints + i]) = faces[npoints + 1];
    RDATA (edges[1 * npoints + i]) = faces[i];
    LDATA (edges[2 * npoints + i]) = faces[prev_i_around_ct];
    RDATA (edges[2 * npoints + i]) = faces[i];

    /* NB: Contours are counter clockwise in XY plane.
     *     edges[          0-npoints-1] are the base of the extrusion, following in the counter clockwise order
     *     edges[1*npoints-2*npoints-1] are the top  of the extrusion, following in the counter clockwise order
     *     edges[2*npoints-3*npoints-1] are the upright edges, oriented from bottom to top
     */

    /* Link edges orbiting around each bottom vertex i (0 <= i < npoints) */
    splice (edges[i], edges[2 * npoints + i]);
    splice (edges[2 * npoints + i], SYM(edges[prev_i_around_ct]));

    /* Link edges orbiting around each top vertex (npoints + i) (0 <= i < npoints) */
    splice (SYM(edges[2 * npoints + i]), edges[npoints + i]);
    splice (edges[npoints + i], SYM(edges[npoints + prev_i_around_ct]));

    if (ct->is_round)
      {

        face3d_set_cylindrical (faces[i], COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), 0., /* A point on the axis of the cylinder */
                                          0., 0., 1.,                                     /* Direction of the cylindrical axis */
                                          COORD_TO_MM (ct->radius));
        face3d_set_normal (faces[i], 1., 0., 0.);  /* A normal to the axis direction */
                                  /* XXX: ^^^ Could line this up with the direction to the vertex in the corresponding circle edge */


        edge_info_set_round (UNDIR_DATA (edges[i]),
                             COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), -COORD_TO_MM (HACK_BOARD_THICKNESS), /* Center of circle */
                             0., 0., 1., /* Normal */ COORD_TO_MM (ct->radius));
        edge_info_set_round (UNDIR_DATA (edges[npoints + i]),
                             COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), 0., /* Center of circle */
                             0., 0., -1., /* Normal */ COORD_TO_MM (ct->radius));
        edge_info_set_stitch (UNDIR_DATA (edges[2 * npoints + i]));
      }

  }

  poly_Free (&outline);

  return object;
}

void
object3d_test_board_outline (void)
{
  object3d *board_outline;

  board_outline = object3d_from_board_outline ();
  object3d_export_to_step (board_outline, "object3d_test.step");
  destroy_object3d (board_outline);
}

object3d *
object3d_from_tracking (void)
{
  object3d *object;
  appearance *board_appearance;
  appearance *top_bot_appearance;
  POLYAREA *outline;
  PLINE *contour;
  PLINE *ct;
  int ncontours;
  int npoints;
  int i;
  vertex3d **vertices;
  edge_ref *edges;
  face3d **faces;
  int start_of_ct;
  int offset_in_ct;
  int ct_npoints;

  outline = board_outline_poly (true);
  ncontours = 0;
  npoints = 0;

  /* XXX: There can be more than one contour, but for now we restrict ourselves to the first one */
  contour = outline->contours;

  ct = contour;
  while (ct != NULL)
    {
      ncontours ++;
      npoints += get_contour_npoints (ct);
      ct = ct->next;
    }

  object = make_object3d (PCB->Name);
  board_appearance = make_appearance ();
  top_bot_appearance = make_appearance ();
  appearance_set_color (board_appearance,   1.0, 1.0, 0.6);
  appearance_set_color (top_bot_appearance, 0.2, 0.8, 0.2);

  object3d_set_appearance (object, board_appearance);

  vertices = malloc (sizeof (vertex3d *) * 2 * npoints);
  edges    = malloc (sizeof (edge_ref  ) * 3 * npoints);
  faces    = malloc (sizeof (face3d *) * (2 + npoints));

  /* Define the vertices */
  ct = contour;
  start_of_ct = 0;
  offset_in_ct = 0;
  ct_npoints = get_contour_npoints (ct);

  for (i = 0; i < npoints; i++, offset_in_ct++)
    {
      double x1, y1;

      /* Update which contour we're looking at */
      if (offset_in_ct == ct_npoints)
        {
          offset_in_ct = 0;
          ct = ct->next;
          ct_npoints = get_contour_npoints (ct);
        }

      get_contour_coord_n_in_mm (ct, offset_in_ct, &x1, &y1);
      vertices[i]           = make_vertex3d (x1, y1, -COORD_TO_MM (HACK_BOARD_THICKNESS)); /* Bottom */
      vertices[npoints + i] = make_vertex3d (x1, y1, 0);                                   /* Top */

      object3d_add_vertex (object, vertices[i]);
      object3d_add_vertex (object, vertices[npoints + i]);
    }

  /* Define the edges */
  for (i = 0; i < 3 * npoints; i++)
    {
      edges[i] = make_edge ();
      UNDIR_DATA (edges[i]) = make_edge_info ();
      object3d_add_edge (object, edges[i]);
    }

  /* Define the faces */
  for (i = 0; i < npoints; i++)
    {
      faces[i] = make_face3d ();

      object3d_add_face (object, faces[i]);
      /* Pick one of the upright edges which is within this face outer contour loop, and link it to the face */
      face3d_add_contour (faces[i], make_contour3d (SYM(edges[2 * npoints + i])));
    }

  faces[npoints] = make_face3d (); /* bottom_face */
  face3d_set_normal (faces[npoints], 0., 0., -1.);
  face3d_set_appearance (faces[npoints], top_bot_appearance);
  object3d_add_face (object, faces[npoints]);

  faces[npoints + 1] = make_face3d (); /* top_face */
  face3d_set_normal (faces[npoints + 1], 0., 0., 1.);
  face3d_set_appearance (faces[npoints + 1], top_bot_appearance);
  object3d_add_face (object, faces[npoints + 1]);

  /* Pick the first bottom / top edge within the bottom / top face outer contour loop, and link it to the face */
  face3d_add_contour (faces[npoints], make_contour3d (SYM(edges[0])));
  face3d_add_contour (faces[npoints + 1], make_contour3d (edges[npoints]));

  ct = contour;
  start_of_ct = 0;
  offset_in_ct = 0;
  ct_npoints = get_contour_npoints (ct);

  for (i = 0; i < npoints; i++, offset_in_ct++)
    {
      int next_i_around_ct;
      int prev_i_around_ct;

      /* Update which contour we're looking at */
      if (offset_in_ct == ct_npoints)
        {
          start_of_ct = i;
          offset_in_ct = 0;
          ct = ct->next;
          ct_npoints = get_contour_npoints (ct);

          /* If there is more than one contour, it will be an inner contour of the bottom and top faces. Refer to it here */
          face3d_add_contour (faces[npoints], make_contour3d (SYM(edges[i])));
          face3d_add_contour (faces[npoints + 1], make_contour3d (edges[npoints + i]));
        }

      next_i_around_ct = start_of_ct + (offset_in_ct + 1) % ct_npoints;
      prev_i_around_ct = start_of_ct + (offset_in_ct + ct_npoints - 1) % ct_npoints;

      /* Setup the face normals for the edges along the contour extrusion (top and bottom are handled separaetely) */
      /* Define the (non-normalized) face normal to point to the outside of the contour */
      face3d_set_normal (faces[i], (vertices[next_i_around_ct]->y - vertices[i]->y),
                                  -(vertices[next_i_around_ct]->x - vertices[i]->x), 0.);

      /* Assign the appropriate vertex geometric data to each edge end */
      ODATA (edges[              i]) = vertices[0 * npoints + i];
      DDATA (edges[              i]) = vertices[0 * npoints + next_i_around_ct];
      ODATA (edges[1 * npoints + i]) = vertices[1 * npoints + i];
      DDATA (edges[1 * npoints + i]) = vertices[1 * npoints + next_i_around_ct];
      ODATA (edges[2 * npoints + i]) = vertices[0 * npoints + i];
      DDATA (edges[2 * npoints + i]) = vertices[1 * npoints + i];
      LDATA (edges[              i]) = faces[i];
      RDATA (edges[              i]) = faces[npoints];
      LDATA (edges[1 * npoints + i]) = faces[npoints + 1];
      RDATA (edges[1 * npoints + i]) = faces[i];
      LDATA (edges[2 * npoints + i]) = faces[prev_i_around_ct];
      RDATA (edges[2 * npoints + i]) = faces[i];

      /* NB: Contours are counter clockwise in XY plane.
       *     edges[          0-npoints-1] are the base of the extrusion, following in the counter clockwise order
       *     edges[1*npoints-2*npoints-1] are the top  of the extrusion, following in the counter clockwise order
       *     edges[2*npoints-3*npoints-1] are the upright edges, oriented from bottom to top
       */

      /* Link edges orbiting around each bottom vertex i (0 <= i < npoints) */
      splice (edges[i], edges[2 * npoints + i]);
      splice (edges[2 * npoints + i], SYM(edges[prev_i_around_ct]));

      /* Link edges orbiting around each top vertex (npoints + i) (0 <= i < npoints) */
      splice (SYM(edges[2 * npoints + i]), edges[npoints + i]);
      splice (edges[npoints + i], SYM(edges[npoints + prev_i_around_ct]));

      if (ct->is_round)
        {

          face3d_set_cylindrical (faces[i], COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), 0., /* A point on the axis of the cylinder */
                                            0., 0., 1.,                                     /* Direction of the cylindrical axis */
                                            COORD_TO_MM (ct->radius));
          face3d_set_normal (faces[i], 1., 0., 0.);  /* A normal to the axis direction */
                                    /* XXX: ^^^ Could line this up with the direction to the vertex in the corresponding circle edge */

          edge_info_set_round (UNDIR_DATA (edges[i]),
                               COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), -COORD_TO_MM (HACK_BOARD_THICKNESS), /* Center of circle */
                               0., 0., 1., /* Normal */ COORD_TO_MM (ct->radius));
          edge_info_set_round (UNDIR_DATA (edges[npoints + i]),
                               COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), 0., /* Center of circle */
                               0., 0., -1., /* Normal */ COORD_TO_MM (ct->radius));
          edge_info_set_stitch (UNDIR_DATA (edges[2 * npoints + i]));
        }

    }

  poly_Free (&outline);

  return object;
}
