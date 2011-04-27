
#include "global.h"
#include "data.h"
#include "misc.h"
#include "rtree.h"
#include "draw_funcs.h"
#include "draw.h"

static void
_draw_pv (PinType *pv, bool draw_hole)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    gui->thindraw_pcb_pv (Output.fgGC, Output.fgGC, pv, draw_hole, false);
  else
    gui->fill_pcb_pv (Output.fgGC, Output.bgGC, pv, draw_hole, false);
}

static void
draw_pin (PinType *pin, const BoxType *drawn_area, void *userdata)
{
  _draw_pv (pin, false);
}

static void
draw_pin_mask (PinType *pin, const BoxType *drawn_area, void *userdata)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB) || TEST_FLAG (THINDRAWPOLYFLAG, PCB))
    gui->thindraw_pcb_pv (Output.pmGC, Output.pmGC, pin, false, true);
  else
    gui->fill_pcb_pv (Output.pmGC, Output.pmGC, pin, false, true);
}

static void
draw_via (PinType *via, const BoxType *drawn_area, void *userdata)
{
  _draw_pv (via, false);
}

static void
draw_via_mask (PinType *via, const BoxType *drawn_area, void *userdata)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB) || TEST_FLAG (THINDRAWPOLYFLAG, PCB))
    gui->thindraw_pcb_pv (Output.pmGC, Output.pmGC, via, false, true);
  else
    gui->fill_pcb_pv (Output.pmGC, Output.pmGC, via, false, true);
}

static void
draw_hole (PinType *pv, const BoxType *drawn_area, void *userdata)
{
  if (!TEST_FLAG (THINDRAWFLAG, PCB))
    gui->fill_circle (Output.bgGC, pv->X, pv->Y, pv->DrillingHole / 2);

  if (TEST_FLAG (THINDRAWFLAG, PCB) || TEST_FLAG (HOLEFLAG, pv))
    {
      gui->set_line_cap (Output.fgGC, Round_Cap);
      gui->set_line_width (Output.fgGC, 0);

      gui->draw_arc (Output.fgGC, pv->X, pv->Y,
                     pv->DrillingHole / 2, pv->DrillingHole / 2, 0, 360);
    }
}

static void
_draw_pad (hidGC gc, PadType *pad, bool clear, bool mask)
{
  if (clear && !mask && pad->Clearance <= 0)
    return;

  if (TEST_FLAG (THINDRAWFLAG, PCB) ||
      (clear && TEST_FLAG (THINDRAWPOLYFLAG, PCB)))
    gui->thindraw_pcb_pad (gc, pad, clear, mask);
  else
    gui->fill_pcb_pad (gc, pad, clear, mask);
}

static void
draw_pad (PadType *pad, const BoxType *drawn_area, void *userdata)
{
  _draw_pad (Output.fgGC, pad, false, false);
}

static void
draw_pad_mask (PadType *pad, const BoxType *drawn_area, void *userdata)
{
  if (pad->Mask <= 0)
    return;

  _draw_pad (Output.pmGC, pad, true, true);
}

static void
draw_pad_paste (PadType *pad, const BoxType *drawn_area, void *userdata)
{
  if (TEST_FLAG (NOPASTEFLAG, pad) || pad->Mask <= 0)
    return;

  if (pad->Mask < pad->Thickness)
    _draw_pad (Output.fgGC, pad, true, true);
  else
    _draw_pad (Output.fgGC, pad, false, false);
}

static void
_draw_line (LineType *line)
{
  gui->set_line_cap (Output.fgGC, Trace_Cap);
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    gui->set_line_width (Output.fgGC, 0);
  else
    gui->set_line_width (Output.fgGC, line->Thickness);

  gui->draw_line (Output.fgGC,
                  line->Point1.X, line->Point1.Y,
                  line->Point2.X, line->Point2.Y);
}

static void
draw_line (LineType *line, const BoxType *drawn_area, void *userdata)
{
  _draw_line (line);
}

static void
draw_rat (RatType *rat, const BoxType *drawn_area, void *userdata)
{
  if (Settings.RatThickness < 20)
    rat->Thickness = pixel_slop * Settings.RatThickness;
  /* rats.c set VIAFLAG if this rat goes to a containing poly: draw a donut */
  if (TEST_FLAG(VIAFLAG, rat))
    {
      int w = rat->Thickness;

      if (TEST_FLAG (THINDRAWFLAG, PCB))
        gui->set_line_width (Output.fgGC, 0);
      else
        gui->set_line_width (Output.fgGC, w);
      gui->draw_arc (Output.fgGC, rat->Point1.X, rat->Point1.Y,
                     w * 2, w * 2, 0, 360);
    }
  else
    _draw_line ((LineType *) rat);
}

