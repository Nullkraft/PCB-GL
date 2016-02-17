#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include <glib.h>

#include "hid/common/step_id.h"
#include "hid/common/quad.h"
#include "hid/common/vertex3d.h"
#include "hid/common/contour3d.h"
#include "hid/common/appearance.h"
#include "hid/common/face3d.h"
#include "hid/common/edge3d.h"
#include "hid/common/object3d.h"
#include "polygon.h"
#include "data.h"

#include "step_writer.h"

#include "pcb-printf.h"

#include "object3d_step.h"


static step_id_list
presentation_style_assignments_from_appearance (step_file *step, appearance *appear)
{
  step_id colour = step_colour_rgb (step, "", appear->r, appear->g, appear->b);
  step_id fill_area_style = step_fill_area_style (step, "", make_step_id_list (1, step_fill_area_style_colour (step, "", colour)));
  step_id surface_side_style = step_surface_side_style (step, "", make_step_id_list (1, step_surface_style_fill_area (step, fill_area_style)));
  step_id_list styles_list = make_step_id_list (1, step_surface_style_usage (step, "BOTH", surface_side_style));
  step_id_list psa_list = make_step_id_list (1, step_presentation_style_assignment (step, styles_list));

  return psa_list;
}

void
object3d_export_to_step (object3d *object, char *filename)
{
  FILE *f;
  step_file *step;
  time_t currenttime;
  struct tm utc;
  int geometric_representation_context_identifier;
  int shape_representation_identifier;
  int brep_identifier;
  int pcb_shell_identifier;
  int brep_style_identifier;
  GList *styled_item_identifiers = NULL;
  GList *shell_face_list = NULL;
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

  step = step_output_file (f);

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
  fprintf (f, "#10 =    CARTESIAN_POINT ( 'NONE', ( 0.0, 0.0, 0.0 ) ) ;\n"
              "#11 =          DIRECTION ( 'NONE', ( 0.0, 0.0, 1.0 ) ) ;\n"
              "#12 =          DIRECTION ( 'NONE', ( 1.0, 0.0, 0.0 ) ) ;\n"
              "#13 = AXIS2_PLACEMENT_3D ( 'NONE', #10, #11, #12 ) ;\n");

  /* Grr.. more boilerplate - this time unit definitions */

  fprintf (f, "#14 = UNCERTAINTY_MEASURE_WITH_UNIT (LENGTH_MEASURE( 1.0E-005 ), #15, 'distance_accuracy_value', 'NONE');\n"
              "#15 =( LENGTH_UNIT ( ) NAMED_UNIT ( * ) SI_UNIT ( .MILLI., .METRE. ) );\n"
              "#16 =( NAMED_UNIT ( * ) PLANE_ANGLE_UNIT ( ) SI_UNIT ( $, .RADIAN. ) );\n"
              "#17 =( NAMED_UNIT ( * ) SI_UNIT ( $, .STERADIAN. ) SOLID_ANGLE_UNIT ( ) );\n"
              "#18 =( GEOMETRIC_REPRESENTATION_CONTEXT ( 3 ) GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT ( ( #14 ) ) GLOBAL_UNIT_ASSIGNED_CONTEXT ( ( #15, #16, #17 ) ) REPRESENTATION_CONTEXT ( 'NONE', 'WORKASPACE' ) );\n");
  geometric_representation_context_identifier = 18;
  step->next_id = 19;

#define FWD 1
#define REV 2
#define ORIENTED_EDGE_IDENTIFIER(e) (((edge_info *)UNDIR_DATA (e))->edge_identifier + ((e & 2) ? REV : FWD))

  /* Define ininite planes corresponding to every planar face, and cylindrical surfaces for every cylindrical face */
  for (face_iter = object->faces; face_iter != NULL; face_iter = g_list_next (face_iter)) {
    face3d *face = face_iter->data;

    if (face->is_cylindrical) {
      /* CYLINDRICAL SURFACE NORMAL POINTS OUTWARDS AWAY FROM ITS AXIS.
       * face->surface_orientation_reversed NEEDS TO BE SET FOR HOLES IN THE SOLID
       */
      face->surface_identifier =
        step_cylindrical_surface (step, "NONE",
                                  step_axis2_placement_3d (step, "NONE",
                                                           step_cartesian_point (step, "NONE", face->cx, face->cy, face->cz),
                                                                 step_direction (step, "NONE", face->ax, face->ay, face->az),
                                                                 step_direction (step, "NONE", face->nx, face->ny, face->nz)),
                                  face->radius);
    } else {
      contour3d *outer_contour = face->contours->data;
      vertex3d *ov = ODATA (outer_contour->first_edge);
      vertex3d *dv = DDATA (outer_contour->first_edge);

      double rx, ry, rz;

      rx = dv->x - ov->x;
      ry = dv->y - ov->y;
      rz = dv->z - ov->z;

      /* Catch the circular face case where the start and end vertices are identical */
      if (rx < EPSILON && -rx < EPSILON &&
          ry < EPSILON && -ry < EPSILON &&
          rz < EPSILON && -rz < EPSILON) {
        rx = 1., ry = 0., rz = 0.;
      }

      face->surface_identifier =
        step_plane (step, "NONE",
                    step_axis2_placement_3d (step, "NONE",
                                             step_cartesian_point (step, "NONE", ov->x,  /* A point on the plane. Defines 0,0 of the plane's parameterised coords. */ 
                                                                                 ov->y,      /* Set this to the origin vertex of the first edge */
                                                                                 ov->z),     /* this contour links to in the quad edge structure. */
                                                   step_direction (step, "NONE", face->nx, face->ny, face->nz), /* An axis direction normal to the the face - Gives z-axis */
                                                   step_direction (step, "NONE", rx,     /* Reference x-axis, orthogonal to z-axis. */
                                                                                 ry,         /* Define this to be along the first edge this */
                                                                                 rz)));      /* contour links to in the quad edge structure */
    }
  }

  /* Define the infinite lines corresponding to every edge (either lines or circles)*/
  for (edge_iter = object->edges; edge_iter != NULL; edge_iter = g_list_next (edge_iter)) {
    edge_ref edge = (edge_ref)edge_iter->data;
    edge_info *info = UNDIR_DATA (edge);

    if (info->is_round) {
      info->infinite_line_identifier =
        step_circle (step, "NONE",
                     step_axis2_placement_3d (step, "NONE",
                                              step_cartesian_point (step, "NONE", info->cx, info->cy, info->cz),  // <--- Center of the circle
                                                    step_direction (step, "NONE", info->nx, info->ny, info->nz),  // <--- Normal of the circle
                                                    step_direction (step, "NONE", -1.0,     0.0,      0.0)),      // <--- Approximate X-axis direction of placement /* XXX: PULL FROM FACE DATA */
                                                    info->radius);
    } else {
      vertex3d *ov = ODATA (edge);
      vertex3d *dv = DDATA (edge);

      double dir_x, dir_y, dir_z;

      dir_x = dv->x - ov->x;
      dir_y = dv->y - ov->y;
      dir_z = dv->z - ov->z;

#if 1
      /* XXX: This avoids the test file step_outline_test.pcb failing to display properly in freecad when coordinates are slightly rounded */
      if (dir_x < EPSILON && -dir_x < EPSILON &&
          dir_y < EPSILON && -dir_y < EPSILON &&
          dir_z < EPSILON && -dir_z < EPSILON) {
        printf ("EDGE TOO SHORT TO DETERMINE DIRECTION - GUESSING! Coords (%f, %f)\n", ov->x, ov->y);
        pcb_printf ("Approx PCB coords of short edge: %#mr, %#mr\n", (Coord)STEP_X_TO_COORD (PCB, ov->x), (Coord)STEP_Y_TO_COORD (PCB, ov->y));
        dir_x = 1.0; /* DUMMY TO AVOID A ZERO LENGTH DIRECTION VECTOR */
      }
#endif

      info->infinite_line_identifier =
        step_line (step, "NONE",
                   step_cartesian_point (step, "NONE", ov->x, ov->y, ov->z),  // <--- A point on the line (the origin vertex)
                   step_vector (step, "NONE",
                                step_direction (step, "NONE", dir_x, dir_y, dir_z), // <--- Direction along the line
                                1000.0));     // <--- Arbitrary length in this direction for the parameterised coordinate "1".

    }
  }

  /* Define the vertices */
  for (vertex_iter = object->vertices; vertex_iter != NULL; vertex_iter = g_list_next (vertex_iter)) {
    vertex3d *vertex = vertex_iter->data;

    vertex->vertex_identifier =
      step_vertex_point (step, "NONE", step_cartesian_point (step, "NONE", vertex->x, vertex->y, vertex->z));
  }

  /* Define the Edges */
  for (edge_iter = object->edges; edge_iter != NULL; edge_iter = g_list_next (edge_iter)) {
    edge_ref edge = (edge_ref)edge_iter->data;
    edge_info *info = UNDIR_DATA (edge);
    step_id sv = ((vertex3d *)ODATA (edge))->vertex_identifier;
    step_id ev = ((vertex3d *)DDATA (edge))->vertex_identifier;

    /* XXX: The lookup of these edges by adding to info->edge_identifier requires the step_* functions to assign sequential identifiers */
    info->edge_identifier = step_edge_curve (step, "NONE", sv, ev, info->infinite_line_identifier, true);
    step_oriented_edge (step, "NONE", info->edge_identifier, true);  /* Add 1 to info->edge_identifier to find this (same) oriented edge */
    step_oriented_edge (step, "NONE", info->edge_identifier, false); /* Add 2 to info->edge_identifier to find this (back) oriented edge */
  }

  /* Define the faces */
  for (face_iter = object->faces; face_iter != NULL; face_iter = g_list_next (face_iter)) {
    face3d *face = face_iter->data;
    bool outer_contour = true;
    step_id_list face_contour_list = NULL;

    for (contour_iter = face->contours;
         contour_iter != NULL;
         contour_iter = g_list_next (contour_iter), outer_contour = false) {
      contour3d *contour = contour_iter->data;
      edge_ref edge;
      step_id edge_loop;
      step_id_list edge_loop_edges = NULL;

      edge = contour->first_edge;
      do {
        edge_loop_edges = g_list_append (edge_loop_edges, GINT_TO_POINTER (ORIENTED_EDGE_IDENTIFIER (edge)));
      } while (edge = LNEXT (edge), edge != contour->first_edge);

      edge_loop = step_edge_loop (step, "NONE", edge_loop_edges);

      if (outer_contour)
        contour->face_bound_identifier = step_face_outer_bound (step, "NONE", edge_loop, true);
      else
        contour->face_bound_identifier = step_face_bound (step, "NONE", edge_loop, true);

      face_contour_list = g_list_append (face_contour_list, GINT_TO_POINTER (contour->face_bound_identifier));
    }

    face->face_identifier = step_advanced_face (step, "NONE", face_contour_list, face->surface_identifier, !face->surface_orientation_reversed);
    shell_face_list = g_list_append (shell_face_list, GINT_TO_POINTER (face->face_identifier));
  }

  /* Closed shell which bounds the brep solid */
  pcb_shell_identifier = step_closed_shell (step, "NONE", shell_face_list);
  brep_identifier = step_manifold_solid_brep (step, "PCB outline", pcb_shell_identifier);

#if 1
  /* Body style */
  /* XXX: THERE MUST BE A BODY STYLE, CERTAINLY IF WE WANT TO OVER RIDE FACE COLOURS */
  brep_style_identifier = step_styled_item (step, "NONE", presentation_style_assignments_from_appearance (step, object->appear), brep_identifier);
  step_presentation_layer_assignment (step, "1", "Layer 1", make_step_id_list (1, brep_style_identifier));

  styled_item_identifiers = g_list_append (styled_item_identifiers, GINT_TO_POINTER (brep_style_identifier));

  /* Face styles */
  for (face_iter = object->faces; face_iter != NULL; face_iter = g_list_next (face_iter)) {
    face3d *face = face_iter->data;

    if (face->appear != NULL) {
      step_id orsi = step_over_riding_styled_item (step, "NONE",
                                                   presentation_style_assignments_from_appearance (step, face->appear),
                                                   face->face_identifier, brep_style_identifier);
      styled_item_identifiers = g_list_append (styled_item_identifiers, GINT_TO_POINTER (orsi));
    }
  }

  /* Emit references to the styled and over_ridden styled items */
  step_mechanical_design_geometric_presentation_representation (step, "", styled_item_identifiers, geometric_representation_context_identifier);
#endif

  shape_representation_identifier =
    step_advanced_brep_shape_representation (step, "test_pcb_absr_name",
                                             make_step_id_list (2, brep_identifier, 13 /* XXX */), geometric_representation_context_identifier);

  step_shape_definition_representation (step, 9 /* XXX */, shape_representation_identifier);

#undef ORIENTED_EDGE_IDENTIFIER
#undef FWD
#undef REV

  fprintf (f, "ENDSEC;\n" );
  fprintf (f, "END-ISO-10303-21;\n" );

  fclose (f);
}

