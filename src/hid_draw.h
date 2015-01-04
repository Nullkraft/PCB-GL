enum mask_mode {
  HID_MASK_OFF    = 0, /* Flush the buffer and return to non-mask operation. */
  HID_MASK_BEFORE = 1, /* Polygons being drawn before clears.                */
  HID_MASK_CLEAR  = 2, /* Clearances being drawn.                            */
  HID_MASK_AFTER  = 3, /* Polygons being drawn after clears.                 */
};

/* Low level drawing API */

struct hid_draw_st
{
  /* Drawing Functions.  Coordinates and distances are ALWAYS in PCB's
     default coordinates (1 nm at the time this comment was written).
     Angles are always in degrees, with 0 being "right" (positive X)
     and 90 being "up" (positive Y).  */

  /* Make an empty graphics context.  */
  hidGC (*make_gc) (void);
  void (*destroy_gc) (hidGC gc);
  void (*use_mask) (enum mask_mode mode);

  /* Set a color.  Names can be like "red" or "#rrggbb" or special
     names like "erase".  *Always* use the "erase" color for removing
     ink (like polygon reliefs or thermals), as you cannot rely on
     knowing the background color or special needs of the HID.  Always
     use the "drill" color to draw holes.  You may assume this is
     cheap enough to call inside the redraw callback, but not cheap
     enough to call for each item drawn. */
  void (*set_color) (hidGC gc, const char *name);

  /* Set the line style.  While calling this is cheap, calling it with
     different values each time may be expensive, so grouping items by
     line style is helpful.  */
  void (*set_line_cap) (hidGC gc, EndCapStyle style);
  void (*set_line_width) (hidGC gc, Coord width);
  void (*set_draw_xor) (hidGC gc, int xor_);

  /* Blends 20% or so color with 80% background.  Only used for
     assembly drawings so far. */
  void (*set_draw_faded) (hidGC gc, int faded);

  /* The usual drawing functions.  "draw" means to use segments of the
     given width, whereas "fill" means to fill to a zero-width
     outline.  */
  void (*draw_line)    (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2);
  void (*draw_arc)     (hidGC gc, Coord cx, Coord cy, Coord xradius, Coord yradius, Angle start_angle, Angle delta_angle);
  void (*draw_rect)    (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2);
  void (*fill_circle)  (hidGC gc, Coord cx, Coord cy, Coord radius);
  void (*fill_polygon) (hidGC gc, int n_coords, Coord *x, Coord *y);
  void (*fill_rect)    (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2);

  /* The following APIs render using PCB data-structures, not immediate parameters */

  void (*draw_pcb_line) (hidGC gc, LineType *line);
  void (*draw_pcb_arc) (hidGC gc, ArcType *arc);
  void (*draw_pcb_text) (hidGC gc, TextType *, Coord);
  void (*draw_pcb_polygon) (hidGC gc, PolygonType *poly, const BoxType *clip_box);

  void (*fill_pcb_polygon) (hidGC gc, PolygonType *poly, const BoxType *clip_box);
  void (*thindraw_pcb_polygon) (hidGC gc, PolygonType *poly, const BoxType *clip_box);
  void (*fill_pcb_pad) (hidGC gc, PadType *pad, bool clip, bool mask);
  void (*thindraw_pcb_pad) (hidGC gc, PadType *pad, bool clip, bool mask);
  void (*fill_pcb_pv) (hidGC fg_gc, hidGC bg_gc, PinType *pv, bool drawHole, bool mask);
  void (*thindraw_pcb_pv) (hidGC fg_gc, hidGC bg_gc, PinType *pv, bool drawHole, bool mask);

};

/* Base hidGC elements visible to any module */
struct hid_gc_struct {
  HID *hid;   /* Used by HIDs to validate the GCs passed belong to them */
  HID_DRAW *hid_draw;
};

/* Calling wrappers to access the vfunc table */

inline hidGC
hid_draw_make_gc (HID_DRAW *hid_draw)
{
  return hid_draw->make_gc ();
}

inline void
hid_draw_destroy_gc (hidGC gc)
{
  gc->hid_draw->destroy_gc (gc);
}

