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
  int infinite_line_identifier;
  int edge_identifier;
} edge_info;

edge_info *make_edge_info (bool is_stitch, bool is_round, float cx, float cy, float cz, float nx, float ny, float nz, float radius);
void destroy_edge_info (edge_info *info);
