#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h> /* not used */
#include <stdlib.h>
#include <string.h>
#include <assert.h> /* not used */
#include <time.h>

#include "global.h"
#include "data.h"
#include "misc.h"
#include "error.h"
#include "draw.h"
#include "pcb-printf.h"

#include "hid.h"
#include "hid_draw.h"
#include "../hidint.h"
#include "hid/common/hidnogui.h"
#include "hid/common/draw_helpers.h"
#include "step.h"
#include "../../print.h"
#include "hid/common/hidinit.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

#define CRASH fprintf(stderr, "HID error: pcb called unimplemented STEP function %s.\n", __FUNCTION__); abort()

#define HACK_BOARD_THICKNESS MM_TO_COORD(1.6)

static int step_set_layer (const char *name, int group, int empty);
static void use_gc (hidGC gc);

typedef struct hid_gc_struct
{
  HID *me_pointer;
  EndCapStyle cap;
  Coord width;
  unsigned char r, g, b;
  int erase;
  int faded;
} hid_gc_struct;


HID_Attribute step_attribute_list[] = {
  /* other HIDs expect this to be first.  */

/* %start-doc options "91 STEP Export"
@ftable @code
@item --stepfile <string>
Name of the STEP output file. Can contain a path.
@end ftable
%end-doc
*/
  {"stepfile", "STEP output file",
   HID_String, 0, 0, {0, 0, 0}, 0, 0},
#define HA_stepfile 0
};

#define NUM_OPTIONS (sizeof(step_attribute_list)/sizeof(step_attribute_list[0]))

REGISTER_ATTRIBUTES (step_attribute_list)

/* All file-scope data is in global struct */
static struct {

  FILE *f;
  bool print_group[MAX_LAYER];
  bool print_layer[MAX_LAYER];

  const char *filename;

  LayerType *outline_layer;

  HID_Attr_Val step_values[NUM_OPTIONS];

  bool is_mask;
  bool is_drill;
  bool is_assy;
  bool is_copper;
  bool is_paste;

  int next_identifier;
} global;

static HID_Attribute *
step_get_export_options (int *n)
{
  static char *last_made_filename = 0;
  if (PCB)
    derive_default_filename(PCB->Filename, &step_attribute_list[HA_stepfile], ".step", &last_made_filename);

  if (n)
    *n = NUM_OPTIONS;
  return step_attribute_list;
}

static int
group_for_layer (int l)
{
  if (l < max_copper_layer + 2 && l >= 0)
    return GetLayerGroupNumberByNumber (l);
  /* else something unique */
  return max_group + 3 + l;
}

static int
layer_sort (const void *va, const void *vb)
{
  int a = *(int *) va;
  int b = *(int *) vb;
  int d = group_for_layer (b) - group_for_layer (a);
  if (d)
    return d;
  return b - a;
}

void
step_start_file (FILE *f, const char *filename)
{
  time_t currenttime = time (NULL);
  struct tm utc;

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
              "test_pcb_id", "test_pcb_name2", "test_pcb_description");

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

  global.next_identifier = 21;
}

static int
get_contour_npoints (PLINE *contour)
{
  return 5;
}

static void
get_contour_coord_n_in_mm (PLINE *contour, int n, double *x, double *y)
{
  /* TESTING ONLY */
  /* A simple five sided shape */
  int point_coords[5][2] = {{0, 0},
                            {10, 0},
                            {10, 10},
                            {5, 15},
                            {0, 10}};

  *x = point_coords[n % 5][0];
  *y = point_coords[n % 5][1];
}

