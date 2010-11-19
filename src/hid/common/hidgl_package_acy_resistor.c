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

#include "hidgl.h"
#include "hidgl_material.h"
#include "hidgl_geometry.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

/* DUMMY PROTOTYPES AND TYPES */
typedef struct _hidgl_shape hidgl_shape;
typedef struct _hidgl_transform hidgl_transform;

hidgl_shape *hidgl_shape_new (hidgl_geometry *geometry, hidgl_material *material);
hidgl_geometry *hidgl_tristrip_geometry_new ();
hidgl_transform *hidgl_transform_new ();
void hidgl_transform_rotate (hidgl_transform *transform, float angle, float x, float y, float z);
void hidgl_transform_add_child (hidgl_transform *transform, hidgl_shape *shape);

/* Static data we initialise for the model */
typedef struct {
  hidgl_transform *root;
  hidgl_material *resistor_body_mat;
  hidgl_material *resistor_pins_mat;
  hidgl_material *selected_pins_mat;
  hidgl_geometry *resistor_body_geom;
  hidgl_geometry *resistor_pins_geom;
  GLuint resistor_body_bump_texture;
  GLuint zero_ohm_body_bump_texture;
} model_data;

static model_data model = {
    .root = NULL,
    .resistor_body_mat = NULL,
    .resistor_pins_mat = NULL,
    .selected_pins_mat = NULL,
    .resistor_body_geom = NULL,
    .resistor_pins_geom = NULL,
    .resistor_body_bump_texture = 0,
    .zero_ohm_body_bump_texture = 0
};

static int
compute_offset (int x, int y, int width, int height)
{
  x = (x + width) % width;
  y = (y + height) % height;
  return (y * width + x) * 4;
}

static void
load_texture_from_png (char *filename, bool bumpmap)
{
  GError *error = NULL;
  GdkPixbuf *pixbuf;
  int width;
  int height;
  int rowstride;
  /* int has_alpha; */
  int bits_per_sample;
  int n_channels;
  unsigned char *pixels;
  unsigned char *new_pixels;
  int x, y;

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
  /* has_alpha = gdk_pixbuf_get_has_alpha (pixbuf); */
  bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  g_warn_if_fail (bits_per_sample == 8);
  g_warn_if_fail (n_channels == 4);
  g_warn_if_fail (rowstride == width * n_channels);

  new_pixels = malloc (width * height * 4);

  /* XXX: Move computing bump map out of the texture loading function */
  if (bumpmap) {
    for (y = 0; y < height; y++)
      for (x = 0; x < width; x++) {
        float r, g, b;
        float lenvec;

        float dzdx = (pixels[compute_offset (x + 1, y, width, height)] - pixels[compute_offset (x - 1, y, width, height)]) / 255.;
        float dzdy = (pixels[compute_offset (x, y + 1, width, height)] - pixels[compute_offset (x, y - 1, width, height)]) / 255.;

#if 0
        dx *= 10.;
        dy *= 10.;
#endif

        /* Basis vectors are (x -> x+1) in X direction , z(x,y), z(x,x)+dzdx in Z direction */
        /*                   (y -> y+1) in Y direction , z(x,y), z(x,y)+dzdy in Z direction */

        /* Normal is cross product of the two basis vectors */
        // | i  j  k    |
        // | 1  0  dzdx |
        // | 0  1  dzdy |

        r = -dzdx;
        g = -dzdy;
        b = 1.;

        lenvec = sqrt (r * r + g * g + b * b);

        r /= lenvec;
        g /= lenvec;
        b /= lenvec;

        new_pixels [compute_offset (x, y, width, height) + 0] =
          (unsigned char)((r / 2. + 0.5) * 255.);
        new_pixels [compute_offset (x, y, width, height) + 1] =
          (unsigned char)((g / 2. + 0.5) * 255.);
        new_pixels [compute_offset (x, y, width, height) + 2] =
          (unsigned char)((b / 2. + 0.5) * 255.);
        new_pixels [compute_offset (x, y, width, height) + 3] = 255;
      }

    memcpy (pixels, new_pixels, width * height * 4);
    gdk_pixbuf_save (pixbuf, "debug_bumps.png", "png", NULL, NULL);
  }

  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                (n_channels == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, pixels);

  free (new_pixels);
  g_object_unref (pixbuf);
}

