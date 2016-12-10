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

#include <glib.h>

extern "C" {
#include "model.h"
}


typedef std::list<SDAI_Application_instance *> ai_list;


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

typedef std::list<SdaiManifold_solid_brep *> msb_list;

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

static void process_edges (GHashTable *edges_hash_set, object3d *object)
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

          if (!same_sense)
            printf ("XXX: HAVE NOT TESTED THIS CASE.... same_sense is false\n");

#if 0
          printf ("         underlying curve is %s #%i, same_sense is %s\n", curve->EntityName (), curve->StepFileId(), same_sense ? "True" : "False");
#endif

          if (strcmp (curve->EntityName (), "Line") == 0)
            {
              our_edge = make_edge ();
              UNDIR_DATA (our_edge) = make_edge_info ();
              object3d_add_edge (object, our_edge);
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

              edge_info *info;

              our_edge = make_edge ();
              info = make_edge_info ();
              if (!kludge) //(same_sense)
                {
                  edge_info_set_round (info, cx, cy, cz, nx, ny, nz, radius);
                }
              else
                {
                  printf ("URM................\n");
                  edge_info_set_round (info, cx, cy, cz, -nx, -ny, -nz, radius);
                }
              UNDIR_DATA (our_edge) = info;
              object3d_add_edge (object, our_edge);
              vertex = make_vertex3d (x1, y1, z1);
              ODATA(our_edge) = vertex;
              vertex = make_vertex3d (x2, y2, z2);
              DDATA(our_edge) = vertex;

//              printf ("WARNING: Underlying curve geometry type circle is not supported yet\n");
//              continue;
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
    }
}

extern "C" struct step_model *
step_model_to_shape_master (const char *filename, object3d **out_object)
{
  object3d *object;
  GHashTable *edges_hash_set;
  bool on_plane;
  step_model *step_model;

  printf ("step_model_to_shape_master(\"%s\", %p)\n", filename, out_object);

  object = make_object3d ((char *)"Test");

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

  // If sr is an exact match for the step entity SHAPE_REPRESENTATION (not a subclass), return - we are already in the correct form
  if (strcmp (sr->EntityName (), "Advanced_Brep_Shape_Representation") != 0)
    {
      printf ("step_model_to_shape_master: Looking for Advanced_Brep_Shape_Representation, but found %s (which we don't support yet)\n", sr->EntityName ());
      return NULL;
    }

  SdaiAxis2_placement_3d *part_origin = find_axis2_placement_3d_in_sr (sr);
  if (part_origin == NULL)
    std::cout << "WARNING: Could not find AXIS2_PLACEMENT_3D entity in SHAPE_REPRESENTATION" << std::endl;

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
              printf ("WARNING: Found a STEP Complex entity for our surface (which we don't support yet). Probably a B_SPLINE surface?\n");
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
              printf ("ERROR: Found an unknown surface type (which we obviously don't support). Surface name is %s\n", surface->EntityName ());
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

        process_edges (edges_hash_set, object);

        /* Deal with edges hash set */
        g_hash_table_destroy (edges_hash_set);
    }

  step_model = g_new0(struct step_model, 1);

//  step_model->filename = g_strdup(filename);
//  step_model->instances = NULL;    /* ??? */

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

  step_model->object = object;

  if (out_object != NULL)
    *out_object = object;

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
