struct draw_funcs {
  void (*draw_pin)       (hidGC, PinType *,     void *);
  void (*draw_pin_mask)  (hidGC, PinType *,     void *);
  void (*draw_pin_hole)  (hidGC, PinType *,     void *);
  void (*draw_via)       (hidGC, PinType *,     void *);
  void (*draw_via_mask)  (hidGC, PinType *,     void *);
  void (*draw_via_hole)  (hidGC, PinType *,     void *);
  void (*draw_pad)       (hidGC, PadType *,     void *);
  void (*draw_pad_mask)  (hidGC, PadType *,     void *);
  void (*draw_pad_paste) (hidGC, PadType *,     void *);
  void (*draw_line)      (hidGC, LineType *,    void *);
  void (*draw_rat)       (hidGC, RatType *,     void *);
  void (*draw_arc)       (hidGC, ArcType *,     void *);
  void (*draw_poly)      (hidGC, PolygonType *, void *);
  void (*draw_ppv)       (hidGC, int,           void *);
  void (*draw_holes)     (hidGC, int,           void *);
  void (*draw_layer)     (hidGC, LayerType *,   void *);
};

extern struct draw_funcs *dapi;
