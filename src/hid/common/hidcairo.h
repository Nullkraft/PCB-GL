HID_DRAW *hidcairo_init (void);
struct render_priv *hidcairo_start_render (cairo_t *cr);
void hidcairo_finish_render (struct render_priv *priv);

//extern HID_DRAW cairo_graphics;