#define FWD 1
#define REV 2
static void
step_emit_board_contour (FILE *f, PLINE *contour)
{

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

  int bottom_face_identifier;
  int top_face_identifier;
  int *side_face_identifier;

  int pcb_shell_identifier;

  int i;

  npoints = get_contour_npoints (contour);

  /* TODO: Avoid needing to store these identifiers by nailing down our usage pattern of identifiers */
  /* Allocate some storage for identifiers */

            side_plane_identifier = g_malloc (sizeof (int) * npoints);
  bottom_infinite_line_identifier = g_malloc (sizeof (int) * npoints);
     top_infinite_line_identifier = g_malloc (sizeof (int) * npoints);
    side_infinite_line_identifier = g_malloc (sizeof (int) * npoints);
         bottom_vertex_identifier = g_malloc (sizeof (int) * npoints);
            top_vertex_identifier = g_malloc (sizeof (int) * npoints);
           bottom_edge_identifier = g_malloc (sizeof (int) * npoints);
              top_edge_identifier = g_malloc (sizeof (int) * npoints);
             side_edge_identifier = g_malloc (sizeof (int) * npoints);
             side_face_identifier = g_malloc (sizeof (int) * npoints);

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
  brep_identifier = global.next_identifier++;

  /* Define the bottom and top planes */
  fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
              "#%i = DIRECTION ( 'NONE', (  0.0,  0.0,  1.0 ) ) ; "
              "#%i = DIRECTION ( 'NONE', (  1.0,  0.0,  0.0 ) ) ; "
              "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i, #%i, #%i ) ; "
              "#%i = PLANE ( 'NONE',  #%i ) ;\n",
           global.next_identifier, 0.0, 0.0, -COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
           global.next_identifier + 1,
           global.next_identifier + 2,
           global.next_identifier + 3, global.next_identifier, global.next_identifier + 1, global.next_identifier + 2,
           global.next_identifier + 4, global.next_identifier + 3);
  bottom_plane_identifier = global.next_identifier + 4;
  global.next_identifier = global.next_identifier + 5;

  fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
              "#%i = DIRECTION ( 'NONE', (  0.0,  0.0, -1.0 ) ) ; "
              "#%i = DIRECTION ( 'NONE', ( -1.0,  0.0,  0.0 ) ) ; "
              "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i, #%i, #%i ) ; "
              "#%i = PLANE ( 'NONE',  #%i ) ;\n",
           global.next_identifier, 0.0, 0.0, COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
           global.next_identifier + 1,
           global.next_identifier + 2,
           global.next_identifier + 3, global.next_identifier, global.next_identifier + 1, global.next_identifier + 2,
           global.next_identifier + 4, global.next_identifier + 3);
  top_plane_identifier = global.next_identifier + 4;
  global.next_identifier = global.next_identifier + 5;

  /* Define the side planes */
  for (i = 0; i < npoints; i++) {
    double x1, y1, x2, y2;

    get_contour_coord_n_in_mm (contour, i,     &x1, &y1);
    get_contour_coord_n_in_mm (contour, i + 1, &x2, &y2);

    fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                "#%i = AXIS2_PLACEMENT_3D ( 'NONE', #%i, #%i, #%i ) ; "
                "#%i = PLANE ( 'NONE',  #%i ) ;\n",
             global.next_identifier,     /* A point on the plane                      */ x1, y1, 0.0,
             global.next_identifier + 1, /* An axis direction pointing into the shape */ -(y2 - y1), (x2 - x1), 0.0, // <--- NOT SURE IF I NEED TO NORMALISE THIS, OR FLIP THE DIRECTION??
             global.next_identifier + 2, /* A reference direction pointing.. "meh"?   */ 0.0, 0.0, 1.0,
             global.next_identifier + 3, global.next_identifier, global.next_identifier + 1, global.next_identifier + 2,
             global.next_identifier + 4, global.next_identifier + 3);
    side_plane_identifier[i] = global.next_identifier + 4;
    global.next_identifier = global.next_identifier + 5;
  }

  /* Define the infinite lines */
  for (i = 0; i < npoints; i++) {
    double x1, y1, x2, y2;

    get_contour_coord_n_in_mm (contour, i,     &x1, &y1);
    get_contour_coord_n_in_mm (contour, i + 1, &x2, &y2);

    /* Bottom */
    fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                "#%i = VECTOR ( 'NONE', #%i, 1000.0 ) ; "
                "#%i = LINE ( 'NONE', #%i, #%i ) ;\n",
             global.next_identifier,     /* A point on the line         */ x1, y1, -COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
             global.next_identifier + 1, /* A direction along the line  */ -(x2 - x1), -(y2 - y1), 0.0, // <--- NOT SURE IF I NEED TO NORMALISE THIS, OR FLIP THE DIRECTION??
             global.next_identifier + 2, global.next_identifier + 1,
             global.next_identifier + 3, global.next_identifier, global.next_identifier + 2);
    bottom_infinite_line_identifier[i] = global.next_identifier + 3;
    global.next_identifier = global.next_identifier + 4;

    /* Top */
    fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                "#%i = VECTOR ( 'NONE', #%i, 1000.0 ) ; "
                "#%i = LINE ( 'NONE', #%i, #%i ) ;\n",
             global.next_identifier,     /* A point on the line         */ x1, y1, COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
             global.next_identifier + 1, /* A direction along the line  */ (x2 - x1), (y2 - y1), 0.0, // <--- NOT SURE IF I NEED TO NORMALISE THIS, OR FLIP THE DIRECTION??
             global.next_identifier + 2, global.next_identifier + 1,
             global.next_identifier + 3, global.next_identifier, global.next_identifier + 2);
    top_infinite_line_identifier[i] = global.next_identifier + 3;
    global.next_identifier = global.next_identifier + 4;

    /* Side */
    fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                "#%i =       DIRECTION ( 'NONE', ( %f, %f, %f )) ; "
                "#%i = VECTOR ( 'NONE', #%i, 1000.0 ) ; "
                "#%i = LINE ( 'NONE', #%i, #%i ) ;\n",
             global.next_identifier,     /* A point on the line         */ x1, y1, -COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
             global.next_identifier + 1, /* A direction along the line  */ 0.0, 0.0, 1.0, // <--- NOT SURE IF I NEED TO NORMALISE THIS, OR FLIP THE DIRECTION??
             global.next_identifier + 2, global.next_identifier + 1,
             global.next_identifier + 3, global.next_identifier, global.next_identifier + 2);
    side_infinite_line_identifier[i] = global.next_identifier + 3;
    global.next_identifier = global.next_identifier + 4;
  }

  /* Define the vertices */
  for (i = 0; i < npoints; i++) {
    double x1, y1;

    get_contour_coord_n_in_mm (contour, i, &x1, &y1);

    /* Bottom */
    fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                "#%i = VERTEX_POINT ( 'NONE', #%i ) ;\n",
             global.next_identifier,     /* Vertex coordinate  */ x1, y1, -COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
             global.next_identifier + 1, global.next_identifier);
    bottom_vertex_identifier[i] = global.next_identifier + 1;
    global.next_identifier = global.next_identifier + 2;

    /* Top */
    fprintf (f, "#%i = CARTESIAN_POINT ( 'NONE', ( %f, %f, %f )) ; "
                "#%i = VERTEX_POINT ( 'NONE', #%i ) ;\n",
             global.next_identifier,     /* Vertex coordinate  */ x1, y1, COORD_TO_MM (HACK_BOARD_THICKNESS) / 2.0,
             global.next_identifier + 1, global.next_identifier);
    top_vertex_identifier[i] = global.next_identifier + 1;
    global.next_identifier = global.next_identifier + 2;
  }

  /* Define the Edges */
  for (i = 0; i < npoints; i++) {

    /* Bottom */
    fprintf (f, "#%i = EDGE_CURVE ( 'NONE', #%i, #%i, #%i,   .T. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .F. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .T. ) ;\n",
             global.next_identifier, bottom_vertex_identifier[i], bottom_vertex_identifier[(i + 1) % npoints], bottom_infinite_line_identifier[i],      // <-- MIGHT NEED TO REVERSE THIS???
             global.next_identifier + 1, global.next_identifier,
             global.next_identifier + 2, global.next_identifier);
    bottom_edge_identifier[i] = global.next_identifier; /* Add 1 for same oriented, add 2 for back oriented */
    global.next_identifier = global.next_identifier + 3;

    /* Top */
    fprintf (f, "#%i = EDGE_CURVE ( 'NONE', #%i, #%i, #%i,   .T. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .F. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .T. ) ;\n",
             global.next_identifier, top_vertex_identifier[i], top_vertex_identifier[(i + 1) % npoints], top_infinite_line_identifier[i],                 // <-- MIGHT NEED TO REVERSE THIS???
             global.next_identifier + 1, global.next_identifier,
             global.next_identifier + 2, global.next_identifier);
    top_edge_identifier[i] = global.next_identifier; /* Add 1 for same oriented, add 2 for back oriented */
    global.next_identifier = global.next_identifier + 3;

    /* Side */
    fprintf (f, "#%i = EDGE_CURVE ( 'NONE', #%i, #%i, #%i,   .T. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .F. ) ; "
                "#%i = ORIENTED_EDGE ( 'NONE', *, *, #%i, .T. ) ;\n",
             global.next_identifier, bottom_vertex_identifier[i], top_vertex_identifier[i], side_infinite_line_identifier[i],
             global.next_identifier + 1, global.next_identifier,
             global.next_identifier + 2, global.next_identifier);
    side_edge_identifier[i] = global.next_identifier; /* Add 1 for same oriented, add 2 for back oriented */
    global.next_identifier = global.next_identifier + 3;
  }

  /* Define the faces */

  /* Bottom */
  fprintf (f, "#%i = EDGE_LOOP ( 'NONE', ( ",
           global.next_identifier);
  for (i = 0; i < npoints - 1; i++)
    fprintf (f, "#%i, ", bottom_edge_identifier[i] + REV);
  fprintf (f, "#%i ) ) ; "
              "#%i = FACE_OUTER_BOUND ( 'NONE', #%i, .T. ) ; "
              "#%i = ADVANCED_FACE ( 'NONE', ( #%i ), #%i, .F. ) ;\n",
           bottom_edge_identifier[npoints - 1] + REV,
           global.next_identifier + 1, global.next_identifier,
           global.next_identifier + 2, global.next_identifier + 1, bottom_plane_identifier);
  bottom_face_identifier = global.next_identifier + 2;
  global.next_identifier = global.next_identifier + 3;

  /* Top */
  fprintf (f, "#%i = EDGE_LOOP ( 'NONE', ( ",
           global.next_identifier);
  for (i = npoints - 1; i > 0; i--)
    fprintf (f, "#%i, ", top_edge_identifier[i] + FWD);
  fprintf (f, "#%i ) ) ; "
              "#%i = FACE_OUTER_BOUND ( 'NONE', #%i, .T. ) ; "
              "#%i = ADVANCED_FACE ( 'NONE', ( #%i ), #%i, .F. ) ;\n",
           top_edge_identifier[0] + FWD,
           global.next_identifier + 1, global.next_identifier,
           global.next_identifier + 2, global.next_identifier + 1, top_plane_identifier);
  top_face_identifier = global.next_identifier + 2;
  global.next_identifier = global.next_identifier + 3;

  /* Sides */
  for (i = 0; i < npoints; i++) {
    fprintf (f, "#%i = EDGE_LOOP ( 'NONE', ( #%i, #%i, #%i, #%i ) ) ; "
                "#%i = FACE_OUTER_BOUND ( 'NONE', #%i, .T. ) ; "
                "#%i = ADVANCED_FACE ( 'NONE', ( #%i ), #%i, .F. ) ;\n",
             global.next_identifier, bottom_edge_identifier[i] + FWD, side_edge_identifier[i] + REV, top_edge_identifier[i] + REV, side_edge_identifier[(i + 1) % npoints] + FWD,
             global.next_identifier + 1, global.next_identifier,
             global.next_identifier + 2, global.next_identifier + 1, side_plane_identifier[i]);
    side_face_identifier[i] = global.next_identifier + 2;
    global.next_identifier = global.next_identifier + 3;
  }


  /* Closed shell which bounds the brep solid */
  pcb_shell_identifier = global.next_identifier;
  global.next_identifier++;
  fprintf (f, "#%i = CLOSED_SHELL ( 'NONE', ( #%i, #%i, ", pcb_shell_identifier, bottom_face_identifier, top_face_identifier);
  for (i = 0; i < npoints - 1; i++) {
    fprintf (f, "#%i, ", side_face_identifier[i]);
  }
  fprintf (f, "#%i) ) ;\n",
           side_face_identifier[npoints - 1]);

  /* Finally emit the brep solid definition */
  fprintf (f, "#%i = MANIFOLD_SOLID_BREP ( 'PCB outline', #%i ) ;\n", brep_identifier, pcb_shell_identifier);

  g_free (side_plane_identifier);
  g_free (bottom_infinite_line_identifier);
  g_free (top_infinite_line_identifier);
  g_free (side_infinite_line_identifier);
  g_free (bottom_vertex_identifier);
  g_free (top_vertex_identifier);
  g_free (bottom_edge_identifier);
  g_free (top_edge_identifier);
  g_free (side_edge_identifier);
  g_free (side_face_identifier);
}
#undef FWD
#undef REV

