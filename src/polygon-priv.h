#define ISECTED 3
#define UNKNWN  0
#define INSIDE  1
#define OUTSIDE 2
#define SHARED 3
#define SHARED2 4

#define TOUCHES 99

#define ISECT_BAD_PARAM (-1)
#define ISECT_NO_MEMORY (-2)

typedef struct seg
{
  BoxType box;
  VNODE *v;
  PLINE *p;
  int intersected;
} seg;


struct seg *lookup_seg (PLINE *contour, VNODE *vertex);

int vect_inters2 (Vector p1, Vector p2, Vector q1, Vector q2, Vector S1, Vector S2);
