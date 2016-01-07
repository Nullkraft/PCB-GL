typedef struct
{
  /* For edge curves */
  bool is_stitch; /* Allows us to identify the stitch edge along the side of a cylinder */

  /* For circular curves */
  bool is_round;
  float cx;
  float cy;
  float cz;
  float nx;
  float ny;
  float nz;
  float radius;

  /* STEP crap - to hell with encapsulation */
  step_id infinite_line_identifier;
  step_id edge_identifier;
} edge_info;

edge_info *make_edge_info (void);
void edge_info_set_round (edge_info *info, float cx, float cy, float cz, float nx, float ny, float nz, float radius);
void edge_info_set_stitch (edge_info *info);
void destroy_edge_info (edge_info *info);