static void
step_end_file (FILE *f)
{
  fprintf (f, "ENDSEC;\n" );
  fprintf (f, "END-ISO-10303-21;\n" );
}

static void
step_hid_export_to_file (FILE * the_file, HID_Attr_Val * options)
{
  int i;
  static int saved_layer_stack[MAX_LAYER];
  FlagType save_thindraw;

  save_thindraw = PCB->Flags;
  CLEAR_FLAG(THINDRAWFLAG, PCB);
  CLEAR_FLAG(THINDRAWPOLYFLAG, PCB);
  CLEAR_FLAG(CHECKPLANESFLAG, PCB);

  global.f = the_file;

  step_start_file (global.f, global.filename);
  step_emit_board_contour (global.f, NULL); /* TEST */

  memset (global.print_group, 0, sizeof (global.print_group));
  memset (global.print_layer, 0, sizeof (global.print_layer));

  global.outline_layer = NULL;

  for (i = 0; i < max_copper_layer; i++)
    {
      LayerType *layer = PCB->Data->Layer + i;
      if (layer->LineN || layer->TextN || layer->ArcN || layer->PolygonN)
        global.print_group[GetLayerGroupNumberByNumber (i)] = 1;

      if (strcmp (layer->Name, "outline") == 0 ||
          strcmp (layer->Name, "route") == 0)
        {
          global.outline_layer = layer;
        }
    }
  global.print_group[GetLayerGroupNumberByNumber (solder_silk_layer)] = 1;
  global.print_group[GetLayerGroupNumberByNumber (component_silk_layer)] = 1;
  for (i = 0; i < max_copper_layer; i++)
    if (global.print_group[GetLayerGroupNumberByNumber (i)])
      global.print_layer[i] = 1;

  memcpy (saved_layer_stack, LayerStack, sizeof (LayerStack));
  qsort (LayerStack, max_copper_layer, sizeof (LayerStack[0]), layer_sort);

  /* reset static vars */
  step_set_layer (NULL, 0, -1);
  use_gc (NULL);

  hid_expose_callback (&step_hid, NULL, 0);

  step_set_layer (NULL, 0, -1);  /* reset static vars */
  hid_expose_callback (&step_hid, NULL, 0);

  memcpy (LayerStack, saved_layer_stack, sizeof (LayerStack));
  PCB->Flags = save_thindraw;
}

