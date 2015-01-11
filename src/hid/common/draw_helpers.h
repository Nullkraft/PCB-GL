void common_set_clip_box (hidGC gc, BoxType *clip_box);
void common_gui_draw_pcb_polygon (hidGC gc, PolygonType *poly);
void common_fill_pcb_polygon (hidGC gc, PolygonType *poly);
void common_thindraw_pcb_polygon (hidGC gc, PolygonType *poly);
void common_gui_draw_pcb_pad (hidGC gc, PadType *pad, bool clear, bool mask);
void common_fill_pcb_pad (hidGC gc, PadType *pad, bool clear, bool mask);
void common_thindraw_pcb_pad (hidGC gc, PadType *pad, bool clear, bool mask);
void common_gui_draw_pcb_pv (hidGC gc, PinType *pv, bool mask);
void common_gui_draw_pcb_pv_hole (hidGC gc, PinType *pv);
void common_fill_pcb_pv (hidGC gc, PinType *pv, bool mask);
void common_fill_pcb_pv_hole (hidGC gc, PinType *pv);
void common_thindraw_pcb_pv (hidGC gc, PinType *pv, bool mask);
void common_thindraw_pcb_pv_hole (hidGC gc, PinType *pv);
void common_draw_helpers_init (HID_DRAW *graphics);
