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

#ifndef __HIDGL_MATERIAL_INCLUDED__
#define __HIDGL_MATERIAL_INCLUDED__

#include "hidgl_shaders.h"

typedef struct _hidgl_material hidgl_material;

hidgl_material *hidgl_material_new (char *name);
void hidgl_material_free (hidgl_material *material);
void hidgl_material_activate (hidgl_material *material);

void hidgl_material_set_ambient_color     (hidgl_material *material, GLfloat color[4]);
void hidgl_material_set_diffuse_color     (hidgl_material *material, GLfloat color[4]);
void hidgl_material_set_emission_color    (hidgl_material *material, GLfloat color[4]);
void hidgl_material_set_specular_color    (hidgl_material *material, GLfloat color[4]);
void hidgl_material_set_shininess         (hidgl_material *material, float shininess);
void hidgl_material_set_transparent_color (hidgl_material *material, GLfloat color[4]);   /* Not used? */
void hidgl_material_set_transparency      (hidgl_material *material, float transparency); /* Not used? */
void hidgl_material_set_reflective_color  (hidgl_material *material, GLfloat color[4]);   /* Not used? */
void hidgl_material_set_reflectivity      (hidgl_material *material, float reflectivity); /* Not used? */
void hidgl_material_set_shader            (hidgl_material *material, hidgl_shader *program);
/* XXX: Textures? */

void hidgl_material_get_ambient_color     (hidgl_material *material, GLfloat *color);
void hidgl_material_get_diffuse_color     (hidgl_material *material, GLfloat *color);
void hidgl_material_get_emission_color    (hidgl_material *material, GLfloat *color);
void hidgl_material_get_specular_color    (hidgl_material *material, GLfloat *color);
float hidgl_material_get_shininess        (hidgl_material *material);
void hidgl_material_get_transparent_color (hidgl_material *material, GLfloat *color); /* Not used? */
float hidgl_material_get_transparency     (hidgl_material *material);                 /* Not used? */
void hidgl_material_get_reflective_color  (hidgl_material *material, GLfloat *color); /* Not used? */
float hidgl_material_get_reflectivity     (hidgl_material *material);                 /* Not used? */
hidgl_shader * hidgl_material_get_shader  (hidgl_material *material);


#endif /* __HIDGL_MATERIAL_INCLUDED__  */