static void
step_do_export (HID_Attr_Val * options)
{
  FILE *fh;
  int save_ons[MAX_LAYER + 2];
  int i;

  if (!options)
    {
      step_get_export_options (0);
      for (i = 0; i < NUM_OPTIONS; i++)
        global.step_values[i] = step_attribute_list[i].default_val;
      options = global.step_values;
    }

  global.filename = options[HA_stepfile].str_value;
  if (!global.filename)
    global.filename = "pcb-out.step";

  fh = fopen (global.filename, "w");
  if (fh == NULL)
    {
      perror (global.filename);
      return;
    }

  hid_save_and_show_layer_ons (save_ons);
  step_hid_export_to_file (fh, options);
  hid_restore_layer_ons (save_ons);

  step_end_file (fh);
  fclose (fh);
}

static void
step_parse_arguments (int *argc, char ***argv)
{
  hid_register_attributes (step_attribute_list, NUM_OPTIONS);
  hid_parse_command_line (argc, argv);
}

static int
step_set_layer (const char *name, int group, int empty)
{
  static int lastgroup = -1;
  int idx = (group >= 0 && group < max_group)
            ? PCB->LayerGroups.Entries[group][0]
            : group;
  if (name == 0)
    name = PCB->Data->Layer[idx].Name;

  if (empty == -1)
    lastgroup = -1;
  if (empty)
    return 0;

  if (idx >= 0 && idx < max_copper_layer && !global.print_layer[idx])
    return 0;

  if (strcmp (name, "invisible") == 0)
    return 0;

  global.is_drill = (SL_TYPE (idx) == SL_PDRILL || SL_TYPE (idx) == SL_UDRILL);
  global.is_mask  = (SL_TYPE (idx) == SL_MASK);
  global.is_assy  = (SL_TYPE (idx) == SL_ASSY);
  global.is_copper = (SL_TYPE (idx) == 0);
  global.is_paste  = (SL_TYPE (idx) == SL_PASTE);

  if (group < 0 || group != lastgroup)
    {
      lastgroup = group;

      use_gc (NULL);  /* reset static vars */
    }

  /* If we're printing a copper layer other than the outline layer,
     and we want to "print outlines", and we have an outline layer,
     print the outline layer on this layer also.  */
  if (global.is_copper &&
      global.outline_layer != NULL &&
      global.outline_layer != PCB->Data->Layer+idx &&
      strcmp (name, "outline") != 0 &&
      strcmp (name, "route") != 0
      )
    {
      DrawLayer (global.outline_layer, NULL);
    }

  return 1;
}

