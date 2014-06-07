#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "step.h"
#include "edge3d.h"

edge_info *
make_edge_info (void)
{
  edge_info *info;

  info = g_new0 (edge_info, 1);

  return info;
}

void
edge_info_set_round (edge_info *info, float cx, float cy, float cz, float nx, float ny, float nz, float radius)
{
  info->is_round = true;
  info->cx = cx;
  info->cy = cy;
  info->cz = cz;
  info->nx = nx;
  info->ny = ny;
  info->nz = nz;
  info->radius = radius;
}

void edge_info_set_stitch (edge_info *info)
{
  info->is_stitch = true;
}

void
destroy_edge_info (edge_info *info)
{
  g_free (info);
}
