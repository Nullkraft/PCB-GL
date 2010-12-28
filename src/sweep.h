#include "cairo/cairo-traps-private.h"

cairo_status_t bo_poly_to_traps (POLYAREA *poly, cairo_traps_t *traps);
cairo_status_t bo_contour_to_traps (PLINE *contour, cairo_traps_t *traps);
cairo_status_t bo_contour_to_traps_no_draw (PLINE *contour, cairo_traps_t *traps);
cairo_fixed_t _line_compute_intersection_x_for_y (const cairo_line_t *line, cairo_fixed_t y);
