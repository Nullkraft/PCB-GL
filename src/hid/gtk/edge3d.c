#include <stdbool.h>
#include <stdlib.h>

#include "edge3d.h"

edge_info *
make_edge_info (bool is_stitch, bool is_round, float cx, float cy, float cz, float nx, float ny, float nz, float radius)
{
  edge_info *info;

  info = malloc (sizeof(edge_info));
  info->is_stitch = is_stitch;
  info->is_round = is_round;
  info->cx = cx;
  info->cy = cy;
  info->cy = cz;
  info->nx = nx;
  info->ny = ny;
  info->ny = nz;
  info->radius = radius;

  return info;
}

void
destroy_edge_info (edge_info *info)
{
  free (info);
}