static hidGC
step_make_gc (void)
{
  hidGC rv = (hidGC) calloc (1, sizeof (hid_gc_struct));
  rv->me_pointer = &step_hid;
  rv->cap = Trace_Cap;
  return rv;
}

static void
step_destroy_gc (hidGC gc)
{
  free (gc);
}

static void
step_use_mask (enum mask_mode mode)
{
  /* does nothing */
}

static void
step_set_color (hidGC gc, const char *name)
{
  if (strcmp (name, "erase") == 0 || strcmp (name, "drill") == 0)
    {
      gc->r = gc->g = gc->b = 255;
      gc->erase = 1;
    }
  else
    {
      int r, g, b;
      sscanf (name + 1, "%02x%02x%02x", &r, &g, &b);
      gc->r = r;
      gc->g = g;
      gc->b = b;
      gc->erase = 0;
    }
}

static void
step_set_line_cap (hidGC gc, EndCapStyle style)
{
  gc->cap = style;
}

static void
step_set_line_width (hidGC gc, Coord width)
{
  gc->width = width;
}

static void
step_set_draw_xor (hidGC gc, int xor_)
{
}

static void
step_set_draw_faded (hidGC gc, int faded)
{
}

static void
use_gc (hidGC gc)
{
  static int lastcap = -1;
  static int lastcolor = -1;

  if (gc == NULL)
    {
      lastcap = lastcolor = -1;
      return;
    }
  if (gc->me_pointer != &step_hid)
    {
      fprintf (stderr, "Fatal: GC from another HID passed to step HID\n");
      abort ();
    }
  if (lastcap != gc->cap)
    {
      fprintf (global.f, "%%d setlinecap %%d setlinejoin\n");
      lastcap = gc->cap;
    }
#define CBLEND(gc) (((gc->r)<<24)|((gc->g)<<16)|((gc->b)<<8))
  if (lastcolor != CBLEND (gc))
    {
      double r, g, b;
      r = gc->r;
      g = gc->g;
      b = gc->b;
      if (gc->r == gc->g && gc->g == gc->b)
        fprintf (global.f, "%g gray\n", r / 255.0);
      else
        fprintf (global.f, "%g %g %g rgb\n", r / 255.0, g / 255.0, b / 255.0);
      lastcolor = CBLEND (gc);
    }
}

