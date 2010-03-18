/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

/*
 * These definitions are solely for use by the implementation of cairo
 * and constitute no kind of standard.  If you need any of these
 * functions, please drop me a note.  Either the library needs new
 * functionality, or there's a way to do what you need using the
 * existing published interfaces. cworth@cworth.org
 */

#ifndef _CAIRO_TRAPS_PRIVATE_H_
#define _CAIRO_TRAPS_PRIVATE_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "cairo-types-private.h"

typedef struct _cairo_traps {
    cairo_status_t status;

    const cairo_box_t *limits;
    int num_limits;

    unsigned int maybe_region : 1; /* hint: 0 implies that it cannot be */
    unsigned int has_intersections : 1;
    unsigned int is_rectilinear : 1;
    unsigned int is_rectangular : 1;

    int num_traps;
    int traps_size;
    cairo_trapezoid_t *traps;
    cairo_trapezoid_t  traps_embedded[16];
} cairo_traps_t;


/* cairo-traps.c */
cairo_private void
_cairo_traps_init (cairo_traps_t *traps);

cairo_private void
_cairo_traps_limit (cairo_traps_t	*traps,
		    const cairo_box_t	*boxes,
		    int			 num_boxes);

cairo_private cairo_status_t
_cairo_traps_init_boxes (cairo_traps_t	    *traps,
			 const cairo_box_t    *boxes,
			 int		     num_boxes);

cairo_private void
_cairo_traps_clear (cairo_traps_t *traps);

cairo_private void
_cairo_traps_fini (cairo_traps_t *traps);

#define _cairo_traps_status(T) (T)->status

cairo_private void
_cairo_traps_translate (cairo_traps_t *traps, int x, int y);

cairo_private cairo_status_t
_cairo_traps_tessellate_rectangle (cairo_traps_t *traps,
				   const cairo_point_t *top_left,
				   const cairo_point_t *bottom_right);

cairo_private void
_cairo_traps_add_trap (cairo_traps_t *traps,
		       cairo_fixed_t top, cairo_fixed_t bottom,
		       cairo_line_t *left, cairo_line_t *right);

cairo_private cairo_status_t
_cairo_bentley_ottmann_tessellate_rectilinear_polygon (cairo_traps_t	 *traps,
						       const cairo_polygon_t *polygon,
						       cairo_fill_rule_t	  fill_rule);

cairo_private cairo_status_t
_cairo_bentley_ottmann_tessellate_polygon (cairo_traps_t         *traps,
					   const cairo_polygon_t *polygon);

cairo_private cairo_status_t
_cairo_bentley_ottmann_tessellate_traps (cairo_traps_t *traps,
					 cairo_fill_rule_t fill_rule);

cairo_private cairo_status_t
_cairo_bentley_ottmann_tessellate_rectangular_traps (cairo_traps_t *traps,
						     cairo_fill_rule_t fill_rule);

cairo_private cairo_status_t
_cairo_bentley_ottmann_tessellate_rectilinear_traps (cairo_traps_t *traps,
						     cairo_fill_rule_t fill_rule);

cairo_private int
_cairo_traps_contain (const cairo_traps_t *traps,
		      double x, double y);

cairo_private void
_cairo_traps_extents (const cairo_traps_t *traps,
		      cairo_box_t         *extents);

cairo_private cairo_int_status_t
_cairo_traps_extract_region (cairo_traps_t  *traps,
			     cairo_region_t **region);

cairo_private cairo_status_t
_cairo_traps_path (const cairo_traps_t *traps,
		   cairo_path_fixed_t  *path);

cairo_private void
_cairo_trapezoid_array_translate_and_scale (cairo_trapezoid_t *offset_traps,
					    cairo_trapezoid_t *src_traps,
					    int num_traps,
					    double tx, double ty,
					    double sx, double sy);

#endif
