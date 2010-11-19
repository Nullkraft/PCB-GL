/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "data.h"

/* The Linux OpenGL ABI 1.0 spec requires that we define
 * GL_GLEXT_PROTOTYPES before including gl.h or glx.h for extensions
 * in order to get prototypes:
 *   http://www.opengl.org/registry/ABI/
 */
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>

#include "hidgl_matrix.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

static void
load_texture_from_png (char *filename)
{
  GError *error = NULL;
  GdkPixbuf *pixbuf;
  int width;
  int height;
  int rowstride;
  int has_alpha;
  int bits_per_sample;
  int n_channels;
  unsigned char *pixels;

  pixbuf = gdk_pixbuf_new_from_file (filename, &error);

  if (pixbuf == NULL) {
    g_error ("%s", error->message);
    g_error_free (error);
    error = NULL;
    return;
  }

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
  bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  g_warn_if_fail (bits_per_sample == 8);
  g_warn_if_fail (n_channels == 4);
  g_warn_if_fail (rowstride == width * n_channels);

  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                (n_channels == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, pixels);

  g_object_unref (pixbuf);
}


static GLfloat *debug_lines = NULL;
static int no_debug_lines = 0;
static int max_debug_lines = 0;

//#define LENG 1000
#define LENG 0.6
#define STRIDE_FLOATS 6
static void
debug_basis_vector (float x,   float y,   float z,
                    float b1x, float b1y, float b1z,
                    float b2x, float b2y, float b2z,
                    float b3x, float b3y, float b3z)
{
  int comp_count;
  float lenb1, lenb2, lenb3;

  if (no_debug_lines + 3 > max_debug_lines) {
    max_debug_lines += 10;
    debug_lines = realloc (debug_lines, max_debug_lines * sizeof (GLfloat) * 2 * STRIDE_FLOATS);
  }

  lenb1 = sqrt (b1x * b1x + b1y * b1y + b1z * b1z);
  lenb2 = sqrt (b2x * b2x + b2y * b2y + b2z * b2z);
  lenb3 = sqrt (b3x * b3x + b3y * b3y + b3z * b3z);
  b1x /= lenb1;  b1y /= lenb1;  b1z /= lenb1;
  b2x /= lenb2;  b2y /= lenb2;  b2z /= lenb2;
  b3x /= lenb3;  b3y /= lenb3;  b3z /= lenb3;

  comp_count = 0;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = x;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = y;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = z;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 1.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = x + b1x * LENG;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = y + b1y * LENG;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = z + b1z * LENG;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 1.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  no_debug_lines++;

  comp_count = 0;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = x;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = y;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = z;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 1.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = x + b2x * LENG;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = y + b2y * LENG;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = z + b2z * LENG;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 1.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  no_debug_lines++;

  comp_count = 0;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = x;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = y;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = z;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 1.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = x + b3x * LENG;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = y + b3y * LENG;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = z + b3z * LENG;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 0.;
  debug_lines [no_debug_lines * 2 * STRIDE_FLOATS + comp_count++] = 1.;
  no_debug_lines++;
}

static void
debug_basis_display ()
{
  if (no_debug_lines == 0)
    return;

#if 1
  glPushAttrib (GL_CURRENT_BIT);
  glColor4f (1., 1., 1., 1.);
  glVertexPointer (3, GL_FLOAT, STRIDE_FLOATS * sizeof (GLfloat), &debug_lines[0]);
  glColorPointer (3, GL_FLOAT, STRIDE_FLOATS * sizeof (GLfloat), &debug_lines[3]);
  glEnableClientState (GL_VERTEX_ARRAY);
  glEnableClientState (GL_COLOR_ARRAY);
  glDrawArrays (GL_LINES, 0, no_debug_lines * 2);
  glDisableClientState (GL_COLOR_ARRAY);
  glDisableClientState (GL_VERTEX_ARRAY);
  glPopAttrib ();
#endif

  free (debug_lines);
  debug_lines = NULL;
  no_debug_lines = 0;
  max_debug_lines = 0;
}

