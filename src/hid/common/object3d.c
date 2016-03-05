#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include <glib.h>

#include "quad.h"
#include "vertex3d.h"
#include "face3d.h"
#include "edge3d.h"
#include "appearance.h"
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
}

object3d *
make_object3d (char *name)
{
  static int object3d_count = 0;
  object3d *object;

  object = g_new (object3d, 1);
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
      object3d_add_edge (object, cube_edges[i]);
    }

  for (i = 0; i < 6; i++)
    {
      faces[i] = make_face3d ();
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
              glVertex3f (MM_TO_COORD (info->cx + info->radius * cos (i * 2. * M_PI / (double)CIRC_SEGS)),
                          MM_TO_COORD (info->cy + info->radius * sin (i * 2. * M_PI / (double)CIRC_SEGS)),
                          MM_TO_COORD (((vertex3d *)ODATA(e))->z));
              glVertex3f (MM_TO_COORD (info->cx + info->radius * cos ((i + 1) * 2. * M_PI / (double)CIRC_SEGS)),
                          MM_TO_COORD (info->cy + info->radius * sin ((i + 1) * 2. * M_PI / (double)CIRC_SEGS)),
                          MM_TO_COORD (((vertex3d *)ODATA(e))->z));
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


static void
fprint_idlist (FILE *f, int *ids, int num_ids)
{
  int i;
  fprintf (f, "(");
  for (i = 0; i < num_ids - 1; i++)
    fprintf (f, "#%i, ", ids[i]);
  fprintf (f, "#%i) ) ;\n", ids[i]);
}

void
object3d_export_to_step (object3d *object, const char *filename)
{
  FILE *f;
  time_t currenttime;
  struct tm utc;
  int next_step_identifier;
  int brep_identifier;
  int pcb_shell_identifier;

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

  /* BREP STUFF FROM #21 onwards say? */
  fprintf (f, "#19 = ADVANCED_BREP_SHAPE_REPRESENTATION ( '%s', ( /* Manifold_solid_brep */ #21, #13 ), #18 ) ;\n"
              "#20 = SHAPE_DEFINITION_REPRESENTATION ( #9, #19 ) ;\n",
              "test_pcb_absr_name");

  next_step_identifier = 21;

  /* TODO.. EXPORT FROM A QUAD DATA-STRUCTURE */
#if 1
#define FWD 1
#define REV 2

  /* Save a place for the brep identifier */
  brep_identifier = next_step_identifier++;

  /* Define ininite planes corresponding to every planar face, and cylindrical surfaces for every cylindrical face */
  /* XXX: ENUMERATE OVER SPATIAL DATA-STRUCTURE */
  for (;;)
    {
      if (ct->is_round)
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
                   next_step_identifier, /* A point on the axis of the cylinder */ COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), 0.0,
                   next_step_identifier + 1, /* Direction of surface axis... not sure if the sign of the direction matters */ 0.0, 0.0, 1.0,
                   next_step_identifier + 2, /* URM???? NOT SURE WHAT THIS DIRECTION IS FOR                                */ 1.0, 0.0, 0.0,
                   next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2,
                   next_step_identifier + 4, next_step_identifier + 3, COORD_TO_MM (ct->radius));

          plane_identifiers[i] = next_step_identifier + 4;
          next_step_identifier = next_step_identifier + 5;
        }
      else
        {
          /* FOR CONSISTENCY WITH ABOVE, DEFINE PLANE NORMAL TO BE POINTING INSIDE THE SHAPE.
           * THIS ALLOWS TO FLIP THE ORIENTATION OF THE UNDERLYING SURFACE WHEN DEFINING EVERY ADVANCED_FACE
           */
          fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; ", next_step_identifier, x1, y1, 0.0);    // <-- A locating point on the plane. Forms 0,0 of its parameterised coords.
          fprintf (f, "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; ", next_step_identifier + 1,  -(y2 - y1), (x2 - x1), 0.0);  /* An axis direction pointing into the shape */ // <-- Or is this the z-axis of the coordinate placement -> plane normal?
          fprintf (f, "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; ", next_step_identifier + 2, 0.0, 0.0, 1.0);          // <-- Reference x-axis, should be orthogonal to the z-axis above.
          fprintf (f, "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i, #%i, #%i ) ; "
                   next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2);
          fprintf (f, "#%i = PLANE ( 'NONE',  #%i ) ;\n",
                   next_step_identifier + 4, next_step_identifier + 3);
          plane_identifiers[i] = next_step_identifier + 4;
          next_step_identifier = next_step_identifier + 5;
        }
    }

  /* Define the infinite lines corresponding to every edge (either lines or circles)*/
  /* XXX: ENUMERATE OVER SPATIAL DATA-STRUCTURE */
  for (;;)
    {

      if (ct->is_round)
        {
          fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i,  #%i,  #%i ) ;"
                      "#%i = CIRCLE ( 'NONE', #%i, %f ) ;\n",
                   next_step_identifier,     /* Center of the circle   */ edge_info->cx, edge_info->cy, edge_info->cz, // <--- Center of coordinate placement
                   next_step_identifier + 1, /* Normal of circle?      */ 0.0, 0.0, -1.0, // <--- Z-axis direction of placement             /* XXX: PULL FROM FACE DATA */
                   next_step_identifier + 2, /* ??????                 */ -1.0, 0.0, 0.0, // <--- Approximate X-axis direction of placement /* XXX: PULL FROM FACE DATA */
                   next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2,
                   next_step_identifier + 4, next_step_identifier + 3, edge_info->radius);
          infinite_line_identifiers[i] = next_step_identifier + 4;
          next_step_identifier = next_step_identifier + 5;
        }
      else
        {
          double dx, dy, dz;

          dx = end_v->x - start_v->x;
          dy = end_v->y - start_v->y;
          dz = end_v->z - start_v->z;

          fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                      "#%i = VECTOR ( 'NONE', #%i, 1000.0 ) ; "
                      "#%i = LINE ( 'NONE', #%i, #%i ) ;\n",
                   next_step_identifier,     /* A point on the line         */ start_v->x, start_v->y, start_v->z,
                   next_step_identifier + 1, /* A direction along the line  */ dx, dy, dz,
                   next_step_identifier + 2, next_step_identifier + 1,
                   next_step_identifier + 3, next_step_identifier, next_step_identifier + 2);
          infinite_line_identifiers[i] = next_step_identifier + 3;
          next_step_identifier = next_step_identifier + 4;
        }
    }

  /* Define the vertices */
  /* XXX: ENUMERATE OVER SPATIAL DATA-STRUCTURE */
  for (;;)
    {
      fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; ", next_step_identifier,     /* Vertex coordinate  */ x, y, z);
      fprintf (f, "#%i = VERTEX_POINT ( 'NONE', #%i ) ;\n",             next_step_identifier + 1, next_step_identifier);
      vertex_identifiers[i] = next_step_identifier + 1;
      next_step_identifier = next_step_identifier + 2;
    }

  /* Define the Edges */
  /* XXX: ENUMERATE OVER SPATIAL DATA-STRUCTURE */
  for (;;)
    {
      fprintf (f, "#%i = EDGE_CURVE ( 'NONE', #%i, #%i, #%i,   .T. ) ; ", next_step_identifier, start_vertex_identifiers[i], end_vertex_identifiers[i], infinite_line_identifier[i]);
      fprintf (f, "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .F. ) ; ",    next_step_identifier + 1, next_step_identifier);
      fprintf (f, "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .T. ) ;\n",   next_step_identifier + 2, next_step_identifier);
      edge_identifiers[i] = next_step_identifier; /* Add 1 for same oriented, add 2 for back oriented */
      next_step_identifier = next_step_identifier + 3;
    }

  /* Define the faces */
  /* XXX: ENUMERATE OVER SPATIAL DATA-STRUCTURE (ESPECIALLY FOR CORRECT ORDERING!)*/
  for (;;)
    {
      start_i = 0;
      for (icont = 0; icont < ncontours; icont++, start_i += get_contour_npoints (ct), ct = ct->next)
        {

          /* XXX: FWD / BWD NEEDS TO BE FUDGED IN HERE PERHAPS? */ 
          fprintf (f, "#%i = EDGE_LOOP ( 'NONE', ", next_step_identifier); fprint_idlist (f, face_edge_identifiers[i], face_contour_npoints[i]); fprintf (f, " ) ; ");
          fprintf (f, "#%i = FACE_%sBOUND ( 'NONE', #%i, .T. ) ; \n", next_step_identifier + 1, icont > 0 ? "" : "OUTER_", next_step_identifier);
          face_bound_identifiers[icont] = next_step_identifier + 1;
          next_step_identifier = next_step_identifier + 2;
        }

      fprintf (f, "#%i = ADVANCED_FACE ( 'NONE', ", next_step_identifier); fprint_idlist (f, face_bound_identifiers, ncontours);  fprintf (f, ", #%i, .F. ) ;\n", plane_identifiers[i]);
      face_identifiers[i] = next_step_identifier;
      next_step_identifier = next_step_identifier + 1;
    }

  /* Closed shell which bounds the brep solid */
  pcb_shell_identifier = next_step_identifier;
  next_step_identifier++;
  fprintf (f, "#%i = CLOSED_SHELL ( 'NONE', ", pcb_shell_identifier); fprint_idlist (f, face_identifiers, nfaces); fprintf (f, " ) ;\n");

  /* Finally emit the brep solid definition */
  fprintf (f, "#%i = MANIFOLD_SOLID_BREP ( 'PCB outline', #%i ) ;\n", brep_identifier, pcb_shell_identifier);

