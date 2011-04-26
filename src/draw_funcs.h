struct draw_funcs {
  void (*draw_pin)       (PinType *,     void *);
  void (*draw_pin_mask)  (PinType *,     void *);
  void (*draw_via)       (PinType *,     void *);
  void (*draw_via_mask)  (PinType *,     void *);
  void (*draw_hole)      (PinType *,     void *);
  void (*draw_pad)       (PadType *,     void *);
  void (*draw_pad_mask)  (PadType *,     void *);
  void (*draw_pad_paste) (PadType *,     void *);
  void (*draw_line)      (LineType *,    void *);
  void (*draw_rat)       (RatType *,     void *);
  void (*draw_arc)       (ArcType *,     void *);
  void (*draw_poly)      (PolygonType *, void *);
};

extern struct draw_funcs *dapi;