static double
resistor_string_to_value (char *string)
{
  char *c = string;
  double accum_value = 0;
  bool before_decimal_point = true;
  double place_value = 1.;
  double multiplier = 1.;

  while (c != NULL && *c != '\0') {
    if (*c >= '0' && *c <= '9') {
      unsigned char digit_value;
      digit_value = *c - '0';

      if (before_decimal_point) {
        accum_value *= 10.;
        accum_value += digit_value;
      } else {
        place_value /= 10.;
        accum_value += digit_value * place_value;
      }
    } else if (before_decimal_point) {
      switch (*c) {
        case 'R':
        case 'r':
        case '.':
          multiplier = 1.;
          break;

        case 'k':
        case 'K':
          multiplier = 1000.;
          break;

        case 'M':
          multiplier = 1000000.;
          break;

        case 'm':
          multiplier = 1 / 1000.;
          break;

        default:
          /* XXX: Unknown multiplier */
          break;
      }
      before_decimal_point = false;

    } else {
      /* XXX: Unknown input */
    }
    c++;
  }

  accum_value *= multiplier;

  return accum_value;
}

static void
resistor_value_to_stripes (ElementType *element, int *st1, int *st2, int *st3, int *mul, int *tol)
{
  char *value_string = element->Name[VALUE_INDEX].TextString;
  double value;
  double order_of_magnitude;
  int no_stripes = 3; /* XXX: Hard-coded, but so are the function arguments, so whatever.. */

  /* XXX: Don't know tolerenace, so assume 1% */
  *tol = 2;

  if (value_string == NULL) {
    *st1 = 0;
    *st2 = 0;
    *st3 = 0;
    *mul = 0;
    return;
  }

  value = resistor_string_to_value (value_string);

  order_of_magnitude = log (value) / log (10);
  *mul = (int)order_of_magnitude - no_stripes + 3;
  if (*mul < 0 || *mul > 9) {
    /* Resistor multiplier out of range, e.g. zero ohm links */
    *mul = 2; /* PUT BLACK */
    *st1 = 0;
    *st2 = 0;
    *st3 = 0;
    return;
  }

  /* Color the first three significant digits */
  /* Normalise to 0-9 */
  value /= pow (10., (int)order_of_magnitude);
  *st1 = (int)value;
  value -= *st1;

  value *= 10.;
  *st2 = (int)value;
  value -= *st2;

  value *= 10.;
  *st3 = (int)value;
  value -= *st3;
}

static void
setup_zero_ohm_texture (GLfloat *res_body_color)
{
  GLfloat tex_data[32][3];
  int strip;

  for (strip = 0; strip < sizeof (tex_data) / sizeof (GLfloat[3]); strip++) {
    tex_data[strip][0] = res_body_color[0];
    tex_data[strip][1] = res_body_color[1];
    tex_data[strip][2] = res_body_color[2];
  }

  tex_data[15][0] = 0.;  tex_data[15][1] = 0.;  tex_data[15][2] = 0.;
  tex_data[16][0] = 0.;  tex_data[16][1] = 0.;  tex_data[16][2] = 0.;

  glTexImage1D (GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGB, GL_FLOAT, tex_data);
}

