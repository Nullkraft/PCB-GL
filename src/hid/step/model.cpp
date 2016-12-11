/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 2015 Peter Clifton
 *  Copyright (C) 2015 PCB Contributors (see ChangeLog for details)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Contact addresses for paper mail and Email:
 *  Harry Eaton, 6697 Buttonhole Ct, Columbia, MD 21044, USA
 *  haceaton@aplcomm.jhuapl.edu
 *
 */


#include <STEPfile.h>
#include <sdai.h>
#include <STEPattribute.h>
#include <ExpDict.h>
#include <Registry.h>
#include <errordesc.h>

#include <STEPcomplex.h>
#include <SdaiHeaderSchema.h>

#include "schema.h"

#include <SdaiAUTOMOTIVE_DESIGN.h>

#include "utils.h"
#include "string.h"

extern "C" {
#include <glib.h>
/* XXX: Sdai and PCB clash.. both define MarkType */
#include "global.h"
#include "../hid/common/appearance.h"
#include "../hid/common/step_id.h"
#include "../hid/common/quad.h"
#include "../hid/common/edge3d.h"
#include "../hid/common/contour3d.h"
#include "../hid/common/face3d.h"
#include "../hid/common/vertex3d.h"
#include "../hid/common/object3d.h"
}

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if 1
#  define DEBUG_PRODUCT_DEFINITION_SEARCH
#  define DEBUG_CHILD_REMOVAL
#  define DEBUG_PRODUCT_DEFINITION
#else
#  undef DEBUG_PRODUCT_DEFINITION_SEARCH
#  undef DEBUG_CHILD_REMOVAL
#  undef DEBUG_PRODUCT_DEFINITION
#endif

#undef DEBUG_NOT_IMPLEMENTED

#include <glib.h>

extern "C" {
#include "model.h"
}


typedef std::list<SDAI_Application_instance *> ai_list;
typedef std::list<SdaiManifold_solid_brep *> msb_list;
typedef std::list<SdaiMapped_item *> mi_list;


SdaiProduct_definition *
read_model_from_file (Registry *registry,
                        InstMgr *instance_list,
                        const char *filename)
{
  STEPfile sfile = STEPfile (*registry, *instance_list, "", false);

  try
    {
      sfile.ReadExchangeFile (filename);
    }
  catch (...)
    {
      std::cout << "ERROR: Caught exception when attempting to read from file '" << filename << "' (does the file exist?)" << std::endl;
      return NULL;
    }

  Severity severity = sfile.Error().severity();
  if (severity != SEVERITY_NULL)
    {
      sfile.Error().PrintContents (std::cout);
      std::cout << "WARNING: Error reading from file '" << filename << "'" << std::endl;
//      return NULL;
// XXX: HANDLE OTHER ERRORS BETTER?
    }

  pd_list pd_list;

  // Find all PRODUCT_DEFINITION entities with a SHAPE_DEFINITION_REPRESETNATION
  find_all_pd_with_sdr (instance_list, &pd_list, 1);

  /*  Try to determine the root product */
  find_and_remove_child_pd (instance_list, &pd_list, 1, "Next_assembly_usage_occurrence"); // Remove any PD which are children of another via NAUO
  find_and_remove_child_pd (instance_list, &pd_list, 1, "Assembly_component_usage");       // Remove any PD which are children of another via ACU
  find_and_remove_child_pd_mi_rm_sr (instance_list, &pd_list, 1); // Remove any PD which are children of another via MAPPED_ITEM->REPRESENTATION_MAP->SHAPE_REPRESENTATION


#ifdef DEBUG_PRODUCT_DEFINITION_SEARCH
  std::cout << "Hopefully left with the root product definition" << std::endl;
  for (pd_list::iterator iter = pd_list.begin(); iter != pd_list.end(); iter++)
    std::cout << "Product definition list item #" << (*iter)->StepFileId () << std::endl;
  std::cout << std::endl;
#endif

  // If we didn't find a suitable PD, give up now
  if (pd_list.size() == 0)
    {
      std::cout << "ERROR: Did not find a PRODUCT_DEFINITION (with associated SHAPE_DEFINITION_REPRESENTATION)" << std::endl;
      return NULL;
    }

  if (pd_list.size() > 1)
    std::cout << "WARNING: Found more than one PRODUCT_DEFINITION that might be the root" << std::endl;

  // Use the first PD meeting the criterion. Hopefully there should just be one, but if not, we pick the first.
  return *pd_list.begin();
}

static void
find_manifold_solid_brep (SdaiShape_representation *sr,
                          msb_list *msb_list)
{
  SingleLinkNode *iter = sr->items_ ()->GetHead ();

  while (iter != NULL)
    {
      SDAI_Application_instance *node = ((EntityNode *)iter)->node;

      if (strcmp (node->EntityName (), "Manifold_Solid_Brep") == 0)
        msb_list->push_back ((SdaiManifold_solid_brep *)node);

      iter = iter->NextNode ();
    }
}

static void
find_mapped_item (SdaiShape_representation *sr,
                  mi_list *mi_list)
{
  SingleLinkNode *iter = sr->items_ ()->GetHead ();

  while (iter != NULL)
    {
      SDAI_Application_instance *node = ((EntityNode *)iter)->node;

      if (strcmp (node->EntityName (), "Mapped_Item") == 0)
        mi_list->push_back ((SdaiMapped_item *)node);

      iter = iter->NextNode ();
    }
}

typedef struct process_step_info {
  /* Hash / list of SR -> step_model */
  object3d *object;
  double current_transform[4][4];

} process_step_info;

void
copy_4x4 (double from[4][4],
          double to[4][4])
{
  memcpy (to, from, sizeof(double[4][4]));
}

