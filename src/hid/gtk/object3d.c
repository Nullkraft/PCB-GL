#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include "quad.h"
#include "vertex3d.h"
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
}

object3d *
make_object3d ()
{
  static int object3d_count = 0;
  object3d *object;

  object = malloc (sizeof (object3d));
  object->id = object3d_count++;

  return object;
}

void
destroy_object3d (object3d *object)
{
  /* XXX: LEAK GEOMETERY AND TOPOLOGY */
  free (object);
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
  int i;

  cube_vertices[0] = make_vertex3d (XOFFSET + SCALE * 0., YOFFSET + SCALE * 0., ZOFFSET + SCALE *  0.);
  cube_vertices[1] = make_vertex3d (XOFFSET + SCALE * 1., YOFFSET + SCALE * 0., ZOFFSET + SCALE *  0.);
  cube_vertices[2] = make_vertex3d (XOFFSET + SCALE * 1., YOFFSET + SCALE * 0., ZOFFSET + SCALE * -1.);
  cube_vertices[3] = make_vertex3d (XOFFSET + SCALE * 0., YOFFSET + SCALE * 0., ZOFFSET + SCALE * -1.);
  cube_vertices[4] = make_vertex3d (XOFFSET + SCALE * 0., YOFFSET + SCALE * 1., ZOFFSET + SCALE *  0.);
  cube_vertices[5] = make_vertex3d (XOFFSET + SCALE * 1., YOFFSET + SCALE * 1., ZOFFSET + SCALE *  0.);
  cube_vertices[6] = make_vertex3d (XOFFSET + SCALE * 1., YOFFSET + SCALE * 1., ZOFFSET + SCALE * -1.);
  cube_vertices[7] = make_vertex3d (XOFFSET + SCALE * 0., YOFFSET + SCALE * 1., ZOFFSET + SCALE * -1.);

  for (i = 0; i < 12; i++)
    cube_edges[i] = make_edge ();

  for (i = 0; i < 4; i++) {
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

  object = make_object3d ();
  object->first_edge = cube_edges[0];

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

  if (UNDIR_DATA(e) != NULL) {
    edge_info *info = UNDIR_DATA(e);
    if (info->is_stitch)
      return;
    if (info->is_round) {
      int i;
      glBegin (GL_LINES);
      for (i = 0; i < CIRC_SEGS; i++) {
        glVertex3f (MM_TO_COORD (info->cx + info->radius * cos (i * 2. * M_PI / (float)CIRC_SEGS)),
                    MM_TO_COORD (info->cy + info->radius * sin (i * 2. * M_PI / (float)CIRC_SEGS)),
                    MM_TO_COORD (((vertex3d *)ODATA(e))->z));
        glVertex3f (MM_TO_COORD (info->cx + info->radius * cos ((i + 1) * 2. * M_PI / (float)CIRC_SEGS)),
                    MM_TO_COORD (info->cy + info->radius * sin ((i + 1) * 2. * M_PI / (float)CIRC_SEGS)),
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
  quad_enum (object3d_test_object->first_edge, draw_quad_edge, NULL);
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

  while (n > 0) {
    vertex = vertex->next; /* The VNODE structure is circularly linked, so wrapping is OK */
    n--;
  }

  *x = COORD_TO_MM (vertex->point[0]);
  *y = COORD_TO_MM (vertex->point[1]); /* FIXME: PCB's coordinate system has y increasing downwards */
}

void
object3d_export_to_step (object3d *object, char *filename)
{
  FILE *f;
  time_t currenttime;
  struct tm utc;
  //int next_step_identifier;

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

  //next_step_identifier = 21;

  /* TODO.. EXPORT FROM A QUAD DATA-STRUCTURE */
#if 0
#define FWD 1
#define REV 2
static void
quad_emit_board_contour_step (FILE *f, PLINE *contour)
{
  int ncontours;
  int npoints;

  int brep_identifier;

  int bottom_plane_identifier;
  int top_plane_identifier;
  int *side_plane_identifier;

  int *bottom_infinite_line_identifier;
  int *top_infinite_line_identifier;
  int *side_infinite_line_identifier;

  int *bottom_vertex_identifier;
  int *top_vertex_identifier;

  int *bottom_edge_identifier;
  int *top_edge_identifier;
  int *side_edge_identifier;

  int *bottom_face_bound_identifier;
  int *top_face_bound_identifier;

  int bottom_face_identifier;
  int top_face_identifier;
  int *side_face_identifier;

  int pcb_shell_identifier;

  int i;

  PLINE *ct;

  ncontours = 0;
  npoints = 0;
  ct = contour;
  while (ct != NULL) {
    ncontours ++;
    npoints += get_contour_npoints (ct);
    ct = ct->next;
  }

  /* TODO: Avoid needing to store these identifiers by nailing down our usage pattern of identifiers */
  /* Allocate some storage for identifiers */

            side_plane_identifier = malloc (sizeof (int) * npoints);
  bottom_infinite_line_identifier = malloc (sizeof (int) * npoints);
     top_infinite_line_identifier = malloc (sizeof (int) * npoints);
    side_infinite_line_identifier = malloc (sizeof (int) * npoints);
         bottom_vertex_identifier = malloc (sizeof (int) * npoints);
            top_vertex_identifier = malloc (sizeof (int) * npoints);
           bottom_edge_identifier = malloc (sizeof (int) * npoints);
              top_edge_identifier = malloc (sizeof (int) * npoints);
             side_edge_identifier = malloc (sizeof (int) * npoints);
             side_face_identifier = malloc (sizeof (int) * npoints);

     bottom_face_bound_identifier = malloc (sizeof (int) * ncontours);
        top_face_bound_identifier = malloc (sizeof (int) * ncontours);

  /* For a n-sided outline, we need: */

  // PLANES:               2 + n
  // 2 bottom + top planes
  // n side planes

  // INFINITE LINES:       3n
  // n for the bottom (in the bottom plane)
  // n for the top (in the top plane)
  // n for the sides (joining the top + bottom vertex of the extruded shape (n sided outline = n vertices)

  // VERTICES:             2n
  // n for the bottom (in the bottom plane)
  // n for the top (in the top plane)

  // EDGES:                3n          (6n oriented edges)
  // n for the bottom
  // n for the top
  // n for the sides

  // FACES:                2 + n
  // 2 bottom + top faces
  // n side faces

  // A consistent numbering scheme will avoid needing complex data-structures here!

  /* Save a place for the brep identifier */
  brep_identifier = next_step_identifier++;

  /* Define the bottom and top planes */
  fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
              "#%i = DIRECTION ( 'NONE', (  0.0,  0.0,  1.0 ) ) ; "
              "#%i = DIRECTION ( 'NONE', (  1.0,  0.0,  0.0 ) ) ; "
              "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i, #%i, #%i ) ; "
              "#%i = PLANE ( 'NONE',  #%i ) ;\n",
           next_step_identifier, 0.0, 0.0, -COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
           next_step_identifier + 1,
           next_step_identifier + 2,
           next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2,
           next_step_identifier + 4, next_step_identifier + 3);
  bottom_plane_identifier = next_step_identifier + 4;
  next_step_identifier = next_step_identifier + 5;

  fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
              "#%i = DIRECTION ( 'NONE', (  0.0,  0.0, -1.0 ) ) ; "
              "#%i = DIRECTION ( 'NONE', ( -1.0,  0.0,  0.0 ) ) ; "
              "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i, #%i, #%i ) ; "
              "#%i = PLANE ( 'NONE',  #%i ) ;\n",
           next_step_identifier, 0.0, 0.0, COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
           next_step_identifier + 1,
           next_step_identifier + 2,
           next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2,
           next_step_identifier + 4, next_step_identifier + 3);
  top_plane_identifier = next_step_identifier + 4;
  next_step_identifier = next_step_identifier + 5;

  /* Define the side planes */
  for (i = 0; i < npoints; i++) {
    double x1, y1, x2, y2;

    /* Walk through the contours until we find the right one to look at */
    PLINE *ct = contour;
    int adjusted_i = i;

    while (adjusted_i >= get_contour_npoints (ct)) {
      adjusted_i -= get_contour_npoints (ct);
      ct = ct->next;
    }

    if (ct->is_round)
      {
        /* HACK SPECIAL CASE FOR ROUND CONTOURS (Surface edges bounded by a cylindrical surface, not n-planes) */

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

        side_plane_identifier[i] = next_step_identifier + 4;
        next_step_identifier = next_step_identifier + 5;
      }
    else
      {
        get_contour_coord_n_in_mm (ct, adjusted_i,     &x1, &y1);
        get_contour_coord_n_in_mm (ct, adjusted_i + 1, &x2, &y2);

        fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i, #%i, #%i ) ; "
                    "#%i = PLANE ( 'NONE',  #%i ) ;\n",
                 next_step_identifier,     /* A point on the plane                      */ x1, y1, 0.0,
                 next_step_identifier + 1, /* An axis direction pointing into the shape */ -(y2 - y1), (x2 - x1), 0.0, // <--- NOT SURE IF I NEED TO NORMALISE THIS, OR FLIP THE DIRECTION??
                 next_step_identifier + 2, /* A reference direction pointing.. "meh"?   */ 0.0, 0.0, 1.0,
                 next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2,
                 next_step_identifier + 4, next_step_identifier + 3);
        side_plane_identifier[i] = next_step_identifier + 4;
        next_step_identifier = next_step_identifier + 5;
      }
  }

  /* Define the infinite lines */
  for (i = 0; i < npoints; i++) {
    double x1, y1, x2, y2;

    /* Walk through the contours until we find the right one to look at */
    PLINE *ct = contour;
    int adjusted_i = i;

    while (adjusted_i >= get_contour_npoints (ct)) {
      adjusted_i -= get_contour_npoints (ct);
      ct = ct->next;
    }

    get_contour_coord_n_in_mm (ct, adjusted_i,     &x1, &y1);
    get_contour_coord_n_in_mm (ct, adjusted_i + 1, &x2, &y2);

    if (ct->is_round)
      {
        /* HACK SPECIAL CASE FOR ROUND CONTOURS (Top and bottom faces bounded a circular contour, not n-lines) */

        /* Bottom */
        fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i,  #%i,  #%i ) ;"
                    "#%i = CIRCLE ( 'NONE', #%i, %f ) ;\n",
                 next_step_identifier,     /* Center of the circle   */ COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), -COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
                 next_step_identifier + 1, /* Normal of circle?      */ 0.0, 0.0, -1.0, // <--- NOT SURE IF I NEED TO FLIP THE DIRECTION??
                 next_step_identifier + 2, /* ??????                 */ -1.0, 0.0, 0.0, // NOT SURE WHAT THIS IS!
                 next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2,
                 next_step_identifier + 4, next_step_identifier + 3, COORD_TO_MM (ct->radius));
        bottom_infinite_line_identifier[i] = next_step_identifier + 4;
        next_step_identifier = next_step_identifier + 5;

        /* Top */
        fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i,  #%i,  #%i ) ;"
                    "#%i = CIRCLE ( 'NONE', #%i, %f ) ;\n",
                 next_step_identifier,     /* Center of the circle   */ COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
                 next_step_identifier + 1, /* Normal of circle?      */ 0.0, 0.0, -1.0, // <--- NOT SURE IF I NEED TO FLIP THE DIRECTION??
                 next_step_identifier + 2, /* ??????                 */ -1.0, 0.0, 0.0, // NOT SURE WHAT THIS IS!
                 next_step_identifier + 3, next_step_identifier, next_step_identifier + 1, next_step_identifier + 2,
                 next_step_identifier + 4, next_step_identifier + 3, COORD_TO_MM (ct->radius));
        top_infinite_line_identifier[i] = next_step_identifier + 4;
        next_step_identifier = next_step_identifier + 5;
      }
    else
      {
        /* Bottom */
        fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i = VECTOR ( 'NONE', #%i, 1000.0 ) ; "
                    "#%i = LINE ( 'NONE', #%i, #%i ) ;\n",
                 next_step_identifier,     /* A point on the line         */ x1, y1, -COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
                 next_step_identifier + 1, /* A direction along the line  */ (x2 - x1), (y2 - y1), 0.0, // <--- NOT SURE IF I NEED TO NORMALISE THIS, OR FLIP THE DIRECTION??
                 next_step_identifier + 2, next_step_identifier + 1,
                 next_step_identifier + 3, next_step_identifier, next_step_identifier + 2);
        bottom_infinite_line_identifier[i] = next_step_identifier + 3;
        next_step_identifier = next_step_identifier + 4;

        /* Top */
        fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                    "#%i = VECTOR ( 'NONE', #%i, 1000.0 ) ; "
                    "#%i = LINE ( 'NONE', #%i, #%i ) ;\n",
                 next_step_identifier,     /* A point on the line         */ x1, y1, COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
                 next_step_identifier + 1, /* A direction along the line  */ (x2 - x1), (y2 - y1), 0.0, // <--- NOT SURE IF I NEED TO NORMALISE THIS, OR FLIP THE DIRECTION??
                 next_step_identifier + 2, next_step_identifier + 1,
                 next_step_identifier + 3, next_step_identifier, next_step_identifier + 2);
        top_infinite_line_identifier[i] = next_step_identifier + 3;
        next_step_identifier = next_step_identifier + 4;
      }

    /* Side */
    fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                "#%i = VECTOR ( 'NONE', #%i, 1000.0 ) ; "
                "#%i = LINE ( 'NONE', #%i, #%i ) ;\n",
             next_step_identifier,     /* A point on the line         */ x1, y1, -COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
             next_step_identifier + 1, /* A direction along the line  */ 0.0, 0.0, 1.0, // <--- NOT SURE IF I NEED TO NORMALISE THIS, OR FLIP THE DIRECTION??
             next_step_identifier + 2, next_step_identifier + 1,
             next_step_identifier + 3, next_step_identifier, next_step_identifier + 2);
    side_infinite_line_identifier[i] = next_step_identifier + 3;
    next_step_identifier = next_step_identifier + 4;
  }

  /* Define the vertices */
  for (i = 0; i < npoints; i++) {
    double x1, y1;

    /* Walk through the contours until we find the right one to look at */
    PLINE *ct = contour;
    int adjusted_i = i;

    while (adjusted_i >= get_contour_npoints (ct)) {
      adjusted_i -= get_contour_npoints (ct);
      ct = ct->next;
    }

    get_contour_coord_n_in_mm (ct, adjusted_i, &x1, &y1);

    /* Bottom */
    fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                "#%i = VERTEX_POINT ( 'NONE', #%i ) ;\n",
             next_step_identifier,     /* Vertex coordinate  */ x1, y1, -COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
             next_step_identifier + 1, next_step_identifier);
    bottom_vertex_identifier[i] = next_step_identifier + 1;
    next_step_identifier = next_step_identifier + 2;

    /* Top */
    fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                "#%i = VERTEX_POINT ( 'NONE', #%i ) ;\n",
             next_step_identifier,     /* Vertex coordinate  */ x1, y1, COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
             next_step_identifier + 1, next_step_identifier);
    top_vertex_identifier[i] = next_step_identifier + 1;
    next_step_identifier = next_step_identifier + 2;
  }

  /* Define the Edges */
  for (i = 0; i < npoints; i++) {

    /* Walk through the contours until we find the right one to look at */
    PLINE *ct = contour;
    int adjusted_i = i;
    int i_start = 0;

    while (adjusted_i >= get_contour_npoints (ct)) {
      adjusted_i -= get_contour_npoints (ct);
      i_start += get_contour_npoints (ct);
      ct = ct->next;
    }

    /* Due to the way the index wrapping works, this works for circular cutouts as well as n-sided */

    /* Bottom */
    fprintf (f, "#%i = EDGE_CURVE ( 'NONE', #%i, #%i, #%i,   .T. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .F. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .T. ) ;\n",
             next_step_identifier, bottom_vertex_identifier[i], bottom_vertex_identifier[i_start + (adjusted_i + 1) % get_contour_npoints (ct)], bottom_infinite_line_identifier[i],      // <-- MIGHT NEED TO REVERSE THIS???
             next_step_identifier + 1, next_step_identifier,
             next_step_identifier + 2, next_step_identifier);
    bottom_edge_identifier[i] = next_step_identifier; /* Add 1 for same oriented, add 2 for back oriented */
    next_step_identifier = next_step_identifier + 3;

    /* Top */
    fprintf (f, "#%i = EDGE_CURVE ( 'NONE', #%i, #%i, #%i,   .T. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .F. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .T. ) ;\n",
             next_step_identifier, top_vertex_identifier[i], top_vertex_identifier[i_start + (adjusted_i + 1) % get_contour_npoints (ct)], top_infinite_line_identifier[i],                 // <-- MIGHT NEED TO REVERSE THIS???
             next_step_identifier + 1, next_step_identifier,
             next_step_identifier + 2, next_step_identifier);
    top_edge_identifier[i] = next_step_identifier; /* Add 1 for same oriented, add 2 for back oriented */
    next_step_identifier = next_step_identifier + 3;

    /* Side */
    fprintf (f, "#%i = EDGE_CURVE ( 'NONE', #%i, #%i, #%i,   .T. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .F. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .T. ) ;\n",
             next_step_identifier, bottom_vertex_identifier[i], top_vertex_identifier[i], side_infinite_line_identifier[i],
             next_step_identifier + 1, next_step_identifier,
             next_step_identifier + 2, next_step_identifier);
    side_edge_identifier[i] = next_step_identifier; /* Add 1 for same oriented, add 2 for back oriented */
    next_step_identifier = next_step_identifier + 3;
  }

  /* Define the faces */

  /* Bottom */
  {
    PLINE *ct = contour;
    int icont;
    int start_i;

    start_i = 0;
    for (icont = 0; icont < ncontours; icont++, start_i += get_contour_npoints (ct), ct = ct->next) {

      fprintf (f, "#%i = EDGE_LOOP ( 'NONE', ( ",
               next_step_identifier);
      for (i = start_i + get_contour_npoints (ct) - 1; i > start_i; i--)
        fprintf (f, "#%i, ", bottom_edge_identifier[i] + FWD);
      fprintf (f, "#%i ) ) ; "
                  "#%i = FACE_%sBOUND ( 'NONE', #%i, .T. ) ; \n",
               bottom_edge_identifier[start_i] + FWD,
               next_step_identifier + 1, icont > 0 ? "" : "OUTER_", next_step_identifier);
      bottom_face_bound_identifier[icont] = next_step_identifier + 1;
      next_step_identifier = next_step_identifier + 2;
    }

    fprintf (f, "#%i = ADVANCED_FACE ( 'NONE', ( ",
             next_step_identifier);
    for (icont = 0; icont < ncontours - 1; icont++)
      fprintf (f, "#%i, ",
               bottom_face_bound_identifier[icont]);
    fprintf (f, "#%i ), #%i, .F. ) ;\n",
             bottom_face_bound_identifier[ncontours - 1], bottom_plane_identifier);
    bottom_face_identifier = next_step_identifier;
    next_step_identifier = next_step_identifier + 1;
  }

  /* Top */
  {
    PLINE *ct = contour;
    int icont;
    int start_i;

    start_i = 0;
    for (icont = 0; icont < ncontours; icont++, start_i += get_contour_npoints (ct), ct = ct->next) {
      fprintf (f, "#%i = EDGE_LOOP ( 'NONE', ( ",
               next_step_identifier);
      for (i = start_i; i < start_i + get_contour_npoints (ct) - 1; i++)
        fprintf (f, "#%i, ", top_edge_identifier[i] + REV);
      fprintf (f, "#%i ) ) ; "
                  "#%i = FACE_%sBOUND ( 'NONE', #%i, .T. ) ; \n",
               top_edge_identifier[start_i + get_contour_npoints (ct) - 1] + REV,
               next_step_identifier + 1, icont > 0 ? "" : "OUTER_", next_step_identifier);
      top_face_bound_identifier[icont] = next_step_identifier + 1;
      next_step_identifier = next_step_identifier + 2;
    }

    fprintf (f, "#%i = ADVANCED_FACE ( 'NONE', ( ",
             next_step_identifier);
    for (icont = 0; icont < ncontours - 1; icont++)
      fprintf (f, "#%i, ",
               top_face_bound_identifier[icont]);
    fprintf (f, "#%i ), #%i, .F. ) ;\n",
             top_face_bound_identifier[ncontours - 1], top_plane_identifier);
    top_face_identifier = next_step_identifier;
    next_step_identifier = next_step_identifier + 1;
  }

  /* Sides */
  for (i = 0; i < npoints; i++) {

    /* Walk through the contours until we find the right one to look at */
    PLINE *ct = contour;
    int adjusted_i = i;
    int i_start = 0;

    while (adjusted_i >= get_contour_npoints (ct)) {
      adjusted_i -= get_contour_npoints (ct);
      i_start += get_contour_npoints (ct);
      ct = ct->next;
    }

    fprintf (f, "#%i = EDGE_LOOP ( 'NONE', ( #%i, #%i, #%i, #%i ) ) ; "
                "#%i = FACE_OUTER_BOUND ( 'NONE', #%i, .T. ) ; "
                "#%i = ADVANCED_FACE ( 'NONE', ( #%i ), #%i, .F. ) ;\n",
             next_step_identifier, side_edge_identifier[i_start + (adjusted_i + 1) % get_contour_npoints (ct)] + REV, top_edge_identifier[i] + FWD, side_edge_identifier[i] + FWD, bottom_edge_identifier[i] + REV,
             next_step_identifier + 1, next_step_identifier,
             next_step_identifier + 2, next_step_identifier + 1, side_plane_identifier[i]);
    side_face_identifier[i] = next_step_identifier + 2;
    next_step_identifier = next_step_identifier + 3;
  }

  /* Closed shell which bounds the brep solid */
  pcb_shell_identifier = next_step_identifier;
  next_step_identifier++;
  fprintf (f, "#%i = CLOSED_SHELL ( 'NONE', ( #%i, #%i, ", pcb_shell_identifier, bottom_face_identifier, top_face_identifier);
  for (i = 0; i < npoints - 1; i++) {
    fprintf (f, "#%i, ", side_face_identifier[i]);
  }
  fprintf (f, "#%i) ) ;\n",
           side_face_identifier[npoints - 1]);

  /* Finally emit the brep solid definition */
  fprintf (f, "#%i = MANIFOLD_SOLID_BREP ( 'PCB outline', #%i ) ;\n", brep_identifier, pcb_shell_identifier);

  free (side_plane_identifier);
  free (bottom_infinite_line_identifier);
  free (top_infinite_line_identifier);
  free (side_infinite_line_identifier);
  free (bottom_vertex_identifier);
  free (top_vertex_identifier);
  free (bottom_edge_identifier);
  free (top_edge_identifier);
  free (side_edge_identifier);
  free (side_face_identifier);
  free (bottom_face_bound_identifier);
  free (top_face_bound_identifier);
}
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
  POLYAREA *outline;
  PLINE *contour;
  PLINE *ct;
  int ncontours;
  int npoints;
  int i;
  vertex3d **vertices;
  edge_ref *edges;
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
  while (ct != NULL) {
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

  vertices = malloc (sizeof (vertex3d *) * 2 * npoints);
  edges    = malloc (sizeof (edge_ref  ) * 3 * npoints);

  /* Define the vertices */
  ct = contour;
  start_of_ct = 0;
  offset_in_ct = 0;
  ct_npoints = get_contour_npoints (ct);

  for (i = 0; i < npoints; i++, offset_in_ct++) {
    double x1, y1;

    /* Update which contour we're looking at */
    if (offset_in_ct == ct_npoints) {
      offset_in_ct = 0;
      ct = ct->next;
      ct_npoints = get_contour_npoints (ct);
    }

    get_contour_coord_n_in_mm (ct, offset_in_ct, &x1, &y1);
    vertices[i]           = make_vertex3d (x1, y1, -COORD_TO_MM (HACK_BOARD_THICKNESS)); /* Bottom */
    vertices[npoints + i] = make_vertex3d (x1, y1, 0); /* Top */
  }

  /* Define the edges */

  for (i = 0; i < 3 * npoints; i++)
    edges[i] = make_edge ();

  ct = contour;
  start_of_ct = 0;
  offset_in_ct = 0;
  ct_npoints = get_contour_npoints (ct);

  for (i = 0; i < npoints; i++, offset_in_ct++) {
    int next_i_around_ct;

    /* Update which contour we're looking at */
    if (offset_in_ct == ct_npoints) {
      start_of_ct = i;
      printf ("start_of_ct = %i\n", start_of_ct);
      offset_in_ct = 0;
      ct = ct->next;
      ct_npoints = get_contour_npoints (ct);
    }

    next_i_around_ct = start_of_ct + (offset_in_ct + 1) % ct_npoints;

    /* Assign the appropriate vertex geometric data to each edge end */
    ODATA (edges[0 * npoints + i]) = vertices[0 * npoints + i];
    DDATA (edges[0 * npoints + i]) = vertices[0 * npoints + next_i_around_ct];
    ODATA (edges[1 * npoints + i]) = vertices[1 * npoints + i];
    DDATA (edges[1 * npoints + i]) = vertices[1 * npoints + next_i_around_ct];
    ODATA (edges[2 * npoints + i]) = vertices[0 * npoints + i];
    DDATA (edges[2 * npoints + i]) = vertices[1 * npoints + i];

    /* Link edges orbiting around each bottom vertex i (0 <= i < npoints) */
    splice (edges[i], edges[npoints + i]);
    splice (edges[npoints + i], SYM(edges[next_i_around_ct]));

    /* Link edges orbiting around each top vertex (npoints + i) (0 <= i < npoints) */
    splice (edges[npoints + i], SYM(edges[npoints + next_i_around_ct]));
    splice (SYM(edges[npoints + next_i_around_ct]), SYM(edges[2 * npoints + i]));

    /* XXX: TOPOLOGY WILL BE OK, MAY NEED MORE INFO FOR GEOMETRY */
    /* XXX: DO WE NEED TO ASSIGN EXTRA INFORMATION TO CIRCULAR EDGES FOR RENDERING / EXPORT??? */
    if (ct->is_round) {
      UNDIR_DATA (edges[0 * npoints + i]) = make_edge_info (false, true, COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), COORD_TO_MM (ct->radius));
      UNDIR_DATA (edges[1 * npoints + i]) = make_edge_info (false, true, COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), COORD_TO_MM (ct->radius));
      UNDIR_DATA (edges[2 * npoints + i]) = make_edge_info (true,  true, COORD_TO_MM (ct->cx), COORD_TO_MM (ct->cy), COORD_TO_MM (ct->radius));
    }

  }

  poly_Free (&outline);

  object = make_object3d ();
  object->first_edge = edges[0]; /* edges[34] */

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
