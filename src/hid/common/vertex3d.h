typedef struct
{
  float x;
  float y;
  float z;
  int id;

  /* XXX: STEP specific - breaks encapsulation */
  int vertex_identifier;
} vertex3d;

vertex3d *make_vertex3d (float x, float y, float z);
void destroy_vertex3d (vertex3d *v);