void
identity_4x4 (double m[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      m[i][j] = 0.0;

  m[0][0] = 1.0;
  m[1][1] = 1.0;
  m[2][2] = 1.0;
  m[3][3] = 1.0;
}

/* NB: Column major */
/* matrix[column][row] */
void
mult_4x4 (double a[4][4], double b[4][4], double to[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      to[i][j] = a[0][j] * b[i][0] +
                 a[1][j] * b[i][1] +
                 a[2][j] * b[i][2] +
                 a[3][j] * b[i][3];
}

/* NB: Column major */
void
translate_origin (double m[4][4], double x, double y, double z)
{
  m[3][0] += x;
  m[3][1] += y;
  m[3][2] += z;
}

/* NB: Column major */
void
rotate_basis (double m[4][4], double ax, double ay, double az,
                              double rx, double ry, double rz)
{
  double basis[4][4];
  double old[4][4];
  double ox, oy, oz;

  ox = ay * rz - az * ry;
  oy = az * rx - ax * rz;
  oz = ax * ry - ay * rx;

  basis[0][0] = rx;  basis[1][0] = ox;  basis[2][0] = ax;  basis[3][0] = 0.0;
  basis[0][1] = ry;  basis[1][1] = oy;  basis[2][1] = ay;  basis[3][1] = 0.0;
  basis[0][2] = rz;  basis[1][2] = oz;  basis[2][2] = az;  basis[3][2] = 0.0;
  basis[0][3] = 0.0; basis[1][3] = 0.0; basis[2][3] = 0.0; basis[3][3] = 1.0;

  copy_4x4 (m, old);
  //mult_4x4 (old, basis, m);
  mult_4x4 (basis, old, m);
}

/* NB: Row major, or transosed column major. Since Matrix will be orthogonal, this is equal to its inverse */
void
rotate_basis_inverted (double m[4][4], double ax, double ay, double az,
                                       double rx, double ry, double rz)
{
  double basis[4][4];
  double old[4][4];
  double ox, oy, oz;

  ox = ay * rz - az * ry;
  oy = az * rx - ax * rz;
  oz = ax * ry - ay * rx;

  basis[0][0] = rx;  basis[0][1] = ox;  basis[0][2] = ax;  basis[0][3] = 0.0;
  basis[1][0] = ry;  basis[1][1] = oy;  basis[1][2] = ay;  basis[1][3] = 0.0;
  basis[2][0] = rz;  basis[2][1] = oz;  basis[2][2] = az;  basis[2][3] = 0.0;
  basis[3][0] = 0.0; basis[3][1] = 0.0; basis[3][2] = 0.0; basis[3][3] = 1.0;

  copy_4x4 (m, old);
  //mult_4x4 (old, basis, m);
  mult_4x4 (basis, old, m);
}

/* NB: Column major */
void
transform_vertex (double m[4][4], double *x, double *y, double *z)
{
  double new_x, new_y, new_z, new_w;

  new_x = m[0][0] * *x +
          m[1][0] * *y +
          m[2][0] * *z +
          m[3][0] * 1.0;

  new_y = m[0][1] * *x +
          m[1][1] * *y +
          m[2][1] * *z +
          m[3][1] * 1.0;

  new_z = m[0][2] * *x +
          m[1][2] * *y +
          m[2][2] * *z +
          m[3][2] * 1.0;

#if 0
  new_w = m[0][3] * *x +
          m[1][3] * *y +
          m[2][3] * *z +
          m[3][3] * 1.0;

  new_x /= new_w;
  new_y /= new_w;
  new_z /= new_w;
#endif

  *x = new_x;
  *y = new_y;
  *z = new_z;
}

/* NB: Column major */
void
transform_vector (double m[4][4], double *x, double *y, double *z)
{
  double new_x, new_y, new_z, new_w;

  new_x = m[0][0] * *x +
          m[1][0] * *y +
          m[2][0] * *z;

  new_y = m[0][1] * *x +
          m[1][1] * *y +
          m[2][1] * *z;

  new_z = m[0][2] * *x +
          m[1][2] * *y +
          m[2][2] * *z;

#if 0
  new_w = m[0][3] * *x +
          m[1][3] * *y +
          m[2][3] * *z +
          m[3][3] * 1.0;

  new_x /= new_w;
  new_y /= new_w;
  new_z /= new_w;
#endif

  *x = new_x;
  *y = new_y;
  *z = new_z;
}

static void
process_edges (GHashTable *edges_hash_set, process_step_info *info) //object3d *object)
{
  GHashTableIter iter;
  SdaiEdge *edge;
  edge_ref our_edge;
  vertex3d *vertex;
  double x1, y1, z1;
  double x2, y2, z2;
  bool orientation;
  gpointer foo;
  int bar;
  bool kludge;

  g_hash_table_iter_init (&iter, edges_hash_set);
  while (g_hash_table_iter_next (&iter, (void **)&edge, &foo))
    {
      bar = GPOINTER_TO_INT (foo);
      if (strcmp (edge->edge_start_ ()->EntityName (), "Vertex_Point") != 0 ||
          strcmp (edge->edge_end_   ()->EntityName (), "Vertex_Point") != 0)
        {
          printf ("WARNING: Edge start and/or end vertices are not specified as VERTEX_POINT\n");
          continue;
        }

      orientation = (bar & 1) != 0;
      kludge = (bar & 2) != 0;

      // NB: Assuming edge points to an EDGE, or one of its subtypes that does not make edge_start and edge_end derived attributes.
      //     In practice, edge should point to an EDGE_CURVE sub-type
      SdaiVertex_point *edge_start = (SdaiVertex_point *) (orientation ? edge->edge_start_ () : edge->edge_end_ ());
      SdaiVertex_point *edge_end =  (SdaiVertex_point *) (!orientation ? edge->edge_start_ () : edge->edge_end_ ());

      // NB: XXX: SdaiVertex_point multiply inherits from vertex and geometric_representation_item

      SdaiPoint *edge_start_point = edge_start->vertex_geometry_ ();
      SdaiPoint *edge_end_point = edge_end->vertex_geometry_ ();

      if (strcmp (edge_start_point->EntityName (), "Cartesian_Point") == 0)
        {
          /* HAPPY WITH THIS TYPE */
        }
      else
        {
          // XXX: point_on_curve, point_on_surface, point_replica, degenerate_pcurve
          printf ("WARNING: Got Edge start point as unhandled point type (%s)\n", edge_start_point->EntityName ());
          continue;
        }

      if (strcmp (edge_end_point->EntityName (), "Cartesian_Point") == 0)
        {
          /* HAPPY WITH THIS TYPE */
        }
      else
        {
          // XXX: point_on_curve, point_on_surface, point_replica, degenerate_pcurve
          printf ("WARNING: Got Edge end point as unhandled point type (%s)\n", edge_end_point->EntityName ());
          continue;
        }

      SdaiCartesian_point *edge_start_cp = (SdaiCartesian_point *)edge_start_point;
      SdaiCartesian_point *edge_end_cp = (SdaiCartesian_point *)edge_end_point;

      x1 = ((RealNode *)edge_start_cp->coordinates_ ()->GetHead ())->value;
      y1 = ((RealNode *)edge_start_cp->coordinates_ ()->GetHead ()->NextNode ())->value;
      z1 = ((RealNode *)edge_start_cp->coordinates_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      x2 = ((RealNode *)edge_end_cp->coordinates_ ()->GetHead ())->value;
      y2 = ((RealNode *)edge_end_cp->coordinates_ ()->GetHead ()->NextNode ())->value;
      z2 = ((RealNode *)edge_end_cp->coordinates_ ()->GetHead ()->NextNode ()->NextNode ())->value;

#if 0
      printf ("    Edge #%i starts at (%f, %f, %f) and ends at (%f, %f, %f)\n",
              edge->StepFileId (), x1, y1, z1, x2, y2, z2);
#endif

      if (strcmp (edge->EntityName (), "Edge_Curve") == 0)
        {
          SdaiEdge_curve *ec = (SdaiEdge_curve *)edge;

          SdaiCurve *curve = ec->edge_geometry_ ();
          bool same_sense = ec->same_sense_ ();

#ifdef DEBUG_NOT_IMPLEMENTED
          if (!same_sense)
            printf ("XXX: HAVE NOT TESTED THIS CASE.... same_sense is false\n");
#endif

#if 0
          printf ("         underlying curve is %s #%i, same_sense is %s\n", curve->EntityName (), curve->StepFileId(), same_sense ? "True" : "False");
#endif

          if (strcmp (curve->EntityName (), "Line") == 0)
            {
              transform_vertex (info->current_transform, &x1, &y1, &z1);
              transform_vertex (info->current_transform, &x2, &y2, &z2);

              our_edge = make_edge ();
              UNDIR_DATA (our_edge) = make_edge_info ();
              object3d_add_edge (info->object, our_edge);
              vertex = make_vertex3d (x1, y1, z1);
              ODATA(our_edge) = vertex;
              vertex = make_vertex3d (x2, y2, z2);
              DDATA(our_edge) = vertex;

//              printf ("WARNING: Underlying curve geometry type Line is not supported yet\n");
//              continue;
            }
          else if (strcmp (curve->EntityName (), "Circle") == 0)
            {
              SdaiCircle *circle = (SdaiCircle *)curve;
              double cx = ((RealNode *)circle->position_ ()->location_ ()->coordinates_ ()->GetHead ())->value;
              double cy = ((RealNode *)circle->position_ ()->location_ ()->coordinates_ ()->GetHead ()->NextNode ())->value;
              double cz = ((RealNode *)circle->position_ ()->location_ ()->coordinates_ ()->GetHead ()->NextNode ()->NextNode ())->value;
              double nx = ((RealNode *)circle->position_ ()->axis_ ()->direction_ratios_ ()->GetHead ())->value;
              double ny = ((RealNode *)circle->position_ ()->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
              double nz = ((RealNode *)circle->position_ ()->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;

              double radius = circle->radius_();

              edge_info *edge_info;

              transform_vertex (info->current_transform, &cx, &cy, &cz);
              transform_vertex (info->current_transform, &x1, &y1, &z1);
              transform_vertex (info->current_transform, &x2, &y2, &z2);

              transform_vector (info->current_transform, &nx, &ny, &nz);

              our_edge = make_edge ();
              edge_info = make_edge_info ();
              if (!kludge) //(same_sense)
                {
                  edge_info_set_round (edge_info, cx, cy, cz, nx, ny, nz, radius);
                }
              else
                {
                  printf ("URM................\n");
                  edge_info_set_round (edge_info, cx, cy, cz, -nx, -ny, -nz, radius);
                }

              UNDIR_DATA (our_edge) = edge_info;
              object3d_add_edge (info->object, our_edge);
              vertex = make_vertex3d (x1, y1, z1);
              ODATA(our_edge) = vertex;
              vertex = make_vertex3d (x2, y2, z2);
              DDATA(our_edge) = vertex;

//              printf ("WARNING: Underlying curve geometry type circle is not supported yet\n");
//              continue;
            }
          else
            {
#ifdef DEBUG_NOT_IMPLEMENTED
              printf ("WARNING: Unhandled curve geometry type (%s), #%i\n", curve->EntityName (), curve->StepFileId ());
#endif
              // XXX: line, conic, pcurve, surface_curve, offset_curve_2d, offset_curve_3d, curve_replica
              // XXX: Various derived types of the above, e.g.:
              //      conic is a supertype of: circle, ellipse, hyperbola, parabola
              continue;
            }

        }
      else
        {
          printf ("WARNING: found unknown edge type (%s)\n", edge->EntityName ());
          continue;
        }
    }
}

static step_model *
process_sr_or_subtype(InstMgr *instance_list, SdaiShape_representation *sr, process_step_info *info);

static step_model *
process_shape_representation(InstMgr *instance_list, SdaiShape_representation *sr, process_step_info *info)
{
  step_model *step_model;
//  std::cout << "INFO: Processing raw SR" << std::endl;

  step_model = g_new0(struct step_model, 1);
//  step_model->filename = g_strdup(filename);
//  step_model->instances = NULL;    /* ??? */

#if 0
  SdaiAxis2_placement_3d *part_origin = find_axis2_placement_3d_in_sr (sr);
  if (part_origin == NULL)
    std::cout << "WARNING: Could not find AXIS2_PLACEMENT_3D entity in SHAPE_REPRESENTATION" << std::endl;

  if (part_origin != NULL)
    {
      step_model->ox = ((RealNode *)part_origin->location_ ()->coordinates_ ()->GetHead ())->value;
      step_model->oy = ((RealNode *)part_origin->location_ ()->coordinates_ ()->GetHead ()->NextNode ())->value;
      step_model->oz = ((RealNode *)part_origin->location_ ()->coordinates_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      step_model->ax = ((RealNode *)part_origin->axis_ ()->direction_ratios_ ()->GetHead ())->value;
      step_model->ay = ((RealNode *)part_origin->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      step_model->az = ((RealNode *)part_origin->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      step_model->rx = ((RealNode *)part_origin->ref_direction_ ()->direction_ratios_ ()->GetHead ())->value;
      step_model->ry = ((RealNode *)part_origin->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      step_model->rz = ((RealNode *)part_origin->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;
    }
#endif

  /* We need to find "Shape_representation_relation" linking this SR to another.
   * The SRR could be on its own, or (for assemblies), is likely to be in a complex with
   * "Representation_relationship_with_transformation", in which case the
   * "Representation_Relationship" supertype of Shape_representation_relation is also
   * explicitly in the complex.
   */

  srr_list srr_list;

  // Find all SHAPE_REPRESENTATION_RELATIONSHIP with rep_1 = sr
  find_all_srr_with_rep_1 (instance_list, &srr_list, 1, sr);

  for (srr_list::iterator iter = srr_list.begin (); iter != srr_list.end (); iter++)
    {
      SdaiShape_representation_relationship *srr = (*iter);
//      std::cout << "Found SRR; processing" << std::endl;

      SdaiShape_representation *child_sr = dynamic_cast<SdaiShape_representation *>(srr->rep_2_ ());

      /* XXX: Actually only want to "process" the SR once per SR, then create _instances_ of it */
      /* XXX: Do something with the result */
      // Leave existing transformation
      process_sr_or_subtype (instance_list, child_sr, info);
    }


  srr_rrwt_list srr_rrwt_list;

  // Find all SHAPE_REPRESENTATION_RELATIONSHIP with rep_1 = sr
  find_all_srr_rrwt_with_rep_1 (instance_list, &srr_rrwt_list, 1, sr);

  for (srr_rrwt_list::iterator iter = srr_rrwt_list.begin (); iter != srr_rrwt_list.end (); iter++)
    {
      double backup_transform[4][4];
      double ox, oy, oz;
      double ax, ay, az;
      double rx, ry, rz;
      SdaiAxis2_placement_3d *parent_axis;
      SdaiAxis2_placement_3d *child_axis;

      srr_rrwt *item = (*iter);
//      std::cout << "Found SRR + RRWT; processing" << std::endl;

      SdaiShape_representation *child_sr = dynamic_cast<SdaiShape_representation *>(item->rep_2);
      SdaiItem_defined_transformation *idt = item->idt;

//      std::cout << "  child SR: #" << child_sr->StepFileId() << " IDT: #" << idt->StepFileId() << std::endl;

      copy_4x4 (info->current_transform, backup_transform);

      child_axis = dynamic_cast<SdaiAxis2_placement_3d *>(idt->transform_item_1_());
      parent_axis = dynamic_cast<SdaiAxis2_placement_3d *>(idt->transform_item_2_());

      if (parent_axis == NULL ||
          child_axis == NULL)
        {
          std::cout << "ERROR: Got NULL in one of the axis placements for IDT" << std::endl;
          continue;
        }

      ox = ((RealNode *)child_axis->location_ ()->coordinates_ ()->GetHead ())->value;
      oy = ((RealNode *)child_axis->location_ ()->coordinates_ ()->GetHead ()->NextNode ())->value;
      oz = ((RealNode *)child_axis->location_ ()->coordinates_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      ax = ((RealNode *)child_axis->axis_ ()->direction_ratios_ ()->GetHead ())->value;
      ay = ((RealNode *)child_axis->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      az = ((RealNode *)child_axis->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      rx = ((RealNode *)child_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ())->value;
      ry = ((RealNode *)child_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      rz = ((RealNode *)child_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;

      printf ("child axis o: (%f, %f, %f)\n"
              "           a: (%f, %f, %f)\n"
              "           r: (%f, %f, %f)\n",
              ox, oy, oz,
              ax, ay, az,
              rx, ry, rz);

      /* XXX: Looking only at the target vector.. need to find some examples where the parent transform coordinate system is not unity to get this correct */
      rotate_basis (info->current_transform, ax, ay, az, rx, ry, rz);

      /* Is this in the correct order? */
      translate_origin (info->current_transform, ox, oy, oz);

      ox = ((RealNode *)parent_axis->location_ ()->coordinates_ ()->GetHead ())->value;
      oy = ((RealNode *)parent_axis->location_ ()->coordinates_ ()->GetHead ()->NextNode ())->value;
      oz = ((RealNode *)parent_axis->location_ ()->coordinates_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      ax = ((RealNode *)parent_axis->axis_ ()->direction_ratios_ ()->GetHead ())->value;
      ay = ((RealNode *)parent_axis->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      az = ((RealNode *)parent_axis->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      rx = ((RealNode *)parent_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ())->value;
      ry = ((RealNode *)parent_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      rz = ((RealNode *)parent_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;

      printf ("parent axis o: (%f, %f, %f)\n"
              "            a: (%f, %f, %f)\n"
              "            r: (%f, %f, %f)\n",
              ox, oy, oz,
              ax, ay, az,
              rx, ry, rz);
      printf ("\n");

      rotate_basis_inverted (info->current_transform, ax, ay, az, rx, ry, rz);
      translate_origin (info->current_transform, -ox, -oy, -oz);

      /* XXX: Actually only want to "process" the SR once per SR, then create _instances_ of it */
      /* XXX: Origin offset etc..? */
      /* XXX: Do something with the result */
      process_sr_or_subtype (instance_list, child_sr, info);

      // Revert the transformation
      copy_4x4 (backup_transform, info->current_transform);
    }


  // Find all SRR where RR.rep_1 = sr
  //              let   child_sr = RR.rep_2

  // If SRR node is complex, and also RRWT, extract transform as follows:
  // SdaiTransformation(SdaiSelect) transformation = RRWT.transformation_operator
  // Check transformation.UnderlyingTypeName () == "Item_defined_transformation" - if not, error (don't know how to do Functionally defined tranformation
  // item1 = transform.transform_item_1_() // Axis in parent (use dynamic cast to ensure it is the correct type?)
  // item2 = transform.transform_item_2_() // Axis in child (use dynamic cast to ensure it is the correct type?)
  // If item1 or item2 is NULL, then drop this child?

  // If SRR node was not complex, insert child with 1:1 tranformation

  return step_model;
}

static step_model *
process_sr_or_subtype(InstMgr *instance_list, SdaiShape_representation *sr, process_step_info *info)
{
  step_model *step_model;
//  object3d *object;
  GHashTable *edges_hash_set;
  bool on_plane;

  // If sr is an exact match for the step entity SHAPE_REPRESENTATION (not a subclass), call the specific hander
  if (strcmp (sr->EntityName (), "Shape_Representation") == 0)
    {
      return process_shape_representation (instance_list, sr, info);
    }

  if (strcmp (sr->EntityName (), "Advanced_Brep_Shape_Representation") != 0)
    {
      printf ("step_model_to_shape_master: Looking for Advanced_Brep_Shape_Representation, but found %s (which we don't support yet)\n", sr->EntityName ());
      return NULL;
    }

//  object = make_object3d ((char *)"Test");

  step_model = g_new0(struct step_model, 1);
//  step_model->filename = g_strdup(filename);
//  step_model->instances = NULL;    /* ??? */

#if 0
  SdaiAxis2_placement_3d *part_origin = find_axis2_placement_3d_in_sr (sr);
  if (part_origin == NULL)
    std::cout << "WARNING: Could not find AXIS2_PLACEMENT_3D entity in SHAPE_REPRESENTATION" << std::endl;

  if (part_origin != NULL)
    {
      step_model->ox = ((RealNode *)part_origin->location_ ()->coordinates_ ()->GetHead ())->value;
      step_model->oy = ((RealNode *)part_origin->location_ ()->coordinates_ ()->GetHead ()->NextNode ())->value;
      step_model->oz = ((RealNode *)part_origin->location_ ()->coordinates_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      step_model->ax = ((RealNode *)part_origin->axis_ ()->direction_ratios_ ()->GetHead ())->value;
      step_model->ay = ((RealNode *)part_origin->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      step_model->az = ((RealNode *)part_origin->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      step_model->rx = ((RealNode *)part_origin->ref_direction_ ()->direction_ratios_ ()->GetHead ())->value;
      step_model->ry = ((RealNode *)part_origin->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      step_model->rz = ((RealNode *)part_origin->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;
    }
#endif

  msb_list msb_list;
  find_manifold_solid_brep (sr, &msb_list);

  for (msb_list::iterator iter = msb_list.begin (); iter != msb_list.end (); iter++)
    {
      std::cout << "Found MANIFOLD_SOLID_BREP; processing" << std::endl;
      SdaiClosed_shell *cs = (*iter)->outer_ ();

      std::cout << "Closed shell is " << cs << std::endl;

      /* NB: NULLs give g_direct_hash and g_direct_equal */
      edges_hash_set = g_hash_table_new (NULL, NULL);

      for (SingleLinkNode *iter = cs->cfs_faces_ ()->GetHead ();
           iter != NULL;
           iter = iter->NextNode ())
        {
          SdaiFace *face = (SdaiFace *)((EntityNode *)iter)->node;

          /* XXX: Do we look for specific types of face at this point? (Expect ADVANCED_FACE usually?) */
          if (strcmp (face->EntityName (), "Advanced_Face") != 0)
            {
              printf ("WARNING: Found face of type %s (which we don't support yet)\n", face->EntityName ());
              continue;
            }

          /* NB: ADVANCED_FACE is a FACE_SURFACE, which has SdaiSurface *face_geometry_ (), and Boolean same_sense_ () */
          // SdaiAdvanced_face *af = (SdaiAdvanced_face *) face;
          /* NB: FACE_SURFACE is a FACE, which has EntityAggreate bounds_ (), whos' members are SdaiFace_bound *  */
          SdaiFace_surface *fs = (SdaiFace_surface *) face;

          SdaiSurface *surface = fs->face_geometry_ ();

#if 0
          std::cout << "Face " << face->name_ ().c_str () << " has surface of type " << surface->EntityName () << " and same_sense = " << fs->same_sense_ () << std::endl;
#endif

          on_plane = false;

          if (surface->IsComplex ())
            {
#ifdef DEBUG_NOT_IMPLEMENTED
              printf ("WARNING: Found a STEP Complex entity for our surface (which we don't support yet). Probably a B_SPLINE surface?\n");
#endif
            }
          else if (strcmp (surface->EntityName (), "Plane") == 0)
            {
              on_plane = true;
//              printf ("WARNING: planar surfaces are not supported yet\n");
            }
          else if (strcmp (surface->EntityName (), "Cylindrical_Surface") == 0)
            {
//              printf ("WARNING: cylindrical suraces are not supported yet\n");
            }
          else if (strcmp (surface->EntityName (), "Toroidal_Surface") == 0)
            {
//              printf ("WARNING: toroidal suraces are not supported yet\n");
            }
          else if (strcmp (surface->EntityName (), "Spherical_Surface") == 0)
            {
//              printf ("WARNING: spherical surfaces are not supported yet\n");
            }
          else
            {
#ifdef DEBUG_NOT_IMPLEMENTED
              printf ("ERROR: Found an unknown surface type (which we obviously don't support). Surface name is %s\n", surface->EntityName ());
#endif
            }

          for (SingleLinkNode *iter = fs->bounds_ ()->GetHead ();
               iter != NULL;
               iter = iter->NextNode ())
            {
              SdaiFace_bound *fb = (SdaiFace_bound *)((EntityNode *)iter)->node;


#if 0
              bool is_outer_bound = (strcmp (fb->EntityName (), "Face_Outer_Bound") == 0);

              if (is_outer_bound)
                std::cout << "  Outer bounds of face include ";
              else
                std::cout << "  Bounds of face include ";
#endif

              // NB: SdaiFace_bound has SdaiLoop *bound_ (), and Boolean orientation_ ()
              // NB: SdaiLoop is a SdaiTopological_representation_item, which is a SdaiRepresentation_item, which has a name_ ().
              // NB: Expect bounds_ () may return a SUBTYPE of SdaiLoop, such as, but not necessarily: SdaiEdge_loop
              SdaiLoop *loop = fb->bound_ ();

#if 0
              std::cout << "loop #" << loop->StepFileId () << ", of type " << loop->EntityName () << ":" << std::endl;
#endif
              if (strcmp (loop->EntityName (), "Edge_Loop") == 0)
                {
                  SdaiEdge_loop *el = (SdaiEdge_loop *)loop;

                  // NB: EDGE_LOOP uses multiple inheritance from LOOP and PATH, thus needs special handling to
                  //     access the elements belonging to PATH, such as edge_list ...
                  //     (Not sure if this is a bug in STEPcode, as the SdaiEdge_loop class DOES define
                  //     an accessor edge_list_ (), yet it appears to return an empty aggregate.

                  char path_entity_name[] = "Path"; /* SdaiApplication_instance::GetMiEntity() should take const char *, but doesn't */
                  SdaiPath *path = (SdaiPath *)el->GetMiEntity (path_entity_name);

                  for (SingleLinkNode *iter = path->edge_list_ ()->GetHead ();
                       iter != NULL;
                       iter = iter->NextNode ())
                    {
                      SdaiOriented_edge *oe = (SdaiOriented_edge *)((EntityNode *)iter)->node;
                      /* XXX: Will it _always?_ be an SdaiOriented_edge? */

                      // NB: Stepcode does not compute derived attributes, so we need to look at the EDGE
                      //     "edge_element" referred to by the ORIENTED_EDGE, to find the start and end vertices

                      SdaiEdge *edge = oe->edge_element_ ();
                      bool orientation = oe->orientation_ ();

                      if (on_plane)
                        {
                          if (fs->same_sense_())
                            {
                              if (orientation)
                                g_hash_table_insert (edges_hash_set, edge, GINT_TO_POINTER(1));
                              else
                                g_hash_table_insert (edges_hash_set, edge, GINT_TO_POINTER(1));
                            }
                          else
                            {
                              if (orientation)
                                g_hash_table_insert (edges_hash_set, edge, GINT_TO_POINTER(1));
                              else
                                g_hash_table_insert (edges_hash_set, edge, GINT_TO_POINTER(1));
                            }
                        }
                      else
                        {
                          g_hash_table_insert (edges_hash_set, edge, GINT_TO_POINTER(1));
                        }


                      if (strcmp (edge->edge_start_ ()->EntityName (), "Vertex_Point") != 0 ||
                          strcmp (edge->edge_end_   ()->EntityName (), "Vertex_Point") != 0)
                        {
                          printf ("WARNING: Edge start and/or end vertices are not specified as VERTEX_POINT\n");
                          continue;
                        }

                      // NB: Assuming edge points to an EDGE, or one of its subtypes that does not make edge_start and edge_end derived attributes.
                      //     In practice, edge should point to an EDGE_CURVE sub-type
                      SdaiVertex_point *edge_start = (SdaiVertex_point *) (orientation ? edge->edge_start_ () : edge->edge_end_ ());
                      SdaiVertex_point *edge_end =  (SdaiVertex_point *) (!orientation ? edge->edge_start_ () : edge->edge_end_ ());

                      // NB: XXX: SdaiVertex_point multiply inherits from vertex and geometric_representation_item

                      SdaiPoint *edge_start_point = edge_start->vertex_geometry_ ();
                      SdaiPoint *edge_end_point = edge_end->vertex_geometry_ ();

                      if (strcmp (edge_start_point->EntityName (), "Cartesian_Point") == 0)
                        {
                          /* HAPPY WITH THIS TYPE */
                        }
                      else
                        {
                          // XXX: point_on_curve, point_on_surface, point_replica, degenerate_pcurve
                          printf ("WARNING: Got Edge start point as unhandled point type (%s)\n", edge_start_point->EntityName ());
                          continue;
                        }

                      if (strcmp (edge_end_point->EntityName (), "Cartesian_Point") == 0)
                        {
                          /* HAPPY WITH THIS TYPE */
                        }
                      else
                        {
                          // XXX: point_on_curve, point_on_surface, point_replica, degenerate_pcurve
                          printf ("WARNING: Got Edge end point as unhandled point type (%s)\n", edge_end_point->EntityName ());
                          continue;
                        }

#if 0
                      SdaiCartesian_point *edge_start_cp = (SdaiCartesian_point *)edge_start_point;
                      SdaiCartesian_point *edge_end_cp = (SdaiCartesian_point *)edge_end_point;

                      printf ("    Edge #%i starts at (%f, %f, %f) and ends at (%f, %f, %f)\n",
                              edge->StepFileId (),
                              ((RealNode *)edge_start_cp->coordinates_ ()->GetHead())->value,
                              ((RealNode *)edge_start_cp->coordinates_ ()->GetHead()->NextNode())->value,
                              ((RealNode *)edge_start_cp->coordinates_ ()->GetHead()->NextNode()->NextNode())->value,
                              ((RealNode *)edge_end_cp->coordinates_ ()->GetHead())->value,
                              ((RealNode *)edge_end_cp->coordinates_ ()->GetHead()->NextNode())->value,
                              ((RealNode *)edge_end_cp->coordinates_ ()->GetHead()->NextNode()->NextNode())->value);

                      if (strcmp (edge->EntityName (), "Edge_Curve") == 0)
                        {
                          SdaiEdge_curve *ec = (SdaiEdge_curve *)edge;

                          SdaiCurve *curve = ec->edge_geometry_ ();
                          bool same_sense = ec->same_sense_ ();

                          printf ("         underlying curve is %s #%i, same_sense is %s\n", curve->EntityName (), curve->StepFileId(), same_sense ? "True" : "False");

                          if (strcmp (curve->EntityName (), "Line") == 0)
                            {
//                              printf ("WARNING: Underlying curve geometry type Line is not supported yet\n");
//                              continue;
                            }
                          else if (strcmp (curve->EntityName (), "Circle") == 0)
                            {
//                              printf ("WARNING: Underlying curve geometry type circle is not supported yet\n");
//                              continue;
                            }
                          else
                            {
                              printf ("WARNING: Unhandled curve geometry type (%s), #%i\n", curve->EntityName (), curve->StepFileId ());
                              // XXX: line, conic, pcurve, surface_curve, offset_curve_2d, offset_curve_3d, curve_replica
                              // XXX: Various derived types of the above, e.g.:
                              //      conic is a supertype of: circle, ellipse, hyperbola, parabola
                              continue;
                            }

                        }
                      else
                        {
                          printf ("WARNING: found unknown edge type (%s)\n", edge->EntityName ());
                          continue;
                        }
#endif

                    }

                }
              else
                {
                  printf ("WARNING: Face is bounded by an unhandled loop type (%s)\n", loop->EntityName ());
                  continue;
                }
            }

        }

        process_edges (edges_hash_set, info); //object);

        /* Deal with edges hash set */
        g_hash_table_destroy (edges_hash_set);
    }

  mi_list mi_list;
  find_mapped_item (sr, &mi_list);

  for (mi_list::iterator iter = mi_list.begin (); iter != mi_list.end (); iter++)
    {
      std::cout << "Found MAPPED_ITEM; processing" << std::endl;
      SdaiMapped_item *mi = (*iter);

      SdaiRepresentation_map *rm = dynamic_cast<SdaiRepresentation_map *>(mi->mapping_source_());
      SdaiAxis2_placement_3d *mi_axis = dynamic_cast<SdaiAxis2_placement_3d *>(mi->mapping_target_());

      if (rm == NULL)
        {
          std::cout << "ERROR: Could not find REPRESENTATION_ITEM referred to by MAPPED_ITEM" << std::endl;
          continue;
        }

      if (mi_axis == NULL)
        {
          std::cout << "ERROR: Could not find AXIS2_PLACEMENT_3D referred to by MAPPED_ITEM" << std::endl;
          continue;
        }

      SdaiAxis2_placement_3d *rm_axis = dynamic_cast<SdaiAxis2_placement_3d *>(rm->mapping_origin_());
      SdaiShape_representation *child_sr = dynamic_cast<SdaiShape_representation *>(rm->mapped_representation_());

      if (rm_axis == NULL)
        {
          std::cout << "ERROR: Could not find AXIS2_PLACEMENT_3D referred to by REPRESENTATION_ITEM" << std::endl;
          continue;
        }

      if (child_sr == NULL)
        {
          std::cout << "ERROR: Could not find SHAPE_REPRESENTATION referred to by MAPPED_ITEM" << std::endl;
          continue;
        }

#if 0
      /* XXX: Actually only want to "process" the SR once per SR, then create _instances_ of it */
      /* XXX: Origin offset etc..? */
      /* XXX: Do something with the result */
      process_sr_or_subtype (instance_list, child_sr, info);
#endif

      double backup_transform[4][4];
      double ox, oy, oz;
      double ax, ay, az;
      double rx, ry, rz;
      SdaiAxis2_placement_3d *parent_axis = rm_axis;
      SdaiAxis2_placement_3d *child_axis = mi_axis;

      copy_4x4 (info->current_transform, backup_transform);

      if (parent_axis == NULL ||
          child_axis == NULL)
        {
          std::cout << "ERROR: Got NULL in one of the axis placements for IDT" << std::endl;
          continue;
        }

      ox = ((RealNode *)child_axis->location_ ()->coordinates_ ()->GetHead ())->value;
      oy = ((RealNode *)child_axis->location_ ()->coordinates_ ()->GetHead ()->NextNode ())->value;
      oz = ((RealNode *)child_axis->location_ ()->coordinates_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      ax = ((RealNode *)child_axis->axis_ ()->direction_ratios_ ()->GetHead ())->value;
      ay = ((RealNode *)child_axis->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      az = ((RealNode *)child_axis->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      rx = ((RealNode *)child_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ())->value;
      ry = ((RealNode *)child_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      rz = ((RealNode *)child_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;

      /* XXX: Looking only at the target vector.. need to find some examples where the parent transform coordinate system is not unity to get this correct */
      rotate_basis (info->current_transform, ax, ay, az, rx, ry, rz);
      translate_origin (info->current_transform, ox, oy, oz);

      ox = ((RealNode *)parent_axis->location_ ()->coordinates_ ()->GetHead ())->value;
      oy = ((RealNode *)parent_axis->location_ ()->coordinates_ ()->GetHead ()->NextNode ())->value;
      oz = ((RealNode *)parent_axis->location_ ()->coordinates_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      ax = ((RealNode *)parent_axis->axis_ ()->direction_ratios_ ()->GetHead ())->value;
      ay = ((RealNode *)parent_axis->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      az = ((RealNode *)parent_axis->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      rx = ((RealNode *)parent_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ())->value;
      ry = ((RealNode *)parent_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      rz = ((RealNode *)parent_axis->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;

      rotate_basis_inverted (info->current_transform, ax, ay, az, rx, ry, rz);
      translate_origin (info->current_transform, -ox, -oy, -oz);

      /* XXX: Actually only want to "process" the SR once per SR, then create _instances_ of it */
      /* XXX: Origin offset etc..? */
      /* XXX: Do something with the result */
      process_sr_or_subtype (instance_list, child_sr, info);

      // Revert the transformation
      copy_4x4 (backup_transform, info->current_transform);
    }

  step_model->object = info->object;

  return step_model;
}

extern "C" struct step_model *
step_model_to_shape_master (const char *filename)
{
  step_model *step_model;
  process_step_info info;

  printf ("step_model_to_shape_master(\"%s\")\n", filename);

  Registry * registry = new Registry (SchemaInit);
  InstMgr * instance_list = new InstMgr (/* ownsInstance = */1);

  // Increment FileId so entities start at #1 instead of #0.
  instance_list->NextFileId();

  SdaiProduct_definition *pd = read_model_from_file (registry, instance_list, filename);
  if (pd == NULL)
    {
      printf ("ERROR Loading STEP model from file '%s'", filename);
      return NULL;
    }

  SdaiShape_definition_representation *sdr = find_sdr_for_pd (instance_list, pd);
  SdaiShape_representation *sr = (SdaiShape_representation *)sdr->used_representation_ ();

  info.object = make_object3d ((char *)"Test");
  identity_4x4 (info.current_transform);

  step_model = process_sr_or_subtype (instance_list, sr, &info);

  /* KLUDGE */
  SdaiAxis2_placement_3d *part_origin = find_axis2_placement_3d_in_sr (sr);
  if (part_origin == NULL)
    std::cout << "WARNING: Could not find AXIS2_PLACEMENT_3D entity in SHAPE_REPRESENTATION" << std::endl;

  if (part_origin != NULL)
    {
      step_model->ox = ((RealNode *)part_origin->location_ ()->coordinates_ ()->GetHead ())->value;
      step_model->oy = ((RealNode *)part_origin->location_ ()->coordinates_ ()->GetHead ()->NextNode ())->value;
      step_model->oz = ((RealNode *)part_origin->location_ ()->coordinates_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      step_model->ax = ((RealNode *)part_origin->axis_ ()->direction_ratios_ ()->GetHead ())->value;
      step_model->ay = ((RealNode *)part_origin->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      step_model->az = ((RealNode *)part_origin->axis_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;
      step_model->rx = ((RealNode *)part_origin->ref_direction_ ()->direction_ratios_ ()->GetHead ())->value;
      step_model->ry = ((RealNode *)part_origin->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ())->value;
      step_model->rz = ((RealNode *)part_origin->ref_direction_ ()->direction_ratios_ ()->GetHead ()->NextNode ()->NextNode ())->value;
    }

  /* KLUDGE */
  step_model->object = info.object;

  delete instance_list;
  delete registry;

  return step_model;
}

void step_model_free(step_model *step_model)
{
//  g_list_free (step_model->instances);
//  g_free ((char *)step_model->filename);
  destroy_object3d (step_model->object);
  g_free (step_model);
}

/* Geometry surface and face types encountered so far..

Toroidal_surface     Circle (x5)
Toroidal_surface     Circle (x4)
Toroidal_surface     Circle (x3) + B_Spline_Curve_With_Knots

Cylindrical_surface  Circle + Line + B_Spline_Curve_With_Knots
Cylindrical_surface  Circle + Line

Plane                Circle (xn) + Line (xn) + B_Spline_Curve_With_Knots

*/
