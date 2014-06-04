typedef struct {
  float nx, ny, nz; /* Face normal?*/
  GList *contours;

  /* For cylindrical surfaces */
  bool is_cylindrical;
  float cx, cy, cz; /* A point on the axis */
  float ax, ay, az; /* Direction of the axis */
  float radius;

  /* STEP crap - to hell with encapsulation */
  int plane_identifier;
  bool plane_orientation_reversed;
  int face_identifier;
  int face_bound_identifier;
} face3d;

face3d *make_face3d (void);
void destroy_face3d (face3d *face);
//void face3d_add_contour (face3d *face, edge_ref contour);
void face3d_add_contour (face3d *face, contour3d *contour);