/* b1{x,y,z} is the basis vector along "s" texture space */
/* b2{x,y,z} is the basis vector along "t" texture space */
static void
compute_light_vector (float b1x, float b1y, float b1z,
                      float b2x, float b2y, float b2z,
                      float *lx, float *ly, float *lz,
                      float *hx, float *hy, float *hz,
                      float x,   float y,   float z)
{
  float b3x, b3y, b3z;
  float tb1x, tb1y, tb1z;
  float tb2x, tb2y, tb2z;
  float tb3x, tb3y, tb3z;
  float mvm[16]; /* NB: TRANSPOSED IN MEMORY */
  /* NB: light_direction is a vector _TOWARDS_ the light source position */
//  float light_direction[] = {-0.5, 1., -1.}; /* XXX: HARDCODEED! */
  float light_direction[] = {0.0, 0.0, 1.0}; /* XXX: HARDCODEED! */
  float half_direction[3];
  float texspace_to_eye[4][4];
  float eye_to_texspace[4][4];
  float lenb1, lenb2, lenb3;
  float len_half;
  float len_light;

  /* Normalise the light vector */
  len_light = sqrt (light_direction[0] * light_direction[0] +
                    light_direction[1] * light_direction[1] +
                    light_direction[2] * light_direction[2]);
  light_direction[0] /= len_light;
  light_direction[1] /= len_light;
  light_direction[2] /= len_light;

  /* Sum with the unit vector towards the viewer */
  half_direction[0] = light_direction[0] + 0.;
  half_direction[1] = light_direction[1] + 0.;
  half_direction[2] = light_direction[2] + 1.;

  /* XXX: Should cache this ourselves */
  glGetFloatv (GL_MODELVIEW_MATRIX, (GLfloat *)mvm);

  if (0)
    debug_basis_vector ((mvm[0] * x + mvm[4] * y + mvm[ 8] * z + mvm[12]) / mvm[15],
                        (mvm[1] * x + mvm[5] * y + mvm[ 9] * z + mvm[13]) / mvm[15],
                        (mvm[2] * x + mvm[6] * y + mvm[10] * z + mvm[14]) / mvm[15],
                        0., 0., 1.,
                        light_direction[0], light_direction[1], light_direction[2],
                        half_direction[0], half_direction[1], half_direction[2]);

  /* Third basis vector is the cross product of tb1 and tb2 */
  b3x = (b2y * b1z - b2z * b1y);
  b3y = (b2z * b1x - b2x * b1z);
  b3z = (b2x * b1y - b2y * b1x);

  /* Transform the S, T texture space bases into eye coordinates */
  tb1x = mvm[0] * b1x + mvm[4] * b1y + mvm[ 8] * b1z;
  tb1y = mvm[1] * b1x + mvm[5] * b1y + mvm[ 9] * b1z;
  tb1z = mvm[2] * b1x + mvm[6] * b1y + mvm[10] * b1z;

  tb2x = mvm[0] * b2x + mvm[4] * b2y + mvm[ 8] * b2z;
  tb2y = mvm[1] * b2x + mvm[5] * b2y + mvm[ 9] * b2z;
  tb2z = mvm[2] * b2x + mvm[6] * b2y + mvm[10] * b2z;

  tb3x = mvm[0] * b3x + mvm[4] * b3y + mvm[ 8] * b3z;
  tb3y = mvm[1] * b3x + mvm[5] * b3y + mvm[ 9] * b3z;
  tb3z = mvm[2] * b3x + mvm[6] * b3y + mvm[10] * b3z;

#if 1
  /* Normalise tb1, tb2 and tb3 */
  lenb1 = sqrt (tb1x * tb1x + tb1y * tb1y + tb1z * tb1z);
  lenb2 = sqrt (tb2x * tb2x + tb2y * tb2y + tb2z * tb2z);
  lenb3 = sqrt (tb3x * tb3x + tb3y * tb3y + tb3z * tb3z);
  tb1x /= lenb1;  tb1y /= lenb1;  tb1z /= lenb1;
  tb2x /= lenb2;  tb2y /= lenb2;  tb2z /= lenb2;
  tb3x /= lenb3;  tb3y /= lenb3;  tb3z /= lenb3;
#endif

  if (0)
    debug_basis_vector ((mvm[0] * x + mvm[4] * y + mvm[ 8] * z + mvm[12]) / mvm[15],
                        (mvm[1] * x + mvm[5] * y + mvm[ 9] * z + mvm[13]) / mvm[15],
                        (mvm[2] * x + mvm[6] * y + mvm[10] * z + mvm[14]) / mvm[15],
                         tb1x, tb1y, tb1z, tb2x, tb2y, tb2z, tb3x, tb3y, tb3z);

  texspace_to_eye[0][0] = tb1x; texspace_to_eye[0][1] = tb2x; texspace_to_eye[0][2] = tb3x;  texspace_to_eye[0][3] = 0.0;
  texspace_to_eye[1][0] = tb1y; texspace_to_eye[1][1] = tb2y; texspace_to_eye[1][2] = tb3y;  texspace_to_eye[1][3] = 0.0;
  texspace_to_eye[2][0] = tb1z; texspace_to_eye[2][1] = tb2z; texspace_to_eye[2][2] = tb3z;  texspace_to_eye[2][3] = 0.0;
  texspace_to_eye[3][0] = 0.0;  texspace_to_eye[3][1] = 0.0;  texspace_to_eye[3][2] = 0.0;   texspace_to_eye[3][3] = 1.0;

  invert_4x4 (texspace_to_eye, eye_to_texspace);

  /* light_direction is in eye space, we need to transform this into texture space */
  *lx = eye_to_texspace[0][0] * light_direction[0] +
        eye_to_texspace[0][1] * light_direction[1] +
        eye_to_texspace[0][2] * light_direction[2];
  *ly = eye_to_texspace[1][0] * light_direction[0] +
        eye_to_texspace[1][1] * light_direction[1] +
        eye_to_texspace[1][2] * light_direction[2];
  *lz = eye_to_texspace[2][0] * light_direction[0] +
        eye_to_texspace[2][1] * light_direction[1] +
        eye_to_texspace[2][2] * light_direction[2];

  /* half_direction is in eye space, we need to transform this into texture space */
  *hx = eye_to_texspace[0][0] * half_direction[0] +
        eye_to_texspace[0][1] * half_direction[1] +
        eye_to_texspace[0][2] * half_direction[2];
  *hy = eye_to_texspace[1][0] * half_direction[0] +
        eye_to_texspace[1][1] * half_direction[1] +
        eye_to_texspace[1][2] * half_direction[2];
  *hz = eye_to_texspace[2][0] * half_direction[0] +
        eye_to_texspace[2][1] * half_direction[1] +
        eye_to_texspace[2][2] * half_direction[2];

  {
    len_light = sqrt (*lx * *lx + *ly * *ly + *lz * *lz);
    *lx /= len_light;
    *ly /= len_light;
    *lz /= len_light;

    *lx = *lx / 2. + 0.5;
    *ly = *ly / 2. + 0.5;
    *lz = *lz / 2. + 0.5;

    len_half = sqrt (*hx * *hx + *hy * *hy + *hz * *hz);
    *hx /= len_half;
    *hy /= len_half;
    *hz /= len_half;

    *hx = *hx / 2. + 0.5;
    *hy = *hy / 2. + 0.5;
    *hz = *hz / 2. + 0.5;
  }
}