#undef FWD
#undef REV
#endif

  fprintf (f, "ENDSEC;\n" );
  fprintf (f, "END-ISO-10303-21;\n" );

  fclose (f);
}

object3d *
object3d_from_board_outline (void)
{
  object3d *object;
  appearance *board_appearance;
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
  appearance_set_color (board_appearance, 1., 1., 0.);

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
      vertices[npoints + i] = make_vertex3d (x1, y1, 0); /* Top */

      object3d_add_vertex (object, vertices[i]);
      object3d_add_vertex (object, vertices[npoints + i]);
    }

  /* Define the edges */
  for (i = 0; i < 3 * npoints; i++)
    {
      edges[i] = make_edge ();
      object3d_add_edge (object, edges[i]);
    }

  /* Define the faces */
  for (i = 0; i < npoints; i++)
    {
      faces[i] = make_face3d ();
      /* Pick one of the upright edges which is within this face outer contour loop, and link it to the face */
      face3d_add_contour (faces[i], edges[2 * npoints + i]);
    }
  faces[npoints]     = make_face3d (); /* bottom_face */
  faces[npoints + 1] = make_face3d (); /* top_face */

  /* Pick the first bottom / top edge which within the bottom / top face outer contour loop, and link it to the face */
  face3d_add_contour (faces[npoints], edges[0]);
  face3d_add_contour (faces[npoints + 1], edges[npoints]);

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
          printf ("start_of_ct = %i\n", start_of_ct);
          offset_in_ct = 0;
          ct = ct->next;
          ct_npoints = get_contour_npoints (ct);

          /* If there is more than one contour, it will be an inner contour of the bottom and top faces. Refer to it here */
          face3d_add_contour (faces[npoints], edges[i]);
          face3d_add_contour (faces[npoints + 1], edges[npoints + i]);
        }

      next_i_around_ct = start_of_ct + (offset_in_ct + 1) % ct_npoints;
      prev_i_around_ct = start_of_ct + (offset_in_ct + ct_npoints - 1) % ct_npoints;

      /* Assign the appropriate vertex geometric data to each edge end */
      ODATA (edges[0 * npoints + i]) = vertices[0 * npoints + i];
      DDATA (edges[0 * npoints + i]) = vertices[0 * npoints + next_i_around_ct];
      ODATA (edges[1 * npoints + i]) = vertices[1 * npoints + i];
      DDATA (edges[1 * npoints + i]) = vertices[1 * npoints + next_i_around_ct];
      ODATA (edges[2 * npoints + i]) = vertices[0 * npoints + i];
      DDATA (edges[2 * npoints + i]) = vertices[1 * npoints + i];
      LDATA (edges[0 * npoints + i]) = faces[i];
      RDATA (edges[0 * npoints + i]) = faces[npoints];
      LDATA (edges[1 * npoints + i]) = faces[npoints + 1];
      RDATA (edges[1 * npoints + i]) = faces[i];
      LDATA (edges[2 * npoints + i]) = faces[prev_i_around_ct];
      RDATA (edges[2 * npoints + i]) = faces[i];

      /* Link edges orbiting around each bottom vertex i (0 <= i < npoints) */
      splice (edges[i], edges[npoints + i]);
      splice (edges[npoints + i], SYM(edges[next_i_around_ct]));

      /* Link edges orbiting around each top vertex (npoints + i) (0 <= i < npoints) */
      splice (edges[npoints + i], SYM(edges[npoints + next_i_around_ct]));
      splice (SYM(edges[npoints + next_i_around_ct]), SYM(edges[2 * npoints + i]));

      /* XXX: TOPOLOGY WILL BE OK, MAY NEED MORE INFO FOR GEOMETRY */
      /* XXX: DO WE NEED TO ASSIGN EXTRA INFORMATION TO CIRCULAR EDGES FOR RENDERING / EXPORT??? */
      if (ct->is_round)
        {
          UNDIR_DATA (edges[0 * npoints + i]) = make_edge_info (false, true, COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), COORD_TO_MM (ct->radius));
          UNDIR_DATA (edges[1 * npoints + i]) = make_edge_info (false, true, COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), COORD_TO_MM (ct->radius));
          UNDIR_DATA (edges[2 * npoints + i]) = make_edge_info (true,  true, COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), COORD_TO_MM (ct->radius));
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
