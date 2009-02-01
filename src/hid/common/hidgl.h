/* $Id: gui.h,v 1.25 2008-04-13 14:15:38 petercjclifton Exp $ */

/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996 Thomas Nau
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

#ifndef __HIDGL_INCLUDED__
#define __HIDGL_INCLUDED__

//#include <GL/gl.h>
//#include <GL/glu.h>

#define TRIANGLE_ARRAY_SIZE 5000
typedef struct {
  GLfloat triangle_array [2 * 3 * TRIANGLE_ARRAY_SIZE];
  unsigned int triangle_count;
  unsigned int coord_comp_count;
} triangle_buffer;

void hidgl_init_triangle_array (triangle_buffer *buffer);
void hidgl_flush_triangles (triangle_buffer *buffer);
void hidgl_ensure_triangle_space (triangle_buffer *buffer, int count);

static inline void
hidgl_add_triangle (triangle_buffer *buffer,
                    GLfloat x1, GLfloat y1,
                    GLfloat x2, GLfloat y2,
                    GLfloat x3, GLfloat y3)
{
  buffer->triangle_array [buffer->coord_comp_count++] = x1;
  buffer->triangle_array [buffer->coord_comp_count++] = y1;
  buffer->triangle_array [buffer->coord_comp_count++] = x2;
  buffer->triangle_array [buffer->coord_comp_count++] = y2;
  buffer->triangle_array [buffer->coord_comp_count++] = x3;
  buffer->triangle_array [buffer->coord_comp_count++] = y3;
  buffer->triangle_count++;
}

// void draw_grid ()
// int hidgl_set_layer (const char *name, int group, int empty)
// void hidgl_use_mask (int use_it)
// void hidgl_set_draw_xor (hidGC gc, int xor)
void hidgl_set_draw_faded (hidGC gc, int faded);
void hidgl_set_line_cap_angle (hidGC gc, int x1, int y1, int x2, int y2);
void hidgl_draw_line (hidGC gc, int cap, double width, int x1, int y1, int x2, int y2);
void hidgl_draw_arc (hidGC gc, double width, int vx, int vy, int vrx, int vry, int start_angle, int delta_angle, int flip_x, int flip_y);
void hidgl_draw_rect (hidGC gc, int x1, int y1, int x2, int y2);
void hidgl_fill_circle (hidGC gc, int vx, int vy, int vr);
void hidgl_fill_polygon (hidGC gc, int n_coords, int *x, int *y);
void hidgl_fill_rect (hidGC gc, int x1, int y1, int x2, int y2);


#endif /* __HIDGL_INCLUDED__  */
