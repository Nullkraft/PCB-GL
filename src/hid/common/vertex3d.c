#include <glib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "step_id.h"
#include "vertex3d.h"

static int global_vertex3d_count;


vertex3d *
make_vertex3d (double x, double y, double z)
{
  vertex3d *v;

  v = malloc (sizeof(vertex3d));
  v->x = x;
  v->y = y;
  v->z = z;
  v->id = global_vertex3d_count++;

  return v;
}

void
destroy_vertex3d (vertex3d *v)
{
  free (v);
}