static void
draw_arc (ArcType *arc, const BoxType *drawn_area, void *userdata)
{
  if (!arc->Thickness)
    return;

  if (TEST_FLAG (THINDRAWFLAG, PCB))
    gui->set_line_width (Output.fgGC, 0);
  else
    gui->set_line_width (Output.fgGC, arc->Thickness);
  gui->set_line_cap (Output.fgGC, Trace_Cap);

  gui->draw_arc (Output.fgGC, arc->X, arc->Y, arc->Width,
                 arc->Height, arc->StartAngle, arc->Delta);
}

static void
draw_poly (PolygonType *polygon, const BoxType *drawn_area, void *userdata)
{
  if (!polygon->Clipped)
    return;

  if (gui->thindraw_pcb_polygon != NULL &&
      (TEST_FLAG (THINDRAWFLAG, PCB) ||
       TEST_FLAG (THINDRAWPOLYFLAG, PCB)))
    gui->thindraw_pcb_polygon (Output.fgGC, polygon, drawn_area);
  else
    gui->fill_pcb_polygon (Output.fgGC, polygon, drawn_area);

  /* If checking planes, thin-draw any pieces which have been clipped away */
  if (gui->thindraw_pcb_polygon != NULL &&
      TEST_FLAG (CHECKPLANESFLAG, PCB) &&
      !TEST_FLAG (FULLPOLYFLAG, polygon))
    {
      PolygonType poly = *polygon;

      for (poly.Clipped = polygon->Clipped->f;
           poly.Clipped != polygon->Clipped;
           poly.Clipped = poly.Clipped->f)
        gui->thindraw_pcb_polygon (Output.fgGC, &poly, drawn_area);
    }
}

static int
line_callback (const BoxType * b, void *cl)
{
  LayerType *layer = cl;
  LineType *line = (LineType *)b;

  if (TEST_FLAG (SELECTEDFLAG, line))   gui->set_color (Output.fgGC, layer->SelectedColor);
  else if (TEST_FLAG (FOUNDFLAG, line)) gui->set_color (Output.fgGC, PCB->ConnectedColor);
  else                                  gui->set_color (Output.fgGC, layer->Color);

  dapi->draw_line (line, NULL, NULL);
  return 1;
}

static int
arc_callback (const BoxType * b, void *cl)
{
  LayerType *layer = cl;
  ArcType *arc = (ArcType *)b;

  if (TEST_FLAG (SELECTEDFLAG, arc))   gui->set_color (Output.fgGC, layer->SelectedColor);
  else if (TEST_FLAG (FOUNDFLAG, arc)) gui->set_color (Output.fgGC, PCB->ConnectedColor);
  else                                 gui->set_color (Output.fgGC, layer->Color);

  dapi->draw_arc (arc, NULL, NULL);
  return 1;
}

struct poly_info {
  const const BoxType *drawn_area;
  LayerType *layer;
};

static int
poly_callback (const BoxType * b, void *cl)
{
  struct poly_info *i = cl;
  PolygonType *polygon = (PolygonType *)b;

  if (TEST_FLAG (SELECTEDFLAG, polygon))   gui->set_color (Output.fgGC, i->layer->SelectedColor);
  else if (TEST_FLAG (FOUNDFLAG, polygon)) gui->set_color (Output.fgGC, PCB->ConnectedColor);
  else                                     gui->set_color (Output.fgGC, i->layer->Color);

  dapi->draw_poly (polygon, i->drawn_area, NULL);
  return 1;
}

static int
text_callback (const BoxType * b, void *cl)
{
  LayerType *layer = cl;
  TextType *text = (TextType *)b;
  int min_silk_line;

  if (TEST_FLAG (SELECTEDFLAG, text))
    gui->set_color (Output.fgGC, layer->SelectedColor);
  else
    gui->set_color (Output.fgGC, layer->Color);
  if (layer == &PCB->Data->SILKLAYER ||
      layer == &PCB->Data->BACKSILKLAYER)
    min_silk_line = PCB->minSlk;
  else
    min_silk_line = PCB->minWid;
  DrawTextLowLevel (text, min_silk_line);
  return 1;
}

