#include "quad.h"

#include <stdio.h>
#include <stdlib.h>


#ifndef WIN32
/* The Linux OpenGL ABI 1.0 spec requires that we define
 * GL_GLEXT_PROTOTYPES before including gl.h or glx.h for extensions
 * in order to get prototypes:
 *   http://www.opengl.org/registry/ABI/
 */
#   define GL_GLEXT_PROTOTYPES 1
#endif

#ifdef HAVE_OPENGL_GL_H
#   include <OpenGL/gl.h>
#else
#   include <GL/gl.h>
#endif


static void
print_edge_pointer_and_id (edge_ref e)
{
  //printf ("0x%jX, ID %i.%i", (uintmax_t)e, ID(e), e & 3u);
  printf ("ID %i.%i", ID(e), (unsigned int)e & 3u);
}

static void
debug_print_edge (edge_ref e, void *data)
{
  printf ("Edge ID %i.%i\n", ID(e), (int)e & 3u);
//  printf ("Edge has pointer "); print_edge_pointer_and_id (e)       ; printf ("\n");
  printf ("Edge ONEXT is "); print_edge_pointer_and_id (ONEXT(e)); printf ("\n");
  printf ("Edge OPREV is "); print_edge_pointer_and_id (OPREV(e)); printf ("\n");
  printf ("Edge DNEXT is "); print_edge_pointer_and_id (DNEXT(e)); printf ("\n");
  printf ("Edge DPREV is "); print_edge_pointer_and_id (DPREV(e)); printf ("\n");
  printf ("Edge RNEXT is "); print_edge_pointer_and_id (RNEXT(e)); printf ("\n");
  printf ("Edge RPREV is "); print_edge_pointer_and_id (RPREV(e)); printf ("\n");
  printf ("Edge LNEXT is "); print_edge_pointer_and_id (LNEXT(e)); printf ("\n");
  printf ("Edge LPREV is "); print_edge_pointer_and_id (LPREV(e)); printf ("\n");

#if 0
  printf ("RAW POINTERS\n");
  printf ("data[0] is "); print_edge_pointer_and_id (ONEXT   (e)); printf ("\n");
  printf ("data[1] is "); print_edge_pointer_and_id (ROTRNEXT(e)); printf ("\n");
  printf ("data[2] is "); print_edge_pointer_and_id (SYMDNEXT(e)); printf ("\n");
  printf ("data[3] is "); print_edge_pointer_and_id (TORLNEXT(e)); printf ("\n");
#endif
}

static int global_vertex_count;

typedef struct
{
  float x;
  float y;
  float z;
  int id;
} vertex;

static vertex *
make_vertex (float x, float y, float z)
{
  vertex *v;

  v = malloc (sizeof(vertex));
  v->x = x;
  v->y = y;
  v->z = z;
  v->id = global_vertex_count++;

  return v;
}

#if 0
static void
destroy_vertex (vertex *v)
{
  free (v);
}
#endif

edge_ref quad_test_cube = 0;