static void
step_draw_rect (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2)
{
}

static void
step_draw_line (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2)
{
}

static void
step_draw_arc (hidGC gc, Coord cx, Coord cy, Coord width, Coord height,
               Angle start_angle, Angle delta_angle)
{
}

static void
step_fill_circle (hidGC gc, Coord cx, Coord cy, Coord radius)
{
}

static void
step_fill_polygon (hidGC gc, int n_coords, Coord *x, Coord *y)
{
}

static void
fill_polyarea (hidGC gc, POLYAREA * pa, const BoxType * clip_box)
{
}

static void
step_draw_pcb_polygon (hidGC gc, PolygonType * poly, const BoxType * clip_box)
{
  fill_polyarea (gc, poly->Clipped, clip_box);
  if (TEST_FLAG (FULLPOLYFLAG, poly))
    {
      POLYAREA *pa;

      for (pa = poly->Clipped->f; pa != poly->Clipped; pa = pa->f)
        fill_polyarea (gc, pa, clip_box);
    }
}

static void
step_fill_rect (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2)
{
}

static void
step_set_crosshair (int x, int y, int action)
{
}

#include "dolists.h"

HID step_hid;
static HID_DRAW step_graphics;

void step_step_init (HID *hid)
{
  hid->get_export_options = step_get_export_options;
  hid->do_export          = step_do_export;
  hid->parse_arguments    = step_parse_arguments;
  hid->set_layer          = step_set_layer;
  hid->set_crosshair      = step_set_crosshair;
}

