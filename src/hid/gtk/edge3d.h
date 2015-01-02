typedef struct
{
  bool is_stitch;
  bool is_round;
  float cx;
  float cy;
  float radius;
} edge_info;

edge_info *make_edge_info (bool is_stitch, bool is_round, float cx, float cy, float radius);
void destroy_edge_info (edge_info *info);
