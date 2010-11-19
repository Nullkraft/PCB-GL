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

#ifndef __HIDGL_GEOMETRY_INCLUDED__
#define __HIDGL_GEOMETRY_INCLUDED__

typedef struct _hidgl_geometry        hidgl_geometry;
typedef struct _hidgl_geometry_class  hidgl_geometry_class;

struct _hidgl_geometry_class {
  void (*draw) (hidgl_geometry *geometry);   /* What assumptions are made about the GL state prior to calling this? */
  void (*free) (hidgl_geometry *geometry);   /* Clean up any internal state and delete */
};

/* Subclasses required / planned / thought of :
 *
 * hidgl_tristrip_geometry
 * hidgl_triangle_geometry
 * hidgl_mesh_geometry (EXAMPLE)
 * hidgl_brep_geometry (EXAMPLE)
 */

#if 0
tristrip_geometry and traiangle geometry probably share some common data-storage stuff?
Should we do like VRML and keep data-definition separate from the defining geometry?
Should we create a data-storage class for reading the meshes / vertex data into?

Need something which looks a little like the following:

Group / transform node
  (Transform properties)
  (Children)-+------------> Shape -+-> Appearance / material
             |                     `-> Geometry
             |------------> Shape -+-> Appearance / material
             |                     `-> Geometry
             |------------> Shape -+-> Appearance / material
             |                     `-> Geometry
             `---------> Transform
                            (Children)-+---> Shape -+-> Appearance / material
                                       |            `-> Geometry
                                       `---> Shape -+-> Appearance / material
                                                    `-> Geometry
#endif


hidgl_geometry *hidgl_geometry_new (char *name);
void hidgl_geometry_free (hidgl_geometry *geometry);
void hidgl_geometry_draw (hidgl_geometry *geometry);

#endif /* __HIDGL_GEOMETRY_INCLUDED__  */
