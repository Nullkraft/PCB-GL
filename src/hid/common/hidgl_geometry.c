/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 2010 PCB Contributors (See ChangeLog for details).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* The Linux OpenGL ABI 1.0 spec requires that we define
 * GL_GLEXT_PROTOTYPES before including gl.h or glx.h for extensions
 * in order to get prototypes:
 *   http://www.opengl.org/registry/ABI/
 */
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glu.h>
#include "hidgl_geometry.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

/* Opaque data-structure keeping a geometry object */
struct _hidgl_geometry {
  char *name;

  hidgl_geometry_class *klass;
};


hidgl_geometry *
hidgl_geometry_new (char *name)
{
  hidgl_geometry *geometry;

  geometry = malloc (sizeof (hidgl_geometry));

  if (geometry == NULL)
    return NULL;

  geometry->name = strdup (name);
  geometry->klass = NULL; /* To be filled in by the subclass */

  return geometry;
}


/* Delete the passed geometry. */
void
hidgl_geometry_free (hidgl_geometry *geometry)
{
  if (geometry->klass != NULL)
    geometry->klass->free (geometry);

  free (geometry->name);
  free (geometry);
}


/* Draw the given geometry */
void
hidgl_geometry_draw (hidgl_geometry *geometry)
{
  if (geometry->klass != NULL)
    geometry->klass->draw (geometry);
}
