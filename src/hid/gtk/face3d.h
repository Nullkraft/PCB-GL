typedef struct {
  float nx, ny, nz; /* Face normal?*/
  GList *contours;

  /* For cylindrical surfaces */
  bool is_cylindrical;
  float cx, cy, cz; /* A point on the axis */
  float ax, ay, az; /* Direction of the axis */
  float radius;

  appearance *appear;

  /* STEP crap - to hell with encapsulation */
  int surface_identifier;
  bool surface_orientation_reversed;
  int face_identifier;
  int face_bound_identifier;
} face3d;

face3d *make_face3d (void);
void destroy_face3d (face3d *face);
void face3d_add_contour (face3d *face, contour3d *contour);
void face3d_set_appearance (face3d *face, appearance *appear);
void face3d_set_normal (face3d *face, float nx, float ny, float nz);
void face3d_set_cylindrical (face3d *face, float cx, float cy, float cz, float ax, float ay, float az, float radius);
void face3d_set_surface_orientation_reversed (face3d *face);
