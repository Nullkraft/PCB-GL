
#include "global.h"
#include "data.h"
#include "misc.h"
#include "rtree.h"
#include "draw_funcs.h"
#include "draw.h"
#include "hid_draw.h"

void ghid_set_lock_effects (hidGC gc, AnyObjectType *object);

static void
draw_pin (hidGC gc, PinType *pin, void *userdata)
{
  hid_draw_pcb_pv (gc, pin, false);
}

static void
draw_pin_mask (hidGC gc, PinType *pin, void *userdata)
{
  hid_draw_pcb_pv (Output.pmGC, pin, true);
}

static void
draw_via (hidGC gc, PinType *via, void *userdata)
{
  hid_draw_pcb_pv (gc, via, false);
}

static void
draw_via_mask (hidGC gc, PinType *via, void *userdata)
{
  hid_draw_pcb_pv (Output.pmGC, via, true);
}

static void
draw_hole (hidGC gc, PinType *pv, void *userdata)
{
  if (!TEST_FLAG (THINDRAWFLAG, PCB))
    hid_draw_fill_circle (Output.bgGC, pv->X, pv->Y, pv->DrillingHole / 2);

  if (TEST_FLAG (THINDRAWFLAG, PCB) || TEST_FLAG (HOLEFLAG, pv))
    {
      hid_draw_set_line_cap (gc, Round_Cap);
      hid_draw_set_line_width (gc, 0);

      hid_draw_arc (gc, pv->X, pv->Y, pv->DrillingHole / 2, pv->DrillingHole / 2, 0, 360);
    }
}

static void
_draw_pad (hidGC gc, PadType *pad, bool clear, bool mask)
{
  if (clear && !mask && pad->Clearance <= 0)
    return;

  hid_draw_pcb_pad (gc, pad, clear, mask);
}

static void
draw_pad (hidGC gc, PadType *pad, void *userdata)
{
  _draw_pad (gc, pad, false, false);
}

static void
draw_pad_mask (hidGC gc, PadType *pad, void *userdata)
{
  if (pad->Mask <= 0)
    return;

  _draw_pad (Output.pmGC, pad, true, true);
}

static void
draw_pad_paste (hidGC gc, PadType *pad, void *userdata)
{
  if (TEST_FLAG (NOPASTEFLAG, pad) || pad->Mask <= 0)
    return;

  if (pad->Mask < pad->Thickness)
    _draw_pad (gc, pad, true, true);
  else
    _draw_pad (gc, pad, false, false);
}

static void
draw_line (hidGC gc, LineType *line, void *userdata)
{
  hid_draw_pcb_line (gc, line);
}

static void
draw_rat (hidGC gc, RatType *rat, void *userdata)
{
  if (Settings.RatThickness < 100)
    rat->Thickness = pixel_slop * Settings.RatThickness;
  /* rats.c set VIAFLAG if this rat goes to a containing poly: draw a donut */
  if (TEST_FLAG(VIAFLAG, rat))
    {
      int w = rat->Thickness;

      if (TEST_FLAG (THINDRAWFLAG, PCB))
        hid_draw_set_line_width (gc, 0);
      else
        hid_draw_set_line_width (gc, w);
      hid_draw_arc (gc, rat->Point1.X, rat->Point1.Y, w * 2, w * 2, 0, 360);
    }
  else
    hid_draw_pcb_line (gc, (LineType *) rat);
}

static void
draw_arc (hidGC gc, ArcType *arc, void *userdata)
{
  hid_draw_pcb_arc (gc, arc);
}

static void
draw_poly (hidGC gc, PolygonType *polygon, void *userdata)
{
  hid_draw_pcb_polygon (gc, polygon);
}

static void
set_object_color (hidGC gc, AnyObjectType *obj, char *warn_color, char *selected_color,
                  char *connected_color, char *found_color, char *normal_color)
{
  char *color;

  if      (warn_color      != NULL && TEST_FLAG (WARNFLAG,      obj)) color = warn_color;
  else if (selected_color  != NULL && TEST_FLAG (SELECTEDFLAG,  obj)) color = selected_color;
  else if (connected_color != NULL && TEST_FLAG (CONNECTEDFLAG, obj)) color = connected_color;
  else if (found_color     != NULL && TEST_FLAG (FOUNDFLAG,     obj)) color = found_color;
  else                                                                color = normal_color;

  hid_draw_set_color (gc, color);
}