static bool
setup_resistor_texture (ElementType *element, GLfloat *res_body_color)
{
  GLfloat val_color[10][3] = {{0.00, 0.00, 0.00},
                              {0.52, 0.26, 0.00},
                              {1.00, 0.00, 0.00},
                              {1.00, 0.52, 0.00},
                              {1.00, 1.00, 0.00},
                              {0.00, 0.91, 0.00},
                              {0.00, 0.00, 1.00},
                              {0.68, 0.00, 0.68},
                              {0.65, 0.65, 0.65},
                              {1.00, 1.00, 1.00}};

  GLfloat mul_color[10][3] = {{0.65, 0.65, 0.65},  /* 0.01  */ /* XXX: Silver */
                              {0.72, 0.48, 0.09},  /* 0.1   */ /* XXX: Gold */
                              {0.00, 0.00, 0.00},  /* 1     */
                              {0.52, 0.26, 0.00},  /* 10    */
                              {1.00, 0.00, 0.00},  /* 100   */
                              {1.00, 0.52, 0.00},  /* 1k    */
                              {1.00, 1.00, 0.00},  /* 10k   */
                              {0.00, 0.91, 0.00},  /* 100k  */
                              {0.00, 0.00, 1.00},  /* 1M    */
                              {0.68, 0.00, 0.68}}; /* 10M   */

  GLfloat tol_color[10][3] = {{0.65, 0.65, 0.65},  /* 10%   */ /* XXX: Silver */
                              {0.72, 0.48, 0.09},  /* 5%    */ /* XXX: Gold */
                              {0.52, 0.26, 0.00},  /* 1%    */
                              {1.00, 0.00, 0.00},  /* 2%    */
                              {0.00, 0.91, 0.00},  /* 0.5%  */
                              {0.00, 0.00, 1.00},  /* 0.25% */
                              {0.68, 0.00, 0.68}}; /* 0.1%  */
  int st1 = 0;
  int st2 = 3;
  int st3 = 4;
  int mul = 2;
  int tol = 2;

  GLfloat tex_data[32][3];
  int strip;

  resistor_value_to_stripes (element, &st1, &st2, &st3, &mul, &tol);

  if (st1 == 0 && st2 == 0 && st3 == 0) {
    setup_zero_ohm_texture (res_body_color);
    return true;
  }

  for (strip = 0; strip < sizeof (tex_data) / sizeof (GLfloat[3]); strip++) {
    tex_data[strip][0] = res_body_color[0];
    tex_data[strip][1] = res_body_color[1];
    tex_data[strip][2] = res_body_color[2];
  }

  tex_data[ 7][0] = val_color[st1][0];  tex_data[ 7][1] = val_color[st1][1];  tex_data[ 7][2] = val_color[st1][2];

  tex_data[10][0] = val_color[st2][0];  tex_data[10][1] = val_color[st2][1];  tex_data[10][2] = val_color[st2][2];
  tex_data[11][0] = val_color[st2][0];  tex_data[11][1] = val_color[st2][1];  tex_data[11][2] = val_color[st2][2];

  tex_data[14][0] = val_color[st3][0];  tex_data[14][1] = val_color[st3][1];  tex_data[14][2] = val_color[st3][2];
  tex_data[15][0] = val_color[st3][0];  tex_data[15][1] = val_color[st3][1];  tex_data[15][2] = val_color[st3][2];

  tex_data[18][0] = mul_color[mul][0];  tex_data[18][1] = mul_color[mul][1];  tex_data[18][2] = mul_color[mul][2];
  tex_data[19][0] = mul_color[mul][0];  tex_data[19][1] = mul_color[mul][1];  tex_data[19][2] = mul_color[mul][2];

  tex_data[24][0] = tol_color[tol][0];  tex_data[24][1] = tol_color[tol][1];  tex_data[24][2] = tol_color[tol][2];

  glTexImage1D (GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGB, GL_FLOAT, tex_data);

  return false;
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

/* XXX: HARDCODED MAGIC */
#define BOARD_THICKNESS MM_TO_COORD (1.6)

static hidgl_geometry *
body_geometry (float resistor_pin_spacing)
{
  hidgl_geometry *geometry = hidgl_tristrip_geometry_new ();
  int strip;
  int no_strips = NUM_RESISTOR_STRIPS;

  /* XXX: Hard-coded magic */
  float resistor_pin_radius = MIL_TO_COORD (12.);
  float resistor_taper1_radius = MIL_TO_COORD (16.);
  float resistor_taper2_radius = MIL_TO_COORD (35.);
  float resistor_bulge_radius = MIL_TO_COORD (43.);
  float resistor_barrel_radius = MIL_TO_COORD (37.);

  float resistor_taper1_offset = MIL_TO_COORD (25.);
  float resistor_taper2_offset = MIL_TO_COORD (33.);
  float resistor_bulge_offset = MIL_TO_COORD (45.);
  float resistor_bulge_width = MIL_TO_COORD (50.);

  float resistor_pin_bend_radius = resistor_bulge_radius;
  float resistor_width = resistor_pin_spacing - 2. * resistor_pin_bend_radius;

  glBegin (GL_TRIANGLE_STRIP);

  for (strip = 0; strip < no_strips; strip++) {

    int ring;
    int no_rings;
    float angle_edge1 = strip * 2. * M_PI / no_strips;
    float angle_edge2 = (strip + 1) * 2. * M_PI / no_strips;

    float cos_edge1 = cosf (angle_edge1);
    float sin_edge1 = sinf (angle_edge1);
    float cos_edge2 = cosf (angle_edge2);
    float sin_edge2 = sinf (angle_edge2);

    struct strip_item {
      GLfloat z;
      GLfloat r;
      GLfloat tex0_s;
    } strip_data[] = {
      {-resistor_width / 2. - 1.,                                                     resistor_pin_radius,     0.}, /* DUMMY */
      {-resistor_width / 2.,                                                          resistor_pin_radius,     0.},
      {-resistor_width / 2. + resistor_taper1_offset,                                 resistor_taper1_radius,  0.},
      {-resistor_width / 2. + resistor_taper2_offset,                                 resistor_taper2_radius,  0.},
      {-resistor_width / 2. + resistor_bulge_offset                              ,   resistor_bulge_radius, 0.},
      {-resistor_width / 2. + resistor_bulge_offset + resistor_bulge_width * 3. / 4.,   resistor_bulge_radius, 0.},
      {-resistor_width / 2. + resistor_bulge_offset + resistor_bulge_width,           resistor_barrel_radius,  0.},
//      {-resistor_width / 2. + resistor_bulge_offset + resistor_bulge_width,           resistor_barrel_radius,  0.},
                                                                                      /*********************/  
      { 0,                                                                            resistor_barrel_radius,  0.5},
                                                                                      /*********************/
//      { resistor_width / 2. - resistor_bulge_offset - resistor_bulge_width,           resistor_barrel_radius,  1.},
      { resistor_width / 2. - resistor_bulge_offset - resistor_bulge_width,           resistor_barrel_radius,  1.},
      { resistor_width / 2. - resistor_bulge_offset - resistor_bulge_width * 3. / 4.,   resistor_bulge_radius, 1.},
      { resistor_width / 2. - resistor_bulge_offset,                                  resistor_bulge_radius, 1.},
      { resistor_width / 2. - resistor_taper2_offset,                                 resistor_taper2_radius,  1.},
      { resistor_width / 2. - resistor_taper1_offset,                                 resistor_taper1_radius,  1.},
      { resistor_width / 2.,                                                          resistor_pin_radius,     1.},
      { resistor_width / 2. + 1.,                                                     resistor_pin_radius,     1.}, /* DUMMY */
    };

    no_rings = sizeof (strip_data) / sizeof (struct strip_item);
    for (ring = 1; ring < no_rings - 1; ring++) {
      enum geom_pos pos = MIDDLE;
      if (ring == 1)            pos = FIRST;
      if (ring == no_rings - 2) pos = LAST;

      emit_pair (angle_edge1, cos_edge1, sin_edge1,
                 angle_edge2, cos_edge2, sin_edge2,
                 strip_data[ring - 1].r, strip_data[ring - 1].z,
                 strip_data[ring    ].r, strip_data[ring    ].z,
                 strip_data[ring + 1].r, strip_data[ring + 1].z,
                 strip_data[ring    ].tex0_s, resistor_width, pos);
    }
  }
  glEnd ();

  return geometry;
}

static hidgl_geometry *
pins_geometry (float resistor_pin_spacing, float board_thickness)
{
  hidgl_geometry *geometry = hidgl_tristrip_geometry_new ();
  int strip;
  int no_strips = NUM_RESISTOR_STRIPS;
  int ring;
  int no_rings = NUM_PIN_RINGS;
  int end;

  /* XXX: Hard-coded magic */
  float resistor_pin_radius = MIL_TO_COORD (12.);
  float pin_penetration_depth = MIL_TO_COORD (30.) + board_thickness;

  float resistor_bulge_radius = MIL_TO_COORD (43.);
  float resistor_pin_bend_radius = resistor_bulge_radius;
  float resistor_width = resistor_pin_spacing - 2. * resistor_pin_bend_radius;


  float r = resistor_pin_bend_radius;
  for (end = 0; end < 2; end++) {
    float end_sign = (end == 0) ? 1. : -1.;

    for (ring = 0; ring < no_rings; ring++) {

      float angle_ring_edge1 = ring * M_PI / 2. / no_rings + ((end == 0) ? 0. : -M_PI / 2.);
      float angle_ring_edge2 = (ring + 1) * M_PI / 2. / no_rings + ((end == 0) ? 0. : -M_PI / 2.);
      float y_strip_edge1 = cosf (angle_ring_edge1);
      float z_strip_edge1 = sinf (angle_ring_edge1);
      float y_strip_edge2 = cosf (angle_ring_edge2);
      float z_strip_edge2 = sinf (angle_ring_edge2);
      float r = resistor_pin_bend_radius;

      glBegin (GL_TRIANGLE_STRIP);

      /* NB: We wrap back around to complete the last segment, so in effect
       *     we draw no_strips + 1 strips.
       */
      for (strip = 0; strip < no_strips + 1; strip++) {
        float strip_angle = strip * 2. * M_PI / no_strips;

        float x1 = resistor_pin_radius * cos (strip_angle);
        float y1 = resistor_pin_radius * sin (strip_angle) * y_strip_edge1 + r * y_strip_edge1 - r;
        float z1 = resistor_pin_radius * sin (strip_angle) * z_strip_edge1 + r * z_strip_edge1 + resistor_width / 2. * end_sign;

        float x2 = resistor_pin_radius * cos (strip_angle);
        float y2 = resistor_pin_radius * sin (strip_angle) * y_strip_edge2 + r * y_strip_edge2 - r;
        float z2 = resistor_pin_radius * sin (strip_angle) * z_strip_edge2 + r * z_strip_edge2 + resistor_width / 2. * end_sign;

        glNormal3f (cos (strip_angle), sin (strip_angle) * y_strip_edge1, sin (strip_angle) * z_strip_edge1);
        glVertex3f (x1, y1, z1);
        glNormal3f (cos (strip_angle), sin (strip_angle) * y_strip_edge2, sin (strip_angle) * z_strip_edge2);
        glVertex3f (x2, y2, z2);
      }
      glEnd ();
    }

    glBegin (GL_TRIANGLE_STRIP);

    /* NB: We wrap back around to complete the last segment, so in effect
     *     we draw no_strips + 1 strips.
     */
    for (strip = 0; strip < no_strips + 1; strip++) {
      float strip_angle = strip * 2. * M_PI / no_strips;

      float x1 = resistor_pin_radius * cos (strip_angle);
      float y1 = -r;
      float z1 = resistor_pin_radius * sin (strip_angle) + (r + resistor_width / 2.) * end_sign;

      float x2 = resistor_pin_radius * cos (strip_angle);
      float y2 = -r - pin_penetration_depth;
      float z2 = resistor_pin_radius * sin (strip_angle) + (r + resistor_width / 2.) * end_sign;

      glNormal3f (cos (strip_angle), 0., sin (strip_angle));
      glVertex3f (x1, y1, z1);
      glNormal3f (cos (strip_angle), 0., sin (strip_angle));
      glVertex3f (x2, y2, z2);
    }
    glEnd ();

    glBegin (GL_TRIANGLE_FAN);

    glNormal3f (0, 0., -1.);
    glVertex3f (0, -r - pin_penetration_depth - resistor_pin_radius / 2., (r + resistor_width / 2.) * end_sign);

    /* NB: We wrap back around to complete the last segment, so in effect
     *     we draw no_strips + 1 strips.
     */
    for (strip = no_strips + 1; strip > 0; strip--) {
      float strip_angle = strip * 2. * M_PI / no_strips;

      float x = resistor_pin_radius * cos (strip_angle);
      float y = -r - pin_penetration_depth;
      float z = resistor_pin_radius * sin (strip_angle) + (r + resistor_width / 2.) * end_sign;

      glNormal3f (cos (strip_angle), 0., sin (strip_angle));
      glVertex3f (x, y, z);
    }
    glEnd ();
  }

  return geometry;
}

void
hidgl_init_acy_resistor (void)
{
  hidgl_geometry *body_geom;
  hidgl_geometry *pins_geom;
  hidgl_shape *body_shape;
  hidgl_shape *pins_shape;

  GLfloat emission[] = {0.0f, 0.0f, 0.0f, 1.0f};
  GLfloat specular[] = {0.5f, 0.5f, 0.5f, 1.0f};
  GLfloat pin_color[] = {0.55, 0.55, 0.55, 1.0};
  GLfloat selected_pin_color[] = {0.55, 0.55, 1.0, 1.0};

  /* Define the resistor body material */
  model.resistor_body_mat = hidgl_material_new ("resistor_body");
  hidgl_material_set_emission_color (model.resistor_body_mat, emission);
  hidgl_material_set_specular_color (model.resistor_body_mat, specular);
  hidgl_material_set_shininess (model.resistor_body_mat, 20.0f);
  hidgl_material_set_shader (model.resistor_body_mat, resistor_program);

  /* Define the resistor pins material */
  model.resistor_pins_mat = hidgl_material_new ("resistor_pins");
  hidgl_material_set_ambient_color (model.resistor_pins_mat, pin_color);
  hidgl_material_set_diffuse_color (model.resistor_pins_mat, pin_color);
  hidgl_material_set_emission_color (model.resistor_pins_mat, emission);
  hidgl_material_set_specular_color (model.resistor_pins_mat, specular);
  hidgl_material_set_shininess (model.resistor_pins_mat, 120.0f);

  /* Define the resistor selected pin material */
  model.selected_pins_mat = hidgl_material_new ("selected_pins");
  hidgl_material_set_ambient_color (model.selected_pins_mat, selected_pin_color);
  hidgl_material_set_diffuse_color (model.selected_pins_mat, selected_pin_color);
  hidgl_material_set_emission_color (model.selected_pins_mat, emission);
  hidgl_material_set_specular_color (model.selected_pins_mat, specular);
  hidgl_material_set_shininess (model.selected_pins_mat, 120.0f);

  /* Load bump mapping textures */
  glGenTextures (1, &model.resistor_body_bump_texture);
  glBindTexture (GL_TEXTURE_2D, model.resistor_body_bump_texture);
  load_texture_from_png ("resistor_bump.png", true);

  glGenTextures (1, &model.zero_ohm_body_bump_texture);
  glBindTexture (GL_TEXTURE_2D, model.zero_ohm_body_bump_texture);
  load_texture_from_png ("zero_ohm_bump.png", true);

  /* Setup geometry and transforms */
  model.root = hidgl_transform_new ();
  hidgl_transform_rotate (model.root, 90., 1., 0., 0.);

  body_geom = body_geometry (MIL_TO_COORD (400.));
  pins_geom = pins_geometry (MIL_TO_COORD (400.), BOARD_THICKNESS);

  body_shape = hidgl_shape_new (body_geom, model.resistor_body_mat);
  pins_shape = hidgl_shape_new (pins_geom, model.resistor_pins_mat);

  hidgl_transform_add_child (model.root, body_shape);
  hidgl_transform_add_child (model.root, pins_shape);
}

void
hidgl_draw_acy_resistor (ElementType *element, float surface_depth, float board_thickness)
{
  float center_x, center_y;
  float angle;
  GLfloat resistor_body_color[] = {0.31, 0.47, 0.64, 1.0};
  bool zero_ohm;
  static GLuint texture1;
  GLuint restore_sp;

  /* XXX: Hard-coded magic */
  float resistor_bulge_radius = MIL_TO_COORD (43.);
  float resistor_pin_bend_radius = resistor_bulge_radius;

  PinType *first_pin = element->Pin->data;
  PinType *second_pin = g_list_next (element->Pin)->data;

  Coord pin_delta_x = second_pin->X - first_pin->X;
  Coord pin_delta_y = second_pin->Y - first_pin->Y;

  center_x = first_pin->X + pin_delta_x / 2.;
  center_y = first_pin->Y + pin_delta_y / 2.;
  angle = atan2f (pin_delta_y, pin_delta_x);

  /* TRANSFORM MATRIX */
  glPushMatrix ();
  glTranslatef (center_x, center_y, surface_depth + resistor_pin_bend_radius);
  glRotatef (angle * 180. / M_PI + 90, 0., 0., 1.);
  glRotatef (90, 1., 0., 0.);

  /* TEXTURE SETUP */
  glGetIntegerv (GL_CURRENT_PROGRAM, (GLint*)&restore_sp);
  hidgl_shader_activate (resistor_program);

  {
    GLuint program = hidgl_shader_get_program (resistor_program);
    int tex0_location = glGetUniformLocation (program, "detail_tex");
    int tex1_location = glGetUniformLocation (program, "bump_tex");
    glUniform1i (tex0_location, 0);
    glUniform1i (tex1_location, 1);
  }

  glActiveTextureARB (GL_TEXTURE0_ARB);
  glGenTextures (1, &texture1);
  glBindTexture (GL_TEXTURE_1D, texture1);
  zero_ohm = setup_resistor_texture (element, resistor_body_color);

  glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameterf (GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf (GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameterf (GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glEnable (GL_TEXTURE_1D);

  glActiveTextureARB (GL_TEXTURE1_ARB);
  if (zero_ohm)
    glBindTexture (GL_TEXTURE_2D, model.zero_ohm_body_bump_texture);
  else
    glBindTexture (GL_TEXTURE_2D, model.resistor_body_bump_texture);

  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glEnable (GL_TEXTURE_2D);
  glActiveTextureARB (GL_TEXTURE0_ARB);

  glPushAttrib (GL_CURRENT_BIT);

  glDisable (GL_LIGHTING);

  hidgl_material_activate (model.resistor_body_mat);

  /* Draw body */

  glActiveTextureARB (GL_TEXTURE1_ARB);
  glDisable (GL_TEXTURE_2D);
  glBindTexture (GL_TEXTURE_2D, 0);

  glActiveTextureARB (GL_TEXTURE0_ARB);
  glDisable (GL_TEXTURE_1D);
  glBindTexture (GL_TEXTURE_1D, 0);
  glDeleteTextures (1, &texture1);

  glEnable (GL_LIGHTING);

  hidgl_material_activate (model.resistor_pins_mat);

  /* Draw pins */

  glDisable (GL_COLOR_MATERIAL);
  glPopAttrib ();

  glDisable (GL_LIGHTING);

  glPopMatrix ();
  glUseProgram (restore_sp);
}
