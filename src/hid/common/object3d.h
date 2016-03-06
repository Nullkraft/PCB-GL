typedef struct {
  edge_ref first_edge;
  int id;
} object3d;

void object3d_test_init (void);
void object3d_draw_debug (void);
object3d *make_object3d (void);
void destroy_object3d (object3d *object);
object3d *object3d_create_test_cube (void);
object3d *object3d_from_board_outline (void);
