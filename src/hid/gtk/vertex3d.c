#include <stdlib.h>

static int global_vertex3d_count;

typedef struct
{
  float x;
  float y;
  float z;
  int id;

  /* STEP crap - to hell with encapsulation */
  int vertex_identifier;
} vertex3d;

vertex3d *
make_vertex3d (float x, float y, float z)
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
