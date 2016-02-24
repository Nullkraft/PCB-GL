typedef struct {
  edge_ref first_edge;

  /* STEP crap - to hell with encapsulation */
  step_id face_bound_identifier;
} contour3d;

contour3d *make_contour3d (edge_ref first_edge);
void destroy_contour3d (contour3d *contour);