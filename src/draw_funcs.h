struct draw_funcs {
  void (*draw_pin)       (PinType *,     const BoxType *, void *);
  void (*draw_pin_mask)  (PinType *,     const BoxType *, void *);
  void (*draw_via)       (PinType *,     const BoxType *, void *);
  void (*draw_via_mask)  (PinType *,     const BoxType *, void *);
  void (*draw_hole)      (PinType *,     const BoxType *, void *);
  void (*draw_pad)       (PadType *,     const BoxType *, void *);
  void (*draw_pad_mask)  (PadType *,     const BoxType *, void *);
  void (*draw_pad_paste) (PadType *,     const BoxType *, void *);
  void (*draw_line)      (LineType *,    const BoxType *, void *);
  void (*draw_rat)       (RatType *,     const BoxType *, void *);
  void (*draw_arc)       (ArcType *,     const BoxType *, void *);
  void (*draw_poly)      (PolygonType *, const BoxType *, void *);
<<<<<<< current
  void (*draw_layer)     (LayerType *,   const BoxType *, void *);
=======
>>>>>>> patched
};

extern struct draw_funcs *dapi;
