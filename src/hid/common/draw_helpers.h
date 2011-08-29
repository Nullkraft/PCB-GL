
void common_draw_pin       (PinType *,     const BoxType *, void *);
void common_draw_pin_mask  (PinType *,     const BoxType *, void *);
void common_draw_pin_hole  (PinType *,     const BoxType *, void *);
void common_draw_via       (PinType *,     const BoxType *, void *);
void common_draw_via_mask  (PinType *,     const BoxType *, void *);
void common_draw_via_hole  (PinType *,     const BoxType *, void *);
void common_draw_pad       (PadType *,     const BoxType *, void *);
void common_draw_pad_mask  (PadType *,     const BoxType *, void *);
void common_draw_pad_paste (PadType *,     const BoxType *, void *);
void common_draw_line      (LineType *,    const BoxType *, void *);
void common_draw_arc       (ArcType *,     const BoxType *, void *);
void common_draw_poly      (PolygonType *, const BoxType *, void *);

void common_draw_rat       (RatType *,     const BoxType *, void *);

void common_draw_ppv       (int,           const BoxType *, void *);
void common_draw_holes     (int,           const BoxType *, void *);
void common_draw_layer     (LayerType *,   const BoxType *, void *);



void common_draw_helpers_init (HID *hid);
