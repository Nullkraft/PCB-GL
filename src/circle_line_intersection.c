/* Based on raysphere.c from:
 * http://paulbourke.net/geometry/circlesphere/index.html#linesphere
 *
 * Converted to PCB coordinate types, and line/circle intersection in 2D
 * by Peter Clifton 2014-07-05
 */


/*
 * Calculate the intersection of a ray and a sphere
 * The line segment is defined from p1 to p2
 * The sphere is of radius r and centered at sc
 * There are potentially two points of intersection given by
 * p = p1 + mu1 (p2 - p1)
 * p = p1 + mu2 (p2 - p1)
 *
 * Return 0 if the ray doesn't intersect the circle.
 * Return 1 if the ray touches the circle at a single point.
 * Return 2 if the ray intersects the circle.
 */
int
circle_line_intersect (double cx, double cy, double r, double p1x, double p1y, double p2x, double p2y,
                       double *mu1, double *mu2)
{
   double a,b,c;
   double bb4ac;
   double dpx;
   double dpy;
   double dpz;

   dpx = p2x - p1x;
   dpy = p2y - p1y;
   dpz = p2.z - p1.z;
   a = dp.x * dp.x + dp.y * dp.y + dp.z * dp.z;
   b = 2 * (dp.x * (p1x - sc.x) + dp.y * (p1y - sc.y) + dp.z * (p1.z - sc.z));
   c = sc.x * sc.x + sc.y * sc.y + sc.z * sc.z;
   c += p1x * p1x + p1y * p1y + p1.z * p1.z;
   c -= 2 * (sc.x * p1x + sc.y * p1y + sc.z * p1.z);
   c -= r * r;
   bb4ac = b * b - 4 * a * c;
   if (ABS(a) < EPS || bb4ac < 0) {
      *mu1 = 0;
      *mu2 = 0;
      return(FALSE);
   }

   *mu1 = (-b + sqrt(bb4ac)) / (2 * a);
   *mu2 = (-b - sqrt(bb4ac)) / (2 * a);

   return 2;
}