static void
set_pv_inlayer_color (PinType *pv, LayerType *layer, int type)
{
  if (TEST_FLAG (WARNFLAG, pv))          gui->set_color (Output.fgGC, PCB->WarnColor);
  else if (TEST_FLAG (SELECTEDFLAG, pv)) gui->set_color (Output.fgGC, (type == VIA_TYPE) ? PCB->ViaSelectedColor
                                                                                         : PCB->PinSelectedColor);
  else if (TEST_FLAG (FOUNDFLAG, pv))    gui->set_color (Output.fgGC, PCB->ConnectedColor);
  else                                   gui->set_color (Output.fgGC, layer->Color);
}

static int
pin_inlayer_callback (const BoxType * b, void *cl)
{
  set_pv_inlayer_color ((PinType *)b, cl, PIN_TYPE);
  dapi->draw_pin ((PinType *)b, NULL, NULL);
  return 1;
}

static int
via_inlayer_callback (const BoxType * b, void *cl)
{
  set_pv_inlayer_color ((PinType *)b, cl, VIA_TYPE);
  dapi->draw_via ((PinType *)b, NULL, NULL);
  return 1;
}

static int
pad_inlayer_callback (const BoxType * b, void *cl)
{
  PadType* pad = (PadType *)b;
  LayerType *layer = cl;
  int solder_group = GetLayerGroupNumberByNumber (solder_silk_layer);
  int group = GetLayerGroupNumberByPointer (layer);

  int side = (group == solder_group) ? SOLDER_LAYER : COMPONENT_LAYER;

  if (ON_SIDE (pad, side))
    {
      if (TEST_FLAG (WARNFLAG, pad))          gui->set_color (Output.fgGC, PCB->WarnColor);
      else if (TEST_FLAG (SELECTEDFLAG, pad)) gui->set_color (Output.fgGC, PCB->PinSelectedColor);
      else if (TEST_FLAG (FOUNDFLAG, pad))    gui->set_color (Output.fgGC, PCB->ConnectedColor);
      else                                    gui->set_color (Output.fgGC, layer->Color);

      dapi->draw_pad (pad, NULL, NULL);
    }
  return 1;
}

static int
pin_hole_callback (const BoxType * b, void *cl)
{
  PinType *pin = (PinType*)b;
  int plated = cl ? *(int *) cl : -1;

  if ((plated == 0 && !TEST_FLAG (HOLEFLAG, pin)) ||
      (plated == 1 &&  TEST_FLAG (HOLEFLAG, pin)))
    return 1;

  if (TEST_FLAG (WARNFLAG, pin))          gui->set_color (Output.fgGC, PCB->WarnColor);
  else if (TEST_FLAG (SELECTEDFLAG, pin)) gui->set_color (Output.fgGC, PCB->PinSelectedColor);
  else                                    gui->set_color (Output.fgGC, Settings.BlackColor);

  dapi->draw_pin_hole (pin, NULL, NULL);
  return 1;
}

static int
via_hole_callback (const BoxType * b, void *cl)
{
  PinType *via = (PinType*)b;
  int plated = cl ? *(int *) cl : -1;

  if ((plated == 0 && !TEST_FLAG (HOLEFLAG, via)) ||
      (plated == 1 &&  TEST_FLAG (HOLEFLAG, via)))
    return 1;

  if (TEST_FLAG (WARNFLAG, via))          gui->set_color (Output.fgGC, PCB->WarnColor);
  else if (TEST_FLAG (SELECTEDFLAG, via)) gui->set_color (Output.fgGC, PCB->ViaSelectedColor);
  else                                    gui->set_color (Output.fgGC, Settings.BlackColor);

  dapi->draw_via_hole (via, NULL, NULL);
  return 1;
}

static void
set_pv_color (PinType *pv, int type)
{
  if (TEST_FLAG (WARNFLAG, pv))          gui->set_color (Output.fgGC, PCB->WarnColor);
  else if (TEST_FLAG (SELECTEDFLAG, pv)) gui->set_color (Output.fgGC, (type == VIA_TYPE) ? PCB->ViaSelectedColor
                                                                                         : PCB->PinSelectedColor);
  else if (TEST_FLAG (FOUNDFLAG, pv))    gui->set_color (Output.fgGC, PCB->ConnectedColor);
  else                                   gui->set_color (Output.fgGC, (type == VIA_TYPE) ? PCB->ViaColor
                                                                                         : PCB->PinColor);
}

static int
pin_callback (const BoxType * b, void *cl)
{
  set_pv_color ((PinType *)b, PIN_TYPE);
  dapi->draw_pin ((PinType *)b, NULL, NULL);
  return 1;
}

static int
via_callback (const BoxType * b, void *cl)
{
  set_pv_color ((PinType *)b, VIA_TYPE);
  dapi->draw_via ((PinType *)b, NULL, NULL);
  return 1;
}