void step_step_graphics_init (HID_DRAW *graphics)
{
  graphics->make_gc            = step_make_gc;
  graphics->destroy_gc         = step_destroy_gc;
  graphics->use_mask           = step_use_mask;
  graphics->set_color          = step_set_color;
  graphics->set_line_cap       = step_set_line_cap;
  graphics->set_line_width     = step_set_line_width;
  graphics->set_draw_xor       = step_set_draw_xor;
  graphics->set_draw_faded     = step_set_draw_faded;
  graphics->draw_line          = step_draw_line;
  graphics->draw_arc           = step_draw_arc;
  graphics->draw_rect          = step_draw_rect;
  graphics->fill_circle        = step_fill_circle;
  graphics->fill_polygon       = step_fill_polygon;
  graphics->fill_rect          = step_fill_rect;

  graphics->draw_pcb_polygon   = step_draw_pcb_polygon;
}

void
hid_step_init ()
{
  memset (&step_hid, 0, sizeof (HID));
  memset (&step_graphics, 0, sizeof (HID_DRAW));

  common_nogui_init (&step_hid);
  common_draw_helpers_init (&step_graphics);
  step_step_init (&step_hid);
  step_step_graphics_init (&step_graphics);

  step_hid.struct_size        = sizeof (HID);
  step_hid.name               = "step";
  step_hid.description        = "STEP AP214 export";
  step_hid.exporter           = 1;
  step_hid.poly_before        = 1;

  step_hid.graphics           = &step_graphics;

  hid_register_hid (&step_hid);

#include "step_lists.h"
}
