/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 2009 PCB Contributors (See ChangeLog for details).
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

typedef struct {
  GLfloat x;
  GLfloat y;
  GLfloat z;
  GLfloat r;
  GLfloat g;
  GLfloat b;
  GLfloat a;
} tri_array_element;

#define TRIANGLE_ARRAY_SIZE 5461
#define TRIANGLE_ARRAY_BYTES (3 * sizeof (tri_array_element) * TRIANGLE_ARRAY_SIZE)

typedef struct {
//  GLfloat triangle_array [3 * 3 * TRIANGLE_ARRAY_SIZE];
//  GLfloat *triangle_array;
  tri_array_element *triangle_array;
  unsigned int triangle_count;
//  unsigned int coord_comp_count;
  unsigned int array_size;
  GLuint vbo_name;
} triangle_buffer;

extern triangle_buffer buffer;
extern float global_depth;

void hidgl_init_triangle_array (triangle_buffer *buffer);
void hidgl_flush_triangles (triangle_buffer *buffer);
void hidgl_ensure_triangle_space (triangle_buffer *buffer, int count);
void hidgl_color (GLfloat r, GLfloat g, GLfloat b, GLfloat a);

static inline void
hidgl_add_triangle_3D (triangle_buffer *buffer,
                       GLfloat x1, GLfloat y1, GLfloat z1,
                       GLfloat x2, GLfloat y2, GLfloat z2,
                       GLfloat x3, GLfloat y3, GLfloat z3,
                       GLfloat r, GLfloat g,
                       GLfloat b, GLfloat a)
{
  int i = 0;

  buffer->triangle_array[buffer->triangle_count * 3 + i].x = x1;
  buffer->triangle_array[buffer->triangle_count * 3 + i].y = y1;
  buffer->triangle_array[buffer->triangle_count * 3 + i].z = z1;
  buffer->triangle_array[buffer->triangle_count * 3 + i].r = r;
  buffer->triangle_array[buffer->triangle_count * 3 + i].g = g;
  buffer->triangle_array[buffer->triangle_count * 3 + i].b = b;
  buffer->triangle_array[buffer->triangle_count * 3 + i].a = a;
  i++;
  buffer->triangle_array[buffer->triangle_count * 3 + i].x = x2;
  buffer->triangle_array[buffer->triangle_count * 3 + i].y = y2;
  buffer->triangle_array[buffer->triangle_count * 3 + i].z = z2;
  buffer->triangle_array[buffer->triangle_count * 3 + i].r = r;
  buffer->triangle_array[buffer->triangle_count * 3 + i].g = g;
  buffer->triangle_array[buffer->triangle_count * 3 + i].b = b;
  buffer->triangle_array[buffer->triangle_count * 3 + i].a = a;
  i++;
  buffer->triangle_array[buffer->triangle_count * 3 + i].x = x3;
  buffer->triangle_array[buffer->triangle_count * 3 + i].y = y3;
  buffer->triangle_array[buffer->triangle_count * 3 + i].z = z3;
  buffer->triangle_array[buffer->triangle_count * 3 + i].r = r;
  buffer->triangle_array[buffer->triangle_count * 3 + i].g = g;
  buffer->triangle_array[buffer->triangle_count * 3 + i].b = b;
  buffer->triangle_array[buffer->triangle_count * 3 + i].a = a;

  buffer->triangle_count++;
}

static inline void
hidgl_add_triangle (triangle_buffer *buffer,
                    GLfloat x1, GLfloat y1,
                    GLfloat x2, GLfloat y2,
                    GLfloat x3, GLfloat y3,
                    GLfloat r, GLfloat g,
                    GLfloat b, GLfloat a)
{
  hidgl_add_triangle_3D (buffer, x1, y1, global_depth,
                                 x2, y2, global_depth,
                                 x3, y3, global_depth,
                                 r, g, b, a);
}

// void draw_grid ()
void hidgl_draw_line (int cap, double width, int x1, int y1, int x2, int y2, double scale);
void hidgl_draw_arc (double width, int vx, int vy, int vrx, int vry, int start_angle, int delta_angle, double scale);
void hidgl_draw_rect (int x1, int y1, int x2, int y2);
void hidgl_fill_circle (int vx, int vy, int vr, double scale);
void hidgl_fill_polygon (int n_coords, int *x, int *y);
void hidgl_fill_pcb_polygon (PolygonType *poly, const BoxType *clip_box, double scale);
void hidgl_fill_rect (int x1, int y1, int x2, int y2);

void hidgl_init (void);
int hidgl_stencil_bits (void);
int hidgl_assign_clear_stencil_bit (void);
void hidgl_return_stencil_bit (int bit);
void hidgl_reset_stencil_usage (void);
void hidgl_set_depth (float depth);

#endif /* __HIDGL_INCLUDED__  */
