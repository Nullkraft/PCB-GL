typedef int step_id;

typedef struct {
  FILE *f;
  step_id next_id;

} step_file;

step_file *step_output_file (FILE *f);
void destroy_step_output_file (step_file *file);

step_id step_cartesian_point (step_file *file, char *name, double x, double y, double z);
step_id step_direction (step_file *file, char *name, double x, double y, double z);
step_id step_axis2_placement_3d (step_file *file, char *name, step_id location, step_id axis, step_id ref_direction);
step_id step_plane (step_file *file, char *name, step_id position);
step_id step_cylindrical_surface (step_file *file, char *name, step_id position, double radius);
step_id step_circle (step_file *file, char *name, step_id position, double radius);
step_id step_vector (step_file *file, char *name, step_id orientation, double magnitude);
step_id step_line (step_file *file, char *name, step_id pnt, step_id dir);
step_id step_vertex_point (step_file *file, char *name, step_id pnt);
step_id step_edge_curve (step_file *file, char *name, step_id edge_start, step_id edge_end, step_id edge_geometry, bool same_sense);
step_id step_oriented_edge (step_file *file, char *name, step_id edge_element, bool orientation);
