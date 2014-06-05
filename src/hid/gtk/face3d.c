#include <glib.h>
#include <stdbool.h>

#include "quad.h"
#include "contour3d.h"
#include "appearance.h"
#include "face3d.h"

face3d *
make_face3d (void)
{
  face3d *face;

  face = g_new0 (face3d, 1);

  return face;
}

void
destroy_face3d (face3d *face)
{
  g_list_free_full (face->contours, (GDestroyNotify)destroy_contour3d);
  g_free (face);
}

#if 0
void
face3d_add_contour (face3d *face, edge_ref contour)
{
  face->contours = g_list_append (face->contours, (void *)contour);
}
#endif

void
face3d_add_contour (face3d *face, contour3d *contour)
{
  face->contours = g_list_append (face->contours, contour);
}

void
face3d_set_appearance (face3d *face, appearance *appear)
{
  face->appear = appear;
}