inline void
hid_draw_use_mask (HID_DRAW *hid_draw, enum mask_mode mode)
{
  hid_draw->use_mask (mode);
}

inline void
hid_draw_set_color (hidGC gc, char *name)
{
  gc->hid_draw->set_color (gc, name);
}

inline void
hid_draw_set_line_cap (hidGC gc, EndCapStyle style)
{
  gc->hid_draw->set_line_cap (gc, style);
}

inline void
hid_draw_set_line_width (hidGC gc, Coord width)
{
  gc->hid_draw->set_line_width (gc, width);
}

inline void
hid_draw_set_draw_xor (hidGC gc, int xor_)
{
  gc->hid_draw->set_draw_xor (gc, xor_);
}

inline void
hid_draw_set_draw_faded (hidGC gc, int faded)
{
  gc->hid_draw->set_draw_faded (gc, faded);
}

inline void
hid_draw_line (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2)
{
  gc->hid_draw->draw_line (gc, x1, y1, x2, y2);
}

inline void
hid_draw_arc (hidGC gc, Coord cx, Coord cy, Coord xradius, Coord yradius, Angle start_angle, Angle delta_angle)
{
  gc->hid_draw->draw_arc (gc, cx, cy, xradius, yradius, start_angle, delta_angle);
}

inline void
hid_draw_rect (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2)
{
  gc->hid_draw->draw_rect (gc, x1, y1, x2, y2);
}

inline void
hid_draw_fill_circle (hidGC gc, Coord cx, Coord cy, Coord radius)
{
  gc->hid_draw->fill_circle (gc, cx, cy, radius);
}

inline void
hid_draw_fill_polygon (hidGC gc, int n_coords, Coord *x, Coord *y)
{
  gc->hid_draw->fill_polygon (gc, n_coords, x, y);
}

inline void
hid_draw_fill_rect (hidGC gc, Coord x1, Coord y1, Coord x2, Coord y2)
{
  gc->hid_draw->fill_rect (gc, x1, y1, x2, y2);
}


inline void
hid_draw_pcb_line (hidGC gc, LineType *line)
{
  gc->hid_draw->draw_pcb_line (gc, line);
}

inline void
hid_draw_pcb_arc (hidGC gc, ArcType *arc)
{
  gc->hid_draw->draw_pcb_arc (gc, arc);
}

inline void
hid_draw_pcb_text (hidGC gc, TextType *text, Coord min_width)
{
  gc->hid_draw->draw_pcb_text (gc, text, min_width);
}

inline void
hid_draw_pcb_polygon (hidGC gc, PolygonType *poly, const BoxType *clip_box)
{
  gc->hid_draw->draw_pcb_polygon (gc, poly, clip_box);
}


inline void
hid_draw_fill_pcb_polygon (hidGC gc, PolygonType *poly, const BoxType *clip_box)
{
  gc->hid_draw->fill_pcb_polygon (gc, poly, clip_box);
}

inline void
hid_draw_thin_pcb_polygon (hidGC gc, PolygonType *poly, const BoxType *clip_box)
{
  gc->hid_draw->thindraw_pcb_polygon (gc, poly, clip_box);
}

inline void
hid_draw_fill_pcb_pad (hidGC gc, PadType *pad, bool clip, bool mask)
{
  gc->hid_draw->fill_pcb_pad (gc, pad, clip, mask);
}

inline void
hid_draw_thin_pcb_pad (hidGC gc, PadType *pad, bool clip, bool mask)
{
  gc->hid_draw->thindraw_pcb_pad (gc, pad, clip, mask);
}

inline void
hid_draw_fill_pcb_pv (hidGC fg_gc, hidGC bg_gc, PinType *pv, bool draw_hole, bool mask)
{
  fg_gc->hid_draw->fill_pcb_pv (fg_gc, bg_gc, pv, draw_hole, mask);
}

inline void
hid_draw_thin_pcb_pv (hidGC fg_gc, hidGC bg_gc, PinType *pv, bool draw_hole, bool mask)
{
  fg_gc->hid_draw->thindraw_pcb_pv (fg_gc, bg_gc, pv, draw_hole, mask);
}