static void
set_layer_object_color (hidGC gc, LayerType *layer, AnyObjectType *obj)
{
  set_object_color (gc, obj, NULL, layer->SelectedColor, PCB->ConnectedColor, PCB->FoundColor, layer->Color);
}

struct layer_info {
  hidGC gc;
  LayerType *layer;
};

static int
line_callback (const BoxType * b, void *cl)
{
  LineType *line = (LineType *)b;
  struct layer_info *info = cl;

  ghid_set_lock_effects (info->gc, (AnyObjectType *)line);
  set_layer_object_color (info->gc, info->layer, (AnyObjectType *) line);
  dapi->draw_line (info->gc, line, NULL);
  return 1;
}

static int
arc_callback (const BoxType * b, void *cl)
{
  ArcType *arc = (ArcType *)b;
  struct layer_info *info = cl;

  ghid_set_lock_effects (info->gc, (AnyObjectType *)arc);
  set_layer_object_color (info->gc, info->layer, (AnyObjectType *) arc);
  dapi->draw_arc (info->gc, arc, NULL);
  return 1;
}

static int
poly_callback (const BoxType * b, void *cl)
{
  PolygonType *polygon = (PolygonType *)b;
  struct layer_info *info = cl;

  ghid_set_lock_effects (info->gc, (AnyObjectType *)polygon);
  set_layer_object_color (info->gc, info->layer, (AnyObjectType *) polygon);
  dapi->draw_poly (info->gc, polygon, NULL);
  return 1;
}

static int
text_callback (const BoxType * b, void *cl)
{
  TextType *text = (TextType *)b;
  struct layer_info *info = cl;
  int min_silk_line;

  ghid_set_lock_effects (info->gc, (AnyObjectType *)text);
  if (TEST_FLAG (SELECTEDFLAG, text))
    hid_draw_set_color (info->gc, info->layer->SelectedColor);
  else
    hid_draw_set_color (info->gc, info->layer->Color);
  if (info->layer == &PCB->Data->SILKLAYER ||
      info->layer == &PCB->Data->BACKSILKLAYER)
    min_silk_line = PCB->minSlk;
  else
    min_silk_line = PCB->minWid;
  hid_draw_pcb_text (info->gc, text, min_silk_line);
  return 1;
}

static void
set_pv_inlayer_color (hidGC gc, PinType *pv, LayerType *layer, int type)
{
  ghid_set_lock_effects (gc, (AnyObjectType *)pv);
  if (TEST_FLAG (WARNFLAG, pv))          hid_draw_set_color (gc, PCB->WarnColor);
  else if (TEST_FLAG (SELECTEDFLAG, pv)) hid_draw_set_color (gc, (type == VIA_TYPE) ? PCB->ViaSelectedColor
                                                                                    : PCB->PinSelectedColor);
  else if (TEST_FLAG (FOUNDFLAG, pv))    hid_draw_set_color (gc, PCB->ConnectedColor);
  else                                   hid_draw_set_color (gc, layer->Color);
}

static int
pin_inlayer_callback (const BoxType * b, void *cl)
{
  struct layer_info *info = cl;

  set_pv_inlayer_color (info->gc, (PinType *)b, info->layer, PIN_TYPE);
  dapi->draw_pin (info->gc, (PinType *)b, NULL);
  return 1;
}

static int
via_inlayer_callback (const BoxType * b, void *cl)
{
  struct layer_info *info = cl;

  set_pv_inlayer_color (info->gc, (PinType *)b, info->layer, VIA_TYPE);
  dapi->draw_via (info->gc, (PinType *)b, NULL);
  return 1;
}

struct side_info {
  hidGC gc;
  LayerType *layer;
  int side;
};

static int
pad_inlayer_callback (const BoxType * b, void *cl)
{
  PadType *pad = (PadType *)b;
  struct side_info *info = cl;

  if (ON_SIDE (pad, info->side))
    {
      ghid_set_lock_effects (info->gc, (AnyObjectType *)pad);
      if (TEST_FLAG (WARNFLAG, pad))          hid_draw_set_color (info->gc, PCB->WarnColor);
      else if (TEST_FLAG (SELECTEDFLAG, pad)) hid_draw_set_color (info->gc, PCB->PinSelectedColor);
      else if (TEST_FLAG (FOUNDFLAG, pad))    hid_draw_set_color (info->gc, PCB->ConnectedColor);
      else                                    hid_draw_set_color (info->gc, info->layer->Color);

      dapi->draw_pad (info->gc, pad, NULL);
    }
  return 1;
}

