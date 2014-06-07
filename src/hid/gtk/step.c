#include <glib.h>
#include <stdbool.h>
#include <stdio.h>

#include "step.h"

static char *
step_bool (bool expr)
{
  return expr ? ".T." : ".F.";
}

step_file
*step_output_file (FILE *f)
{
  step_file *file;

  file = g_new0 (step_file, 1);
  file->f = f;
  file->next_id = 1;

  return file;
}

void
destroy_step_output_file (step_file *file)
{
  g_free (file);
}

step_id
step_cartesian_point (step_file *file, char *name, double x, double y, double z)
{
  fprintf (file->f, "#%i = CARTESIAN_POINT ( '%s', ( %f, %f, %f ) ) ;\n",
                    file->next_id, name, x, y, z);
  return file->next_id++;
}

step_id
step_direction (step_file *file, char *name, double x, double y, double z)
{
  fprintf (file->f, "#%i = DIRECTION ( '%s', ( %f, %f, %f ) ) ;\n",
                    file->next_id, name, x, y, z);
  return file->next_id++;
}

step_id
step_axis2_placement_3d (step_file *file, char *name, step_id location, step_id axis, step_id ref_direction)
{
  fprintf (file->f, "#%i = AXIS2_PLACEMENT_3D ( '%s', ( #%i, #%i, #%i ) ) ;\n",
                    file->next_id, name, location, axis, ref_direction);
  return file->next_id++;
}

step_id
step_plane (step_file *file, char *name, step_id position)
{
  fprintf (file->f, "#%i = PLANE ( '%s', #%i ) ;\n",
                    file->next_id, name, position);
  return file->next_id++;
}

step_id
step_cylindrical_surface (step_file *file, char *name, step_id position, double radius)
{
  fprintf (file->f, "#%i = CYLINDRICAL_SURFACE ( '%s', #%i, %f ) ;\n",
                    file->next_id, name, position, radius);
  return file->next_id++;
}

step_id
step_circle (step_file *file, char *name, step_id position, double radius)
{
  fprintf (file->f, "#%i = CIRCLE ( '%s', #%i, %f ) ;\n",
                    file->next_id, name, position, radius);
  return file->next_id++;
}

step_id
step_vector (step_file *file, char *name, step_id orientation, double magnitude)
{
  fprintf (file->f, "#%i = VECTOR ( '%s', #%i, %f ) ;\n",
                    file->next_id, name, orientation, magnitude);
  return file->next_id++;
}

step_id
step_line (step_file *file, char *name, step_id pnt, step_id dir)
{
  fprintf (file->f, "#%i = LINE ( '%s', #%i, %i ) ;\n",
                    file->next_id, name, pnt, dir);
  return file->next_id++;
}

step_id
step_vertex_point (step_file *file, char *name, step_id pnt)
{
  fprintf (file->f, "#%i = VERTEX_POINT ( '%s', #%i ) ;\n",
                    file->next_id, name, pnt);
  return file->next_id++;
}

step_id
step_edge_curve (step_file *file, char *name, step_id edge_start, step_id edge_end, step_id edge_geometry, bool same_sense)
{
  fprintf (file->f, "#%i = EDGE_CURVE ( '%s', #%i, #%i, #%i, %s ) ;\n",
                    file->next_id, name, edge_start, edge_end, edge_geometry, step_bool (same_sense));
  return file->next_id++;
}

step_id
step_oriented_edge (step_file *file, char *name, step_id edge_element, bool orientation)
{
  fprintf (file->f, "#%i = ORIENTED_EDGE ( '%s', *, *, #%i, %s ) ;\n",
                    file->next_id, name, edge_element, step_bool (orientation));
  return file->next_id++;
}