static int
pad_callback (const BoxType * b, void *cl)
{
  PadTypePtr pad = (PadTypePtr) b;
  int *side = cl;

  if (ON_SIDE (pad, *side))
    {
      if (TEST_FLAG (WARNFLAG, pad))          gui->set_color (Output.fgGC, PCB->WarnColor);
      else if (TEST_FLAG (SELECTEDFLAG, pad)) gui->set_color (Output.fgGC, PCB->PinSelectedColor);
      else if (TEST_FLAG (FOUNDFLAG, pad))    gui->set_color (Output.fgGC, PCB->ConnectedColor);
      else if (FRONT (pad))                   gui->set_color (Output.fgGC, PCB->PinColor);
      else                                    gui->set_color (Output.fgGC, PCB->InvisibleObjectsColor);

      dapi->draw_pad (pad, NULL, NULL);
    }
  return 1;
}

static void
draw_ppv (int group, const BoxType *drawn_area, void *userdata)
{
  int component_group = GetLayerGroupNumberByNumber (component_silk_layer);
  int solder_group = GetLayerGroupNumberByNumber (solder_silk_layer);
  int side;

  if (PCB->PinOn || !gui->gui)
    {
      /* draw element pins */
      r_search (PCB->Data->pin_tree, drawn_area, NULL, pin_callback, NULL);

      /* draw element pads */
      if (group == component_group)
        {
          side = COMPONENT_LAYER;
          r_search (PCB->Data->pad_tree, drawn_area, NULL, pad_callback, &side);
        }

      if (group == solder_group)
        {
          side = SOLDER_LAYER;
          r_search (PCB->Data->pad_tree, drawn_area, NULL, pad_callback, &side);
        }
    }

  /* draw vias */
  if (PCB->ViaOn)
    r_search (PCB->Data->via_tree, drawn_area, NULL, via_callback, NULL);

  dapi->draw_holes (-1, drawn_area, NULL);
}

static void
draw_holes (int plated, const BoxType *drawn_area, void *userdata)
{
  if (PCB->PinOn)
    r_search (PCB->Data->pin_tree, drawn_area, NULL, pin_hole_callback, &plated);
  if (PCB->ViaOn)
    r_search (PCB->Data->via_tree, drawn_area, NULL, via_hole_callback, &plated);
}

static void
draw_layer (LayerType *layer, const BoxType *drawn_area, void *userdata)
{
  int component_group = GetLayerGroupNumberByNumber (component_silk_layer);
  int solder_group = GetLayerGroupNumberByNumber (solder_silk_layer);
  int layer_num = GetLayerNumber (PCB->Data, layer);
  int group = GetLayerGroupNumberByPointer (layer);
  struct poly_info info = {drawn_area, layer};

  /* print the non-clearing polys */
  r_search (layer->polygon_tree, drawn_area, NULL, poly_callback, &info);

  if (TEST_FLAG (CHECKPLANESFLAG, PCB))
    return;

  r_search (layer->line_tree, drawn_area, NULL, line_callback, layer);
  r_search (layer->arc_tree,  drawn_area, NULL, arc_callback,  layer);
  r_search (layer->text_tree, drawn_area, NULL, text_callback, layer);

  /* We should check for gui->gui here, but it's kinda cool seeing the
     auto-outline magically disappear when you first add something to
     the "outline" layer.  */

  if (strcmp (layer->Name, "outline") == 0 ||
      strcmp (layer->Name, "route") == 0)
    {
      if (IsLayerEmpty (layer))
        {
          gui->set_color (Output.fgGC, layer->Color);
          gui->set_line_width (Output.fgGC, PCB->minWid);
          gui->draw_rect (Output.fgGC, 0, 0, PCB->MaxWidth, PCB->MaxHeight);
        }
      return;
    }

  /* Don't draw vias on silk layers */
  if (layer_num >= max_copper_layer)
    return;

  r_search (PCB->Data->pin_tree, drawn_area, NULL, pin_inlayer_callback, layer);

  /* draw element pads */
  if (group == component_group)
    r_search (PCB->Data->pad_tree, drawn_area, NULL, pad_inlayer_callback, layer);

  if (group == solder_group)
    r_search (PCB->Data->pad_tree, drawn_area, NULL, pad_inlayer_callback, layer);

  /* draw vias */
  r_search (PCB->Data->via_tree, drawn_area, NULL, via_inlayer_callback, layer);
  r_search (PCB->Data->pin_tree, drawn_area, NULL, pin_hole_callback, NULL);
  r_search (PCB->Data->via_tree, drawn_area, NULL, via_hole_callback, NULL);
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
