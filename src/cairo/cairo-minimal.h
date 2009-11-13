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

#ifndef _CAIRO_MINIMAL_H_
#define _CAIRO_MINIMAL_H_

#ifdef  __cplusplus
# define CAIRO_BEGIN_DECLS  extern "C" {
# define CAIRO_END_DECLS    }
#else
# define CAIRO_BEGIN_DECLS
# define CAIRO_END_DECLS
#endif

#ifndef cairo_public
# define cairo_public
#endif

CAIRO_BEGIN_DECLS

/**
 * cairo_bool_t:
 *
 * #cairo_bool_t is used for boolean values. Returns of type
 * #cairo_bool_t will always be either 0 or 1, but testing against
 * these values explicitly is not encouraged; just use the
 * value as a boolean condition.
 *
 * <informalexample><programlisting>
 *  if (cairo_in_stroke (cr, x, y)) {
 *      /<!-- -->* do something *<!-- -->/
 *  }
 * </programlisting></informalexample>
 **/
typedef int cairo_bool_t;


/**
 * cairo_status_t:
 * @CAIRO_STATUS_SUCCESS: no error has occurred
 * @CAIRO_STATUS_NO_MEMORY: out of memory
 * @CAIRO_STATUS_INVALID_RESTORE: cairo_restore() called without matching cairo_save()
 * @CAIRO_STATUS_INVALID_POP_GROUP: no saved group to pop, i.e. cairo_pop_group() without matching cairo_push_group()
 * @CAIRO_STATUS_NO_CURRENT_POINT: no current point defined
 * @CAIRO_STATUS_INVALID_MATRIX: invalid matrix (not invertible)
 * @CAIRO_STATUS_INVALID_STATUS: invalid value for an input #cairo_status_t
 * @CAIRO_STATUS_NULL_POINTER: %NULL pointer
 * @CAIRO_STATUS_INVALID_STRING: input string not valid UTF-8
 * @CAIRO_STATUS_INVALID_PATH_DATA: input path data not valid
 * @CAIRO_STATUS_READ_ERROR: error while reading from input stream
 * @CAIRO_STATUS_WRITE_ERROR: error while writing to output stream
 * @CAIRO_STATUS_SURFACE_FINISHED: target surface has been finished
 * @CAIRO_STATUS_SURFACE_TYPE_MISMATCH: the surface type is not appropriate for the operation
 * @CAIRO_STATUS_PATTERN_TYPE_MISMATCH: the pattern type is not appropriate for the operation
 * @CAIRO_STATUS_INVALID_CONTENT: invalid value for an input #cairo_content_t
 * @CAIRO_STATUS_INVALID_FORMAT: invalid value for an input #cairo_format_t
 * @CAIRO_STATUS_INVALID_VISUAL: invalid value for an input Visual*
 * @CAIRO_STATUS_FILE_NOT_FOUND: file not found
 * @CAIRO_STATUS_INVALID_DASH: invalid value for a dash setting
 * @CAIRO_STATUS_INVALID_DSC_COMMENT: invalid value for a DSC comment (Since 1.2)
 * @CAIRO_STATUS_INVALID_INDEX: invalid index passed to getter (Since 1.4)
 * @CAIRO_STATUS_CLIP_NOT_REPRESENTABLE: clip region not representable in desired format (Since 1.4)
 * @CAIRO_STATUS_TEMP_FILE_ERROR: error creating or writing to a temporary file (Since 1.6)
 * @CAIRO_STATUS_INVALID_STRIDE: invalid value for stride (Since 1.6)
 * @CAIRO_STATUS_FONT_TYPE_MISMATCH: the font type is not appropriate for the operation (Since 1.8)
 * @CAIRO_STATUS_USER_FONT_IMMUTABLE: the user-font is immutable (Since 1.8)
 * @CAIRO_STATUS_USER_FONT_ERROR: error occurred in a user-font callback function (Since 1.8)
 * @CAIRO_STATUS_NEGATIVE_COUNT: negative number used where it is not allowed (Since 1.8)
 * @CAIRO_STATUS_INVALID_CLUSTERS: input clusters do not represent the accompanying text and glyph array (Since 1.8)
 * @CAIRO_STATUS_INVALID_SLANT: invalid value for an input #cairo_font_slant_t (Since 1.8)
 * @CAIRO_STATUS_INVALID_WEIGHT: invalid value for an input #cairo_font_weight_t (Since 1.8)
 * @CAIRO_STATUS_INVALID_SIZE: invalid value (typically too big) for the size of the input (surface, pattern, etc.) (Since 1.10)
 * @CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED: user-font method not implemented (Since 1.10)
 * @CAIRO_STATUS_LAST_STATUS: this is a special value indicating the number of
 *   status values defined in this enumeration.  When using this value, note
 *   that the version of cairo at run-time may have additional status values
 *   defined than the value of this symbol at compile-time. (Since 1.10)
 *
 * #cairo_status_t is used to indicate errors that can occur when
 * using Cairo. In some cases it is returned directly by functions.
 * but when using #cairo_t, the last error, if any, is stored in
 * the context and can be retrieved with cairo_status().
 *
 * New entries may be added in future versions.  Use cairo_status_to_string()
 * to get a human-readable representation of an error message.
 **/