void
quad_test_init (void)
{
  vertex *cube_vertices[8];
  edge_ref cube_edges[12];
  //int i;

  cube_edges[0] = make_edge ();
  cube_edges[1] = make_edge ();
  cube_edges[2] = make_edge ();
  cube_edges[3] = make_edge ();
  cube_edges[4] = make_edge ();
  cube_edges[5] = make_edge ();
  cube_edges[6] = make_edge ();
  cube_edges[7] = make_edge ();
  cube_edges[8] = make_edge ();
  cube_edges[9] = make_edge ();
  cube_edges[10] = make_edge ();
  cube_edges[11] = make_edge ();

  cube_vertices[0] = make_vertex (0., 0., 0.);
  cube_vertices[1] = make_vertex (1., 0., 0.);
  cube_vertices[2] = make_vertex (1., 0., -1.);
  cube_vertices[3] = make_vertex (0., 0., -1.);
  cube_vertices[4] = make_vertex (0., 1., 0.);
  cube_vertices[5] = make_vertex (1., 1., 0.);
  cube_vertices[6] = make_vertex (1., 1., -1.);
  cube_vertices[7] = make_vertex (0., 1., -1.);

  ODATA (cube_edges[0]) = cube_vertices[0];
  DDATA (cube_edges[0]) = cube_vertices[1];
  ODATA (cube_edges[1]) = cube_vertices[1];
  DDATA (cube_edges[1]) = cube_vertices[2];
  ODATA (cube_edges[2]) = cube_vertices[2];
  DDATA (cube_edges[2]) = cube_vertices[3];
  ODATA (cube_edges[3]) = cube_vertices[3];
  DDATA (cube_edges[3]) = cube_vertices[0];

  ODATA (cube_edges[4]) = cube_vertices[4];
  DDATA (cube_edges[4]) = cube_vertices[5];
  ODATA (cube_edges[5]) = cube_vertices[5];
  DDATA (cube_edges[5]) = cube_vertices[6];
  ODATA (cube_edges[6]) = cube_vertices[6];
  DDATA (cube_edges[6]) = cube_vertices[7];
  ODATA (cube_edges[7]) = cube_vertices[7];
  DDATA (cube_edges[7]) = cube_vertices[4];

  ODATA (cube_edges[ 8]) = cube_vertices[0];
  DDATA (cube_edges[ 8]) = cube_vertices[4];
  ODATA (cube_edges[ 9]) = cube_vertices[1];
  DDATA (cube_edges[ 9]) = cube_vertices[5];
  ODATA (cube_edges[10]) = cube_vertices[2];
  DDATA (cube_edges[10]) = cube_vertices[6];
  ODATA (cube_edges[11]) = cube_vertices[3];
  DDATA (cube_edges[11]) = cube_vertices[7];

  printf ("Hello world\n");

  /* Edges around vertex 0 */
  splice (cube_edges[0], cube_edges[8]);
  splice (cube_edges[8], SYM(cube_edges[3]));

  /* Edges around vertex 1 */
  splice (cube_edges[1], cube_edges[9]);
  splice (cube_edges[9], SYM(cube_edges[0]));

  /* Edges around vertex 2 */
  splice (cube_edges[2], cube_edges[10]);
  splice (cube_edges[10], SYM(cube_edges[1]));

  /* Edges around vertex 3 */
  splice (cube_edges[3], cube_edges[11]);
  splice (cube_edges[11], SYM(cube_edges[2]));

  /* Edges around vertex 4 */
  splice (cube_edges[4], SYM(cube_edges[7]));
  splice (SYM(cube_edges[7]), SYM(cube_edges[8]));

  /* Edges around vertex 5 */
  splice (cube_edges[5], SYM(cube_edges[4]));
  splice (SYM(cube_edges[4]), SYM(cube_edges[9]));

  /* Edges around vertex 6 */
  splice (cube_edges[6], SYM(cube_edges[5]));
  splice (SYM(cube_edges[5]), SYM(cube_edges[10]));

  /* Edges around vertex 7 */
  splice (cube_edges[7], SYM(cube_edges[5]));
  splice (SYM(cube_edges[6]), SYM(cube_edges[11]));

#if 0
  printf ("cube_edges[0] :\n");
  debug_print_edge (cube_edges[0], NULL);
  printf ("\n");
  printf ("SYM(cube_edges[0]) :\n");
  debug_print_edge (SYM(cube_edges[0]), NULL);
#endif

  printf ("\n");
  printf ("Walking from cube_edges[0]\n");
  quad_enum (cube_edges[0], debug_print_edge, NULL);

  printf ("\n");
  printf ("Walking from ROT(cube_edges[0])\n");
  quad_enum (ROT(cube_edges[0]), debug_print_edge, NULL);

#if 0
  for (i = 0; i < 8; i++)
    destroy_vertex (cube_vertices[i]);
#endif

  quad_test_cube = cube_edges[0];
}

#define XOFFSET 50000000
#define YOFFSET 50000000
#define ZOFFSET 0
#define SCALE  10000000

float colors[12][3] = {{1., 0., 0.},
                       {1., 1., 0.},
                       {0., 1., 0.},
                       {0., 1., 1.},
                       {0.5, 0., 0.},
                       {0.5, 0.5, 0.},
                       {0., 0.5, 0.},
                       {0., 0.5, 0.5},
                       {1., 0.5, 0.5},
                       {1., 1., 0.5},
                       {0.5, 1., 0.5},
                       {0.5, 1., 1.}};

static void
draw_quad_edge (edge_ref e, void *data)
{
  int id = ID(e) % 12;

  glColor3f (colors[id][0], colors[id][1], colors[id][2]);
  glBegin (GL_LINES);
  glVertex3i (XOFFSET + SCALE * ((vertex *)ODATA(e))->x,
              YOFFSET - SCALE * ((vertex *)ODATA(e))->y,
              ZOFFSET + SCALE * ((vertex *)ODATA(e))->z);
  glVertex3i (XOFFSET + SCALE * ((vertex *)DDATA(e))->x,
              YOFFSET - SCALE * ((vertex *)DDATA(e))->y,
              ZOFFSET + SCALE * ((vertex *)DDATA(e))->z);
  glEnd ();
}

void
draw_quad_debug (void)
{

  quad_enum (quad_test_cube, draw_quad_edge, NULL);
}
