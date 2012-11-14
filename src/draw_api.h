/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 2012 PCB Contributors (See ChangeLog for details)
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

#ifndef PCB_DRAW_API_H
#define PCB_DRAW_API_H

typedef struct DrawAPI DrawAPI;
typedef struct GraphicsAPI GraphicsAPI;

struct DrawAPI {
  /* Virtual functions */

  /* Isolated calls, corresponding to actual copper / mask / paste features. These don't call other dapi's */

  void (*draw_pcb_pin)          (DrawAPI *,              PinType *);
  void (*draw_pcb_pin_mask)     (DrawAPI *,              PinType *);
  void (*draw_pcb_pin_hole)     (DrawAPI *,              PinType *);
  void (*draw_pcb_via)          (DrawAPI *,              PinType *);
  void (*draw_pcb_via_mask)     (DrawAPI *,              PinType *);
  void (*draw_pcb_via_hole)     (DrawAPI *,              PinType *);
  void (*draw_pcb_pad)          (DrawAPI *, LayerType *, PadType *);
  void (*draw_pcb_pad_mask)     (DrawAPI *, LayerType *, PadType *);
  void (*draw_pcb_pad_paste)    (DrawAPI *, LayerType *, PadType *);
  void (*draw_pcb_line)         (DrawAPI *, LayerType *, LineType *);
  void (*draw_pcb_arc)          (DrawAPI *, LayerType *, ArcType *);
  void (*draw_pcb_text)         (DrawAPI *, LayerType *, TextType *, Coord /* <-(HACK) */ );
  void (*draw_pcb_polygon)      (DrawAPI *, LayerType *, PolygonType *);

  /* Nasty, but pragmatic - a lot of our renderers will want to pick a colour */
  void (*set_color_for_pin)     (DrawAPI *,              PinType *);
  void (*set_color_for_via)     (DrawAPI *,              PinType *);
  void (*set_color_for_pad)     (DrawAPI *, LayerType *, PadType *);
  void (*set_color_for_line)    (DrawAPI *, LayerType *, LineType *);
  void (*set_color_for_arc)     (DrawAPI *, LayerType *, ArcType *);
  void (*set_color_for_text)    (DrawAPI *, LayerType *, TextType *);
  void (*set_color_for_polygon) (DrawAPI *, LayerType *, PolygonType *);

//  void (*draw_rat)              (DrawAPI *,              RatType *);

  /* May call other dapi functions */
  void (*draw_pcb_element)      (DrawAPI *, ElementType *);

  /* Operates by calling other functions */
//  void (*draw_holes)            (DrawAPI *,              int);
//  void (*draw_ppv)              (DrawAPI *, LayerType *, int);
  void (*draw_pcb_layer)        (DrawAPI *, LayerType *);
  void (*draw_pcb_layer_group)  (DrawAPI *, int);
  void (*draw_mask_layer)       (DrawAPI *, int);
  void (*draw_paste_layer)      (DrawAPI *, int);
  void (*draw_silk_layer)       (DrawAPI *, int);
  void (*draw_pcb_buffer)       (DrawAPI *, BufferType *);
  void (*draw_everything)       (DrawAPI *);
  void (*draw_pinout_preview)   (DrawAPI *, ElementType *);

  /* Setup dapi API rendering parameters */
  void (*set_draw_offset)      (DrawAPI *, Coord, Coord);
  void (*set_clip_box)         (DrawAPI *, const BoxType *);

  /* Member variables */
  const BoxType *clip_box; /* Clipping box for the current drawing operation                  */
  bool doing_overlay_text; /* Override for colour / style of overlay text such as pin names   */
  bool doing_pinout;       /* Override for visibility / style when drawing a pinout preview   */
  bool doing_assy;         /* Override for visibility / style when making an assembly drawing */

//  GraphicsAPI *gapi;       /* <--- This should be in a subclass of the base dapi, but nevermind */
  HID_DRAW_API *graphics;  /* <--- This should be in a subclass of the base dapi, but nevermind */
  hidGC gc;                /* <--- This should be in a subclass of the base dapi, but nevermind */
  hidGC fg_gc;             /* <--- This should be in a subclass of the base dapi, but nevermind */
  hidGC bg_gc;             /* <--- This should be in a subclass of the base dapi, but nevermind */
};

#if 0
enum mask_mode {
  HID_MASK_OFF    = 0, /* Flush the buffer and return to non-mask operation. */
  HID_MASK_BEFORE = 1, /* Polygons being drawn before clears.                */
  HID_MASK_CLEAR  = 2, /* Clearances being drawn.                            */
  HID_MASK_AFTER  = 3, /* Polygons being drawn after clears.                 */
};
#endif

#if 0
struct GraphicsAPI {
  /* Make an empty graphics context. */
  hidGC (*make_gc) (void);
  void (*destroy_gc) (hidGC gc);
  void (*use_mask) (enum mask_mode mode);

  /* Set a color.  Names can be like "red" or "#rrggbb" or special
   * names like "erase".  *Always* use the "erase" color for removing
   * ink (like polygon reliefs or thermals), as you cannot rely on
   * knowing the background color or special needs of the HID.  Always
   * use the "drill" color to draw holes.  You may assume this is
   * cheap enough to call inside the redraw callback, but not cheap
   * enough to call for each item drawn.
   */
  void (*set_color) (hidGC gc, const char *name);

  /* Set the line style.  While calling this is cheap, calling it with
   * different values each time may be expensive, so grouping items by
   * line style is helpful.
   */
  void (*set_line_cap) (hidGC gc, EndCapStyle style);
  void (*set_line_width) (hidGC gc, Coord width);
  void (*set_draw_xor) (hidGC gc, int xor);

  /* Blends 20% or so color with 80% background.  Only used for
   * assembly drawings so far.
   */
  void (*set_draw_faded) (hidGC gc, int faded);

  /* The usual drawing functions.  "draw" means to use segments of the
   * given width, whereas "fill" means to fill to a zero-width
   * outline.
   */
  void (*draw_line)            (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2);
  void (*draw_arc)             (hidGC gc, Coord cx, Coord cy, Coord rx, Coord ry, Angle sa, Angle da);
  void (*draw_rect)            (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2);
  void (*fill_circle)          (hidGC gc, Coord cx, Coord cy, Coord radius);
  void (*fill_polygon)         (hidGC gc, int n_coords, Coord *x, Coord *y);
  void (*fill_rect)            (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2);
};
#endif

#if 0
  void (*fill_pcb_polygon)     (hidGC gc, PolygonType *poly, const BoxType *clip_box);
  void (*thindraw_pcb_polygon) (hidGC gc, PolygonType *poly, const BoxType *clip_box);
  void (*fill_pcb_pad)         (hidGC gc, PadType *pad, bool clip, bool mask);
  void (*thindraw_pcb_pad)     (hidGC gc, PadType *pad, bool clip, bool mask);
  void (*fill_pcb_pv)          (hidGC fg_gc, hidGC bg_gc, PinType *pv, bool drawHole, bool mask);
  void (*thindraw_pcb_pv)      (hidGC fg_gc, hidGC bg_gc, PinType *pv, bool drawHole, bool mask);
#endif

#endif