struct hole_info {
  hidGC gc;
  int plated;
};

static int
pin_hole_callback (const BoxType * b, void *cl)
{
  PinType *pin = (PinType *)b;
  struct hole_info *info = cl;

  if ((info->plated == 0 && !TEST_FLAG (HOLEFLAG, pin)) ||
      (info->plated == 1 &&  TEST_FLAG (HOLEFLAG, pin)))
    return 1;

  ghid_set_lock_effects (info->gc, (AnyObjectType *)pin);
  set_object_color (info->gc, (AnyObjectType *) pin, PCB->WarnColor,
                    PCB->PinSelectedColor, NULL, NULL, Settings.BlackColor);

  dapi->draw_pin_hole (info->gc, pin, NULL);
  return 1;
}

static int
via_hole_callback (const BoxType * b, void *cl)
{
  PinType *via = (PinType *)b;
  struct hole_info *info = cl;

  if ((info->plated == 0 && !TEST_FLAG (HOLEFLAG, via)) ||
      (info->plated == 1 &&  TEST_FLAG (HOLEFLAG, via)))
    return 1;

  ghid_set_lock_effects (info->gc, (AnyObjectType *)via);
  set_object_color (info->gc, (AnyObjectType *) via, PCB->WarnColor,
                    PCB->ViaSelectedColor, NULL, NULL, Settings.BlackColor);

  dapi->draw_via_hole (info->gc, via, NULL);
  return 1;
}

static int
pin_callback (const BoxType * b, void *cl)
{
  hidGC gc = cl;

  ghid_set_lock_effects (gc, (AnyObjectType *)b);
  set_object_color (gc, (AnyObjectType *)b,
                    PCB->WarnColor, PCB->PinSelectedColor,
                    PCB->ConnectedColor, PCB->FoundColor, PCB->PinColor);

  dapi->draw_pin (gc, (PinType *)b, NULL);
  return 1;
}

static int
via_callback (const BoxType * b, void *cl)
{
  hidGC gc = cl;

  ghid_set_lock_effects (gc, (AnyObjectType *)b);
  set_object_color (gc, (AnyObjectType *)b,
                    PCB->WarnColor, PCB->ViaSelectedColor,
                    PCB->ConnectedColor, PCB->FoundColor, PCB->ViaColor);

  dapi->draw_via (gc, (PinType *)b, NULL);
  return 1;
}

static int
pad_callback (const BoxType * b, void *cl)
{
  PadType *pad = (PadType *) b;
  struct side_info *info = cl;

  if (ON_SIDE (pad, info->side))
    {
      ghid_set_lock_effects (info->gc, (AnyObjectType *)pad);
      set_object_color (info->gc, (AnyObjectType *)pad, PCB->WarnColor,
                        PCB->PinSelectedColor, PCB->ConnectedColor, PCB->FoundColor,
                        FRONT (pad) ? PCB->PinColor : PCB->InvisibleObjectsColor);

      dapi->draw_pad (info->gc, pad, NULL);
    }
  return 1;
}

static void
draw_ppv (hidGC gc, int group, void *userdata)
{
  HID_DRAW *hid_draw = gc->hid_draw;
  int top_group = GetLayerGroupNumberBySide (TOP_SIDE);
  int bottom_group = GetLayerGroupNumberBySide (BOTTOM_SIDE);

  if (PCB->PinOn || !gui->gui)
    {
      struct side_info info;

      /* draw element pins */
      r_search (PCB->Data->pin_tree, hid_draw->clip_box, NULL, pin_callback, gc);

      info.gc = gc;
      info.layer = NULL; /* Nasty, but saves creating a load of different info types */

      /* draw element pads */
      if (group == top_group)
        {
          info.side = TOP_SIDE;
          r_search (PCB->Data->pad_tree, hid_draw->clip_box, NULL, pad_callback, &info);
        }

      if (group == bottom_group)
        {
          info.side = BOTTOM_SIDE;
          r_search (PCB->Data->pad_tree, hid_draw->clip_box, NULL, pad_callback, &info);
        }
    }

  /* draw vias */
  if (PCB->ViaOn)
    r_search (PCB->Data->via_tree, hid_draw->clip_box, NULL, via_callback, gc);

  dapi->draw_holes (gc, -1, NULL);
}

