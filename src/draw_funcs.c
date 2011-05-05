
#include "global.h"
#include "data.h"
#include "draw_funcs.h"

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
};

struct draw_funcs *dapi = &d_f;
