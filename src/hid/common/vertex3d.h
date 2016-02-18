typedef struct
{
  double x;
  double y;
  double z;
  int id;

  /* STEP crap - to hell with encapsulation */
  step_id vertex_identifier;
} vertex3d;

vertex3d *make_vertex3d (double x, double y, double z);
void destroy_vertex3d (vertex3d *v);
