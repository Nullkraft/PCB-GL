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
#include "hidgl_material.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

typedef struct {
  GLfloat value[4];
} _color;

/* Opaque data-structure keeping a material object */
struct _hidgl_material {
  char *name;

  _color ambient_color;
  _color diffuse_color;
  _color emission_color;
  _color specular_color;
  float shininess;

  /* Potentially unused by the renderer */
  _color transparent_color; /* NB: From collada, not used */
  float transparency;       /* NB: From collada, not used */
  _color reflective_color;  /* NB: From collada, not used */
  float reflectivity;       /* NB: From collada, not used */

  hidgl_shader *shader_program;
};


hidgl_material *
hidgl_material_new (char *name)
{
  hidgl_material *material;
  _color black;
  _color white;

  material = malloc (sizeof (hidgl_material));

  if (material == NULL)
    return NULL;

  black.value[0] = 0.;
  black.value[1] = 0.;
  black.value[2] = 0.;
  black.value[3] = 0.;

  white.value[0] = 1.;
  white.value[1] = 1.;
  white.value[2] = 1.;
  white.value[3] = 1.;

  material->name = strdup (name);
  material->ambient_color = white;
  material->diffuse_color = white;
  material->emission_color = black;
  material->specular_color = white;
  material->shininess = 0.;
  material->transparent_color = white;
  material->transparency = 0.;
  material->reflective_color = white;
  material->reflectivity = 0.;
  material->shader_program = NULL;

  return material;
}


/* Delete the passed material. */
void
hidgl_material_free (hidgl_material *material)
{
  free (material->name);
  free (material);
}


/* Set the given material properties */
void
hidgl_material_activate (hidgl_material *material)
{
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT,   material->ambient_color.value);
  glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE,   material->diffuse_color.value);
  glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION,  material->emission_color.value);
  glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  material->specular_color.value);
  glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS, &material->shininess);

  /* XXX: Unused parameter: material->transparent_color */
  /* XXX: Unused parameter: material->reflection_color  */
  /* XXX: Unused parameter: material->transparency      */
  /* XXX: Unused parameter: material->reflectivity      */

  hidgl_shader_activate (material->shader_program);
}

void
hidgl_material_set_ambient_color (hidgl_material *material, GLfloat color[4])
{
  material->ambient_color = *(_color *)color;
}

void hidgl_material_set_diffuse_color (hidgl_material *material, GLfloat color[4])
{
  material->diffuse_color = *(_color *)color;
}

void
hidgl_material_set_emission_color (hidgl_material *material, GLfloat color[4])
{
  material->emission_color = *(_color *)color;
}

void
hidgl_material_set_specular_color (hidgl_material *material, GLfloat color[4])
{
  material->specular_color = *(_color *)color;
}

void
hidgl_material_set_shininess (hidgl_material *material, float shininess)
{
  material->shininess = shininess;
}

void
hidgl_material_set_transparent_color (hidgl_material *material, GLfloat color[4])
{
  material->transparent_color = *(_color *)color;
}

void
hidgl_material_set_transparency (hidgl_material *material, float transparency)
{
  material->transparency = transparency;
}

void
hidgl_material_set_reflective_color (hidgl_material *material, GLfloat color[4])
{
  material->reflective_color = *(_color *)color;
}

void
hidgl_material_set_reflectivity (hidgl_material *material, float reflectivity)
{
  material->reflectivity = reflectivity;
}

void
hidgl_material_set_shader (hidgl_material *material, hidgl_shader *program)
{
  material->shader_program = program;
}


void
hidgl_material_get_ambient_color (hidgl_material *material, GLfloat *color)
{
  *(_color *)color = material->ambient_color;
}

void
hidgl_material_get_diffuse_color (hidgl_material *material, GLfloat *color)
{
  *(_color *)color = material->diffuse_color;
}

void
hidgl_material_get_emission_color (hidgl_material *material, GLfloat *color)
{
  *(_color *)color = material->emission_color;
}

void
hidgl_material_get_specular_color (hidgl_material *material, GLfloat *color)
{
  *(_color *)color = material->specular_color;
}

float
hidgl_material_get_shininess (hidgl_material *material)
{
  return material->shininess;
}

void
hidgl_material_get_transparent_color (hidgl_material *material, GLfloat *color)
{
  *(_color *)color = material->transparent_color;
}

float
hidgl_material_get_transparency (hidgl_material *material)
{
  return material->transparency;
}

void
hidgl_material_get_reflective_color (hidgl_material *material, GLfloat *color)
{
  *(_color *)color = material->reflective_color;
}

float
hidgl_material_get_reflectivity (hidgl_material *material)
{
  return material->reflectivity;
}

hidgl_shader *
hidgl_material_get_shader (hidgl_material *material)
{
  return material->shader_program;
}