static void
emit_vertex (float x,   float y,   float z,
             float b1x, float b1y, float b1z,
             float b2x, float b2y, float b2z,
             float tex0_s, float tex1_s, float tex1_t)
{
  GLfloat lx, ly, lz;
  GLfloat hx, hy, hz;
  compute_light_vector (b1x, b1y, b1z, b2x, b2y, b2z, &lx, &ly, &lz, &hx, &hy, &hz, x, y, z);
  glColor3f (lx, ly, lz);
  glMultiTexCoord1f (GL_TEXTURE0, tex0_s);
  glMultiTexCoord2f (GL_TEXTURE1, tex1_s, tex1_t);
  glMultiTexCoord3f (GL_TEXTURE2, hx, hy, hz);
  glVertex3f (x, y, z);
}

enum geom_pos {
  FIRST,
  MIDDLE,
  LAST
};

static void
emit_pair (float ang_edge1, float cos_edge1, float sin_edge1,
           float ang_edge2, float cos_edge2, float sin_edge2,
           float prev_r, float prev_z,
           float      r, float      z,
           float next_r, float next_z,
           float tex0_s, float resistor_width,
           enum geom_pos pos)
{
  int repeat;

  tex0_s = z / resistor_width + 0.5;

  for (repeat = 0; repeat < ((pos == FIRST) ? 2 : 1); repeat++)
    emit_vertex (r * cos_edge1, r * sin_edge1, z,
                 sin_edge1, -cos_edge1, 0,
                 cos_edge1 * (next_r - prev_r) / 2., sin_edge1 * (next_r - prev_r) / 2., (next_z - prev_z) / 2.,
                 tex0_s, ang_edge1 / 2. / M_PI, tex0_s);

  for (repeat = 0; repeat < ((pos == LAST) ? 2 : 1); repeat++)
    emit_vertex (r * cos_edge2, r * sin_edge2, z,
                 sin_edge2, -cos_edge2, 0,
                 cos_edge2 * (next_r - prev_r) / 2., sin_edge2 * (next_r - prev_r) / 2., (next_z - prev_z) / 2.,
                 tex0_s, ang_edge2 / 2. / M_PI, tex0_s);
}


#define NUM_RESISTOR_STRIPS 100
#define NUM_PIN_RINGS 15
#define MIL_TO_INTERNAL 100.

extern int hidgl_parse_vrml (char *filename);

void
hidgl_draw_vrml (ElementType *element, float surface_depth, float board_thickness, char *vrml_file)
{
  static bool one_shot = true;

  if (!one_shot)
    return;

  printf ("hidgl_draw_vrml\n");
  hidgl_parse_vrml ("test.wrl");
  one_shot = false;
}
