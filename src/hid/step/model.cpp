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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if 0
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
append_model_from_file (Registry *registry,
                        InstMgr *instance_list,
                        const char *filename)
{
  int max_existing_file_id = instance_list->MaxFileId ();

  /* XXX: The following line is coppied from STEPfile.inline.cc, and we rely on it matching the algorithm there! */
//  int file_increment = ( int )( ( ceil( ( max_existing_file_id + 99.0 ) / 1000.0 ) + 1.0 ) * 1000.0 ); /* XXX: RELYING ON SCL NOT CHANGING */
//  std::cout << "INFO: Expecting a to add " << file_increment << " to entity names" << std::endl;

  STEPfile sfile = STEPfile (*registry, *instance_list, "", false);

  sfile.AppendExchangeFile (filename);

  Severity severity = sfile.Error().severity();
  if (severity != SEVERITY_NULL)
    {
      sfile.Error().PrintContents (std::cout);
      std::cout << "WARNING: Error reading from file '" << filename << "'" << std::endl;
//      return NULL;
#warning HANDLE OTHER ERRORS BETTER?
    }

  pd_list all_pd_list;
  pd_list pd_list;

  // Find all PRODUCT_DEFINITION entities with a SHAPE_DEFINITION_REPRESETNATION
  find_all_pd_with_sdr (instance_list, &all_pd_list);

  // Find and copy over any PRODUCT_DEFINITION in our list which have entity numbers from the append
  for (pd_list::iterator iter = all_pd_list.begin(); iter != all_pd_list.end(); iter++)
    if ((*iter)->StepFileId () > max_existing_file_id)
      pd_list.push_back (*iter);

  /*  Try to determine the root product */
  find_and_remove_child_pd (instance_list, &pd_list, "Next_assembly_usage_occurrence"); // Remove any PD which are children of another via NAUO
  find_and_remove_child_pd (instance_list, &pd_list, "Assembly_component_usage");       // Remove any PD which are children of another via ACU

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


extern "C" struct step_model *
step_model_to_shape_master (const char *filename)
{
  Registry * registry = new Registry (SchemaInit);
  InstMgr * instance_list = new InstMgr (/* ownsInstance = */1);

#if 0
  // Increment FileId so entities start at #1 instead of #0.
  instance_list->NextFileId();

  SdaiProduct_definition *assembly_pd = create_parent_assembly (registry, instance_list);
  if (assembly_pd == NULL)
    {
      printf ("ERROR creating parent assembly");
      return;
    }

  GList *model_iter;
  for (model_iter = models;
       model_iter != NULL;
       model_iter = g_list_next (model_iter))
    {
      struct assembly_model *model = (struct assembly_model *)model_iter->data;

      SdaiProduct_definition *model_pd;
      model_pd = append_model_from_file (registry, instance_list, model->filename);
      if (model_pd == NULL)
        {
          printf ("ERROR Loading STEP model from file '%s'", model->filename);
          continue;
        }

      GList *inst_iter;
      for (inst_iter = model->instances;
           inst_iter != NULL;
           inst_iter = g_list_next (inst_iter))
        {
          struct assembly_model_instance *instance = (struct assembly_model_instance *)inst_iter->data;

          SdaiAxis2_placement_3d *child_location;
          child_location = MakeAxis (registry, instance_list,
                                     instance->ox, instance->oy, instance->oz,  // POINT
                                     instance->ax, instance->ay, instance->az,  // AXIS
                                     instance->rx, instance->ry, instance->rz); // REF DIRECTION

          assemble_instance_of_model (registry, instance_list, assembly_pd, model_pd, child_location, instance->name);
        }
    }

  write_ap214 (registry, instance_list, filename);
#endif

  delete instance_list;
  delete registry;

  return NULL;
}