static void
draw_holes (hidGC gc, int plated, void *userdata)
{
  HID_DRAW *hid_draw = gc->hid_draw;
  struct hole_info info;

  info.gc = gc;
  info.plated = plated;

  if (PCB->PinOn)
    r_search (PCB->Data->pin_tree, hid_draw->clip_box, NULL, pin_hole_callback, &info);
  if (PCB->ViaOn)
    r_search (PCB->Data->via_tree, hid_draw->clip_box, NULL, via_hole_callback, &info);
}

static void
draw_layer (hidGC gc, LayerType *layer, void *userdata)
{
  HID_DRAW *hid_draw = gc->hid_draw;
  int top_group = GetLayerGroupNumberBySide (TOP_SIDE);
  int bottom_group = GetLayerGroupNumberBySide (BOTTOM_SIDE);
  int layer_num = GetLayerNumber (PCB->Data, layer);
  int group = GetLayerGroupNumberByPointer (layer);
  bool is_outline;
  struct hole_info h_info;
  struct layer_info l_info;

  h_info.gc = gc;
  h_info.plated = -1; /* Draw both plated and unplated holes */
  l_info.gc = gc;
  l_info.layer = layer;

  is_outline = strcmp (layer->Name, "outline") == 0 ||
               strcmp (layer->Name, "route") == 0;

  if (layer_num < max_copper_layer && !is_outline)
    {
      r_search (PCB->Data->pin_tree, hid_draw->clip_box, NULL, pin_hole_callback, &h_info);
      r_search (PCB->Data->via_tree, hid_draw->clip_box, NULL, via_hole_callback, &h_info);
    }

  /* print the non-clearing polys */
  r_search (layer->polygon_tree, hid_draw->clip_box, NULL, poly_callback, &l_info);

  if (TEST_FLAG (CHECKPLANESFLAG, PCB))
    return;

  /* draw all visible lines this layer */
  r_search (layer->line_tree, hid_draw->clip_box, NULL, line_callback, &l_info);
  r_search (layer->arc_tree,  hid_draw->clip_box, NULL, arc_callback,  &l_info);
  r_search (layer->text_tree, hid_draw->clip_box, NULL, text_callback, &l_info);

  /* We should check for gui->gui here, but it's kinda cool seeing the
     auto-outline magically disappear when you first add something to
     the "outline" layer.  */

  if (is_outline)
    {
      if (IsLayerEmpty (layer))
        {
          hid_draw_set_color (gc, layer->Color);
          hid_draw_set_line_width (gc, PCB->minWid);
          hid_draw_rect (gc, 0, 0, PCB->MaxWidth, PCB->MaxHeight);
        }
      return;
    }

  /* Don't draw vias on silk layers */
  if (layer_num >= max_copper_layer)
    return;

  /* Pins, pads and vias are handled in elsewhere by exporters, don't duplicate */
  if (!gui->gui)
    return;

  r_search (PCB->Data->pin_tree, hid_draw->clip_box, NULL, pin_inlayer_callback, &l_info);

  /* draw element pads */
  if (group == top_group ||
      group == bottom_group)
    {
      struct side_info s_info;

      s_info.gc = gc;
      s_info.layer = layer;
      s_info.side = (group == bottom_group) ? BOTTOM_SIDE : TOP_SIDE;
      r_search (PCB->Data->pad_tree, hid_draw->clip_box, NULL, pad_inlayer_callback, &s_info);
    }

  /* draw vias */
  r_search (PCB->Data->via_tree, hid_draw->clip_box, NULL, via_inlayer_callback, &l_info);
}

struct draw_funcs d_f = {
  .draw_pin       = draw_pin,
  .draw_pin_mask  = draw_pin_mask,
  .draw_pin_hole  = draw_hole,
  .draw_via       = draw_via,
  .draw_via_mask  = draw_via_mask,
  .draw_via_hole  = draw_hole,
  .draw_pad       = draw_pad,
  .draw_pad_mask  = draw_pad_mask,
  .draw_pad_paste = draw_pad_paste,
  .draw_line      = draw_line,
  .draw_rat       = draw_rat,
  .draw_arc       = draw_arc,
  .draw_poly      = draw_poly,

  .draw_ppv       = draw_ppv,
  .draw_holes     = draw_holes,
  .draw_layer     = draw_layer,
};

struct draw_funcs *dapi = &d_f;