typedef enum _cairo_status {
    CAIRO_STATUS_SUCCESS = 0,

    CAIRO_STATUS_NO_MEMORY,
    CAIRO_STATUS_INVALID_RESTORE,
    CAIRO_STATUS_INVALID_POP_GROUP,
    CAIRO_STATUS_NO_CURRENT_POINT,
    CAIRO_STATUS_INVALID_MATRIX,
    CAIRO_STATUS_INVALID_STATUS,
    CAIRO_STATUS_NULL_POINTER,
    CAIRO_STATUS_INVALID_STRING,
    CAIRO_STATUS_INVALID_PATH_DATA,
    CAIRO_STATUS_READ_ERROR,
    CAIRO_STATUS_WRITE_ERROR,
    CAIRO_STATUS_SURFACE_FINISHED,
    CAIRO_STATUS_SURFACE_TYPE_MISMATCH,
    CAIRO_STATUS_PATTERN_TYPE_MISMATCH,
    CAIRO_STATUS_INVALID_CONTENT,
    CAIRO_STATUS_INVALID_FORMAT,
    CAIRO_STATUS_INVALID_VISUAL,
    CAIRO_STATUS_FILE_NOT_FOUND,
    CAIRO_STATUS_INVALID_DASH,
    CAIRO_STATUS_INVALID_DSC_COMMENT,
    CAIRO_STATUS_INVALID_INDEX,
    CAIRO_STATUS_CLIP_NOT_REPRESENTABLE,
    CAIRO_STATUS_TEMP_FILE_ERROR,
    CAIRO_STATUS_INVALID_STRIDE,
    CAIRO_STATUS_FONT_TYPE_MISMATCH,
    CAIRO_STATUS_USER_FONT_IMMUTABLE,
    CAIRO_STATUS_USER_FONT_ERROR,
    CAIRO_STATUS_NEGATIVE_COUNT,
    CAIRO_STATUS_INVALID_CLUSTERS,
    CAIRO_STATUS_INVALID_SLANT,
    CAIRO_STATUS_INVALID_WEIGHT,
    CAIRO_STATUS_INVALID_SIZE,
    CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED,

    CAIRO_STATUS_LAST_STATUS
} cairo_status_t;

/**
 * cairo_fill_rule_t:
 * @CAIRO_FILL_RULE_WINDING: If the path crosses the ray from
 * left-to-right, counts +1. If the path crosses the ray
 * from right to left, counts -1. (Left and right are determined
 * from the perspective of looking along the ray from the starting
 * point.) If the total count is non-zero, the point will be filled.
 * @CAIRO_FILL_RULE_EVEN_ODD: Counts the total number of
 * intersections, without regard to the orientation of the contour. If
 * the total number of intersections is odd, the point will be
 * filled.
 *
 * #cairo_fill_rule_t is used to select how paths are filled. For both
 * fill rules, whether or not a point is included in the fill is
 * determined by taking a ray from that point to infinity and looking
 * at intersections with the path. The ray can be in any direction,
 * as long as it doesn't pass through the end point of a segment
 * or have a tricky intersection such as intersecting tangent to the path.
 * (Note that filling is not actually implemented in this way. This
 * is just a description of the rule that is applied.)
 *
 * The default fill rule is %CAIRO_FILL_RULE_WINDING.
 *
 * New entries may be added in future versions.
 **/
typedef enum _cairo_fill_rule {
    CAIRO_FILL_RULE_WINDING,
    CAIRO_FILL_RULE_EVEN_ODD
} cairo_fill_rule_t;

/* Region functions */

typedef struct _cairo_region cairo_region_t;

typedef struct _cairo_rectangle_int {
    int x, y;
    int width, height;
} cairo_rectangle_int_t;

typedef enum _cairo_region_overlap {
    CAIRO_REGION_OVERLAP_IN,		/* completely inside region */
    CAIRO_REGION_OVERLAP_OUT,		/* completely outside region */
    CAIRO_REGION_OVERLAP_PART		/* partly inside region */
} cairo_region_overlap_t;

CAIRO_END_DECLS

#endif /* _CAIRO_MINIMAL_H_ */