>>>>>>> patched
GList *
object3d_from_board_outline (void)
{
  GList *board_objects = NULL;
  object3d *board_object;
  appearance *board_appearance;
  appearance *top_bot_appearance;
  POLYAREA *board_outline;
  POLYAREA *pa;
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

  board_outline = board_outline_poly (true);

  /* Loop over all board outline pieces */
  pa = board_outline;
  do {

    contour = pa->contours;
    ncontours = 0;
    npoints = 0;

    ct = contour;
    while (ct != NULL) {
      ncontours ++;
      npoints += get_contour_npoints (ct);
      ct = ct->next;
    }

    board_object = make_object3d (PCB->Name);
    board_appearance = make_appearance ();
    top_bot_appearance = make_appearance ();
    appearance_set_color (board_appearance,   1.0, 1.0, 0.6);
    appearance_set_color (top_bot_appearance, 0.2, 0.8, 0.2);

    object3d_set_appearance (board_object, board_appearance);

    vertices = malloc (sizeof (vertex3d *) * 2 * npoints); /* (n-bottom, n-top) */
    edges    = malloc (sizeof (edge_ref  ) * 3 * npoints); /* (n-bottom, n-top, n-sides) */
    faces    = malloc (sizeof (face3d *) * (npoints + 2)); /* (n-sides, 1-bottom, 1-top */

    /* Define the vertices */
    ct = contour;
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

      get_contour_coord_n_in_step_mm (ct, offset_in_ct, &x1, &y1);
      vertices[i]           = make_vertex3d (x1, y1, -COORD_TO_STEP_Z (PCB, HACK_BOARD_THICKNESS)); /* Bottom */
      vertices[npoints + i] = make_vertex3d (x1, y1, 0);                                            /* Top */

      object3d_add_vertex (board_object, vertices[i]);
      object3d_add_vertex (board_object, vertices[npoints + i]);
    }

    /* Define the edges */
    for (i = 0; i < 3 * npoints; i++) {
      edges[i] = make_edge ();
      UNDIR_DATA (edges[i]) = make_edge_info ();
      object3d_add_edge (board_object, edges[i]);
    }

    /* Define the faces */
    for (i = 0; i < npoints; i++) {
      faces[i] = make_face3d ();

      object3d_add_face (board_object, faces[i]);
      /* Pick one of the upright edges which is within this face outer contour loop, and link it to the face */
#ifdef REVERSED_PCB_CONTOURS
      face3d_add_contour (faces[i], make_contour3d (edges[2 * npoints + i]));
#else
      face3d_add_contour (faces[i], make_contour3d (SYM(edges[2 * npoints + i])));
#endif
    }

    faces[npoints] = make_face3d (); /* bottom_face */
    face3d_set_normal (faces[npoints], 0., 0., -1.);
    face3d_set_appearance (faces[npoints], top_bot_appearance);
    object3d_add_face (board_object, faces[npoints]);

    faces[npoints + 1] = make_face3d (); /* top_face */
    face3d_set_normal (faces[npoints + 1], 0., 0., 1.);
    face3d_set_appearance (faces[npoints + 1], top_bot_appearance);
    object3d_add_face (board_object, faces[npoints + 1]);

    /* Pick the first bottom / top edge within the bottom / top face outer contour loop, and link it to the face */
#ifdef REVERSED_PCB_CONTOURS
    face3d_add_contour (faces[npoints], make_contour3d (edges[0]));
    face3d_add_contour (faces[npoints + 1], make_contour3d (SYM(edges[npoints])));
#else
    face3d_add_contour (faces[npoints], make_contour3d (SYM(edges[0])));
    face3d_add_contour (faces[npoints + 1], make_contour3d (edges[npoints]));
#endif

    ct = contour;
    start_of_ct = 0;
    offset_in_ct = 0;
    ct_npoints = get_contour_npoints (ct);

    for (i = 0; i < npoints; i++, offset_in_ct++) {
      int next_i_around_ct;
      int prev_i_around_ct;

      /* Update which contour we're looking at */
      if (offset_in_ct == ct_npoints) {
        start_of_ct = i;
        offset_in_ct = 0;
        ct = ct->next;
        ct_npoints = get_contour_npoints (ct);

        /* If there is more than one contour, it will be an inner contour of the bottom and top faces. Refer to it here */
#ifdef REVERSED_PCB_CONTOURS
        face3d_add_contour (faces[npoints], make_contour3d (edges[i]));
        face3d_add_contour (faces[npoints + 1], make_contour3d (SYM(edges[npoints + i])));
#else
        face3d_add_contour (faces[npoints], make_contour3d (SYM(edges[i])));
        face3d_add_contour (faces[npoints + 1], make_contour3d (edges[npoints + i]));
#endif
      }

      next_i_around_ct = start_of_ct + (offset_in_ct + 1) % ct_npoints;
      prev_i_around_ct = start_of_ct + (offset_in_ct + ct_npoints - 1) % ct_npoints;

      /* Setup the face normals for the edges along the contour extrusion (top and bottom are handled separaetely) */
      /* Define the (non-normalized) face normal to point to the outside of the contour */
#if REVERSED_PCB_CONTOURS
      /* Vertex ordering of the edge we're finding the normal to is reversed in this case */
      face3d_set_normal (faces[i], -(vertices[next_i_around_ct]->y - vertices[i]->y),
                                    (vertices[next_i_around_ct]->x - vertices[i]->x), 0.);
#else
      face3d_set_normal (faces[i],  (vertices[next_i_around_ct]->y - vertices[i]->y),
                                   -(vertices[next_i_around_ct]->x - vertices[i]->x), 0.);
#endif

      /* Assign the appropriate vertex geometric data to each edge end */
      ODATA (edges[              i]) = vertices[0 * npoints + i];
      DDATA (edges[              i]) = vertices[0 * npoints + next_i_around_ct];
      ODATA (edges[1 * npoints + i]) = vertices[1 * npoints + i];
      DDATA (edges[1 * npoints + i]) = vertices[1 * npoints + next_i_around_ct];
      ODATA (edges[2 * npoints + i]) = vertices[0 * npoints + i];
      DDATA (edges[2 * npoints + i]) = vertices[1 * npoints + i];
#if REVERSED_PCB_CONTOURS
      RDATA (edges[              i]) = faces[i];
      LDATA (edges[              i]) = faces[npoints];
      RDATA (edges[1 * npoints + i]) = faces[npoints + 1];
      LDATA (edges[1 * npoints + i]) = faces[i];
      RDATA (edges[2 * npoints + i]) = faces[prev_i_around_ct];
      LDATA (edges[2 * npoints + i]) = faces[i];
#else
      LDATA (edges[              i]) = faces[i];
      RDATA (edges[              i]) = faces[npoints];
      LDATA (edges[1 * npoints + i]) = faces[npoints + 1];
      RDATA (edges[1 * npoints + i]) = faces[i];
      LDATA (edges[2 * npoints + i]) = faces[prev_i_around_ct];
      RDATA (edges[2 * npoints + i]) = faces[i];
#endif

      /* NB: Contours are counter clockwise in XY plane.
       *     edges[          0-npoints-1] are the base of the extrusion, following in the counter clockwise order
       *     edges[1*npoints-2*npoints-1] are the top  of the extrusion, following in the counter clockwise order
       *     edges[2*npoints-3*npoints-1] are the upright edges, oriented from bottom to top
       */

#ifdef REVERSED_PCB_CONTOURS  /* UNDERLYING DATA HAS CW CONTOURS FOR OUTER, CCW FOR INNER - E.g. PCB's polygons when translated into STEP coordinates */
      /* Link edges orbiting around each bottom vertex i (0 <= i < npoints) */
      splice (SYM(edges[prev_i_around_ct]), edges[2 * npoints + i]);
      splice (edges[2 * npoints + i], edges[i]);

      /* Link edges orbiting around each top vertex (npoints + i) (0 <= i < npoints) */
      splice (edges[npoints + i], SYM(edges[2 * npoints + i]));
      splice (SYM(edges[2 * npoints + i]), SYM(edges[npoints + prev_i_around_ct]));
#else /* UNDERLYING DATA HAS CCW CONTOURS FOR OUTER, CW FOR INNER. E.g. PCB's raw coordinates in X, Y */
      /* Link edges orbiting around each bottom vertex i (0 <= i < npoints) */
      splice (edges[i], edges[2 * npoints + i]);
      splice (edges[2 * npoints + i], SYM(edges[prev_i_around_ct]));

      /* Link edges orbiting around each top vertex (npoints + i) (0 <= i < npoints) */
      splice (SYM(edges[npoints + prev_i_around_ct]), SYM(edges[2 * npoints + i]));
      splice (SYM(edges[2 * npoints + i]),  edges[npoints + i]);
#endif

      if (ct->is_round) {

        face3d_set_cylindrical (faces[i], COORD_TO_STEP_X (PCB, ct->cx), COORD_TO_STEP_Y (PCB, ct->cy), 0., /* A point on the axis of the cylinder */
                                          0., 0., 1.,                                                       /* Direction of the cylindrical axis */
                                          COORD_TO_MM (ct->radius));
        face3d_set_surface_orientation_reversed (faces[i]); /* XXX: Assuming this is a hole, the cylindrical surface normal points in the wrong direction - INCORRECT IF THIS IS THE OUTER CONTOUR!*/
        face3d_set_normal (faces[i], 1., 0., 0.);  /* A normal to the axis direction */
                                  /* XXX: ^^^ Could line this up with the direction to the vertex in the corresponding circle edge */

#ifdef REVERSED_PCB_CONTOURS
        edge_info_set_round (UNDIR_DATA (edges[i]),
                             COORD_TO_STEP_X (PCB, ct->cx), COORD_TO_STEP_Y (PCB, ct->cy), COORD_TO_STEP_Z (PCB, -HACK_BOARD_THICKNESS), /* Center of circle */
                             0., 0., 1., /* Normal */ COORD_TO_MM (ct->radius)); /* NORMAL POINTING TO -VE Z MAKES CIRCLE CLOCKWISE */
        edge_info_set_round (UNDIR_DATA (edges[npoints + i]),
                             COORD_TO_STEP_X (PCB, ct->cx), COORD_TO_STEP_Y (PCB, ct->cy), 0., /* Center of circle */
                             0., 0., 1., /* Normal */ COORD_TO_MM (ct->radius)); /* NORMAL POINTING TO -VE Z MAKES CIRCLE CLOCKWISE */
#else
        edge_info_set_round (UNDIR_DATA (edges[i]),
                             COORD_TO_STEP_X (PCB, ct->cx), COORD_TO_STEP_Y (PCB, ct->cy), COORD_TO_STEP_Z (PCB, -HACK_BOARD_THICKNESS), /* Center of circle */
                             0., 0., -1., /* Normal */ COORD_TO_MM (ct->radius)); /* NORMAL POINTING TO -VE Z MAKES CIRCLE CLOCKWISE */
        edge_info_set_round (UNDIR_DATA (edges[npoints + i]),
                             COORD_TO_STEP_X (PCB, ct->cx), COORD_TO_STEP_Y (PCB, ct->cy), 0., /* Center of circle */
                             0., 0., -1., /* Normal */ COORD_TO_MM (ct->radius)); /* NORMAL POINTING TO -VE Z MAKES CIRCLE CLOCKWISE */
#endif
        edge_info_set_stitch (UNDIR_DATA (edges[2 * npoints + i]));
      }

    }

    if (1) {
      /* Cylinder centers on 45x45mm, stitch vertex is at 40x45mm. Radius is thus 5mm */

      edge_ref cylinder_edges[3];
      vertex3d *cylinder_vertices[2];
      face3d *cylinder_faces[2];

      /* Edge on top of board */
      cylinder_edges[0] = make_edge ();
      UNDIR_DATA (cylinder_edges[0]) = make_edge_info ();
#ifdef REVERSED_PCB_CONTOURS
      edge_info_set_round (UNDIR_DATA (cylinder_edges[0]),
                           COORD_TO_STEP_X (PCB, MM_TO_COORD (45.)), COORD_TO_STEP_Y (PCB, MM_TO_COORD (45.)), 0., /* Center of circle */
                            0.,   0., 1., /* Normal */
                            5.);          /* Radius */
#else
      edge_info_set_round (UNDIR_DATA (cylinder_edges[0]),
                           COORD_TO_STEP_X (PCB, MM_TO_COORD (45.)), COORD_TO_STEP_Y (PCB, MM_TO_COORD (45.)), 0., /* Center of circle */
                            0.,   0., 1., /* Normal */
                            5.);         /* Radius */
#endif
      object3d_add_edge (board_object, cylinder_edges[0]);

      /* Edge on top of cylinder */
      cylinder_edges[1] = make_edge ();
      UNDIR_DATA (cylinder_edges[1]) = make_edge_info ();
      edge_info_set_round (UNDIR_DATA (cylinder_edges[1]),
                           COORD_TO_STEP_X (PCB, MM_TO_COORD (45.)), COORD_TO_STEP_Y (PCB, MM_TO_COORD (45.)), 10., /* Center of circle */
                            0.,   0., 1.,  /* Normal */
                            5.);          /* Radius */
      object3d_add_edge (board_object, cylinder_edges[1]);

      /* Edge stitching cylinder */
      cylinder_edges[2] = make_edge ();
      UNDIR_DATA (cylinder_edges[2]) = make_edge_info ();
      edge_info_set_stitch (UNDIR_DATA (cylinder_edges[2]));
      object3d_add_edge (board_object, cylinder_edges[2]);

      /* Vertex on board top surface */
      cylinder_vertices[0] = make_vertex3d (COORD_TO_STEP_X (PCB, MM_TO_COORD (40.)), COORD_TO_STEP_Y (PCB, MM_TO_COORD (45.)), 0.); /* Bottom */
      object3d_add_vertex (board_object, cylinder_vertices[0]);

      /* Vertex on cylinder top surface */
      cylinder_vertices[1] = make_vertex3d (COORD_TO_STEP_X (PCB, MM_TO_COORD (40.)), COORD_TO_STEP_Y (PCB, MM_TO_COORD (45.)), 10.); /* Top */
      object3d_add_vertex (board_object, cylinder_vertices[1]);

      /* Cylindrical face */
      cylinder_faces[0] = make_face3d ();
      face3d_set_cylindrical (cylinder_faces[0], COORD_TO_STEP_X (PCB, MM_TO_COORD (45.)), COORD_TO_STEP_Y (PCB, MM_TO_COORD (45.)), 0., /* A point on the axis of the cylinder */
                                        0., 0., 1.,             /* Direction of the cylindrical axis */
                                        5.);                   /* Radius of cylinder */
      face3d_set_normal (cylinder_faces[0], 1., 0., 0.);       /* A normal to the axis direction */
                                   /* XXX: ^^^ Could line this up with the direction to the vertex in the corresponding circle edge */
      object3d_add_face (board_object, cylinder_faces[0]);
      face3d_add_contour (cylinder_faces[0], make_contour3d (cylinder_edges[0]));

      /* Top face of cylinder */
      cylinder_faces[1] = make_face3d (); /* top face of cylinder */
      face3d_set_normal (cylinder_faces[1], 0., 0., 1.);
      face3d_set_appearance (cylinder_faces[1], top_bot_appearance);
      object3d_add_face (board_object, cylinder_faces[1]);
      face3d_add_contour (cylinder_faces[1], make_contour3d (cylinder_edges[1]));

      /* Splice onto board */
      face3d_add_contour (faces[npoints + 1], make_contour3d (SYM(cylinder_edges[0])));

      /* Assign the appropriate vertex geometric data to each edge end */
      ODATA (cylinder_edges[0]) = cylinder_vertices[0];
      DDATA (cylinder_edges[0]) = cylinder_vertices[0];
      ODATA (cylinder_edges[1]) = cylinder_vertices[1];
      DDATA (cylinder_edges[1]) = cylinder_vertices[1];
      ODATA (cylinder_edges[2]) = cylinder_vertices[0];
      DDATA (cylinder_edges[2]) = cylinder_vertices[1];
      LDATA (cylinder_edges[0]) = cylinder_faces[0];
      RDATA (cylinder_edges[0]) = faces[npoints + 1]; /* TOP OF BOARD FACE */
      LDATA (cylinder_edges[1]) = cylinder_faces[1];
      RDATA (cylinder_edges[1]) = cylinder_faces[0];
      LDATA (cylinder_edges[2]) = cylinder_faces[0];
      RDATA (cylinder_edges[2]) = cylinder_faces[0];

      /* Splice things together.... */

      /* Link edges orbiting the cylinder bottom vertex */
      splice (cylinder_edges[0], cylinder_edges[2]);
      splice (cylinder_edges[2], SYM(cylinder_edges[0]));

      /* Link edges orbiting the cylinder top vertex */
      splice (SYM(cylinder_edges[2]), cylinder_edges[1]);
      splice (cylinder_edges[1], SYM(cylinder_edges[1]));
    }

    board_objects = g_list_append (board_objects, board_object);

  } while (pa = pa->f, pa != board_outline);

  poly_Free (&board_outline);

  return board_objects;
}

void
object3d_test_board_outline (void)
{
  GList *board_outlines;

  board_outlines = object3d_from_board_outline ();
  object3d_export_to_step (board_outlines->data, "object3d_test.step");

  g_list_free_full (board_outlines, (GDestroyNotify)destroy_object3d);
}
