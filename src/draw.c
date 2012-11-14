/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996, 2003, 2004 Thomas Nau
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Contact addresses for paper mail and Email:
 *  Thomas Nau, Schlehenweg 15, 88471 Baustetten, Germany
 *  Thomas.Nau@rz.uni-ulm.de
 *
 */


/* drawing routines
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "global.h"

/*#include "clip.h"*/
#include "compat.h"
#include "crosshair.h"
#include "data.h"
#include "draw.h"
#include "error.h"
#include "mymem.h"
#include "misc.h"
#include "rotate.h"
#include "rtree.h"
#include "search.h"
#include "select.h"
#include "print.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

#undef NDEBUG
#include <assert.h>

#ifndef MAXINT
#define MAXINT (((unsigned int)(~0))>>1)
#endif

#define	SMALL_SMALL_TEXT_SIZE	0
#define	SMALL_TEXT_SIZE			1
#define	NORMAL_TEXT_SIZE		2
#define	LARGE_TEXT_SIZE			3
#define	N_TEXT_SIZES			4


/* ---------------------------------------------------------------------------
 * some local identifiers
 */
static BoxType Block = {MAXINT, MAXINT, -MAXINT, -MAXINT};

/* ---------------------------------------------------------------------------
 * some local prototypes
 */
static void DrawPPV (DrawAPI *dapi, int group);
static void AddPart (void *);
static void DrawEMark (DrawAPI *dapi, ElementType *, Coord, Coord, bool);
static void DrawRats (DrawAPI *dapi);

struct side_info {
  DrawAPI *dapi;
  int side;
};

/*--------------------------------------------------------------------------------------
 * setup color for various objects
 */
static void
set_color_for_pin (DrawAPI *dapi, PinType *pin)
{
  char *color = PCB->PinColor;

  if (!dapi->doing_pinout)
    {
      if (TEST_FLAG (WARNFLAG, pin))
        color = PCB->WarnColor;
      else if (TEST_FLAG (SELECTEDFLAG, pin))
        color = PCB->PinSelectedColor;
      else if (TEST_FLAG (FOUNDFLAG, pin))
        color = PCB->ConnectedColor;
    }

  dapi->graphics->set_color (dapi->fg_gc, color);
}

static void
set_color_for_via (DrawAPI *dapi, PinType *via)
{
  char *color = PCB->ViaColor;

  if (!dapi->doing_pinout)
    {
      if (TEST_FLAG (WARNFLAG, via))
        color = PCB->WarnColor;
      else if (TEST_FLAG (SELECTEDFLAG, via))
        color = PCB->ViaSelectedColor;
      else if (TEST_FLAG (FOUNDFLAG, via))
        color = PCB->ConnectedColor;
    }

  dapi->graphics->set_color (dapi->fg_gc, color);
}

static void
set_color_for_pad (DrawAPI *dapi, LayerType *layer, PadType *pad)
{
  char *color = PCB->PinColor;

  if (!dapi->doing_pinout)
    {
      if (TEST_FLAG (WARNFLAG, pad))
        color = PCB->WarnColor;
      else if (TEST_FLAG (SELECTEDFLAG, pad))
        color = PCB->PinSelectedColor;
      else if (TEST_FLAG (FOUNDFLAG, pad))
        color = PCB->ConnectedColor;
      else if (!FRONT (pad))
        color = PCB->InvisibleObjectsColor;
    }

  dapi->graphics->set_color (dapi->fg_gc, color);
}

/*---------------------------------------------------------------------------
 *  Adds the update rect to the update region
 */
static void
AddPart (void *b)
{
  BoxType *box = (BoxType *) b;

  Block.X1 = MIN (Block.X1, box->X1);
  Block.X2 = MAX (Block.X2, box->X2);
  Block.Y1 = MIN (Block.Y1, box->Y1);
  Block.Y2 = MAX (Block.Y2, box->Y2);
}

/*
 * initiate the actual redrawing of the updated area
 */
void
Draw (void)
{
  if (Block.X1 <= Block.X2 && Block.Y1 <= Block.Y2)
    gui->invalidate_lr (Block.X1, Block.X2, Block.Y1, Block.Y2);

  /* shrink the update block */
  Block.X1 = Block.Y1 =  MAXINT;
  Block.X2 = Block.Y2 = -MAXINT;
}

/* ---------------------------------------------------------------------- 
 * redraws all the data by the event handlers
 */
void
Redraw (void)
{
  gui->invalidate_all ();
}

static void
_draw_pv_name (DrawAPI *dapi, PinType *pv)
{
  BoxType box;
  bool vert;
  TextType text;

  if (!pv->Name || !pv->Name[0])
    text.TextString = EMPTY (pv->Number);
  else
    text.TextString = EMPTY (TEST_FLAG (SHOWNUMBERFLAG, PCB) ? pv->Number : pv->Name);

  vert = TEST_FLAG (EDGE2FLAG, pv);

  if (vert)
    {
      box.X1 = pv->X - pv->Thickness    / 2 + Settings.PinoutTextOffsetY;
      box.Y1 = pv->Y - pv->DrillingHole / 2 - Settings.PinoutTextOffsetX;
    }
  else
    {
      box.X1 = pv->X + pv->DrillingHole / 2 + Settings.PinoutTextOffsetX;
      box.Y1 = pv->Y - pv->Thickness    / 2 + Settings.PinoutTextOffsetY;
    }

  dapi->graphics->set_color (dapi->fg_gc, PCB->PinNameColor);

  text.Flags = NoFlags ();
  /* Set font height to approx 56% of pin thickness */
  text.Scale = 56 * pv->Thickness / FONT_CAPHEIGHT;
  text.X = box.X1;
  text.Y = box.Y1;
  text.Direction = vert ? 1 : 0;

  g_warn_if_fail (dapi->doing_overlay_text == false);
  dapi->doing_overlay_text = gui->gui;
  dapi->draw_pcb_text (dapi, NULL, &text, 0);
  dapi->doing_overlay_text = false;
}

static int
pin_callback (const BoxType * b, void *cl)
{
  DrawAPI *dapi = cl;
  PinType *pin = (PinType *)b;

  dapi->set_color_for_pin (dapi, pin);
  dapi->draw_pcb_pin (dapi, pin);

  if (!TEST_FLAG (HOLEFLAG, pin) && TEST_FLAG (DISPLAYNAMEFLAG, pin))
    _draw_pv_name (dapi, pin);
  return 1;
}

static int
via_callback (const BoxType * b, void *cl)
{
  DrawAPI *dapi = cl;
  PinType *via = (PinType *)b;

  dapi->set_color_for_via (dapi, via);
  dapi->draw_pcb_via (dapi, via);

  if (!TEST_FLAG (HOLEFLAG, via) && TEST_FLAG (DISPLAYNAMEFLAG, via))
    _draw_pv_name (dapi, via);
  return 1;
}

static void
draw_pad_name (DrawAPI *dapi, PadType *pad)
{
  BoxType box;
  bool vert;
  TextType text;

  if (!pad->Name || !pad->Name[0])
    text.TextString = EMPTY (pad->Number);
  else
    text.TextString = EMPTY (TEST_FLAG (SHOWNUMBERFLAG, PCB) ? pad->Number : pad->Name);

  /* should text be vertical ? */
  vert = (pad->Point1.X == pad->Point2.X);

  if (vert)
    {
      box.X1 = pad->Point1.X                      - pad->Thickness / 2;
      box.Y1 = MAX (pad->Point1.Y, pad->Point2.Y) + pad->Thickness / 2;
      box.X1 += Settings.PinoutTextOffsetY;
      box.Y1 -= Settings.PinoutTextOffsetX;
    }
  else
    {
      box.X1 = MIN (pad->Point1.X, pad->Point2.X) - pad->Thickness / 2;
      box.Y1 = pad->Point1.Y                      - pad->Thickness / 2;
      box.X1 += Settings.PinoutTextOffsetX;
      box.Y1 += Settings.PinoutTextOffsetY;
    }

  dapi->graphics->set_color (dapi->fg_gc, PCB->PinNameColor);

  text.Flags = NoFlags ();
  /* Set font height to approx 90% of pin thickness */
  text.Scale = 90 * pad->Thickness / FONT_CAPHEIGHT;
  text.X = box.X1;
  text.Y = box.Y1;
  text.Direction = vert ? 1 : 0;

  dapi->draw_pcb_text (dapi, NULL, &text, 0);
}

static void
draw_pad (DrawAPI *dapi, PadType *pad)
{
  dapi->set_color_for_pad (dapi, NULL, pad);
  dapi->draw_pcb_pad (dapi, NULL, pad);

  if (dapi->doing_pinout || TEST_FLAG (DISPLAYNAMEFLAG, pad))
    draw_pad_name (dapi, pad);
}

static int
pad_callback (const BoxType * b, void *cl)
{
  PadType *pad = (PadType *) b;
  struct side_info *info = cl;
  DrawAPI *dapi = info->dapi;

  if (ON_SIDE (pad, info->side))
    draw_pad (dapi, pad);
  return 1;
}

static void
draw_element_name (DrawAPI *dapi, ElementType *element)
{
  if ((TEST_FLAG (HIDENAMESFLAG, PCB) && gui->gui) ||
      TEST_FLAG (HIDENAMEFLAG, element))
    return;
  if (dapi->doing_overlay_text || dapi->doing_pinout || dapi->doing_assy)
    dapi->graphics->set_color (dapi->fg_gc, PCB->ElementColor);
  else if (TEST_FLAG (SELECTEDFLAG, &ELEMENT_TEXT (PCB, element)))
    dapi->graphics->set_color (dapi->fg_gc, PCB->ElementSelectedColor);
  else if (FRONT (element))
    dapi->graphics->set_color (dapi->fg_gc, PCB->ElementColor);
  else
    dapi->graphics->set_color (dapi->fg_gc, PCB->InvisibleObjectsColor);

  dapi->draw_pcb_text (dapi, NULL, &ELEMENT_TEXT (PCB, element), PCB->minSlk);
}

static int
name_callback (const BoxType * b, void *cl)
{
  TextType *text = (TextType *) b;
  ElementType *element = (ElementType *) text->Element;
  struct side_info *info = cl;
  DrawAPI *dapi = info->dapi;

  if (TEST_FLAG (HIDENAMEFLAG, element))
    return 0;

  if (ON_SIDE (element, info->side))
    draw_element_name (dapi, element);
  return 0;
}

static int
EMark_callback (const BoxType * b, void *cl)
{
  DrawAPI *dapi = cl;
  ElementType *element = (ElementType *) b;

  DrawEMark (dapi, element, element->MarkX, element->MarkY, !FRONT (element));
  return 1;
}

struct hole_info {
  DrawAPI *dapi;
  int plated;
};

static int
hole_callback (const BoxType * b, void *cl)
{
  PinType *pv = (PinType *) b;
  struct hole_info *info = cl;
  DrawAPI *dapi = info->dapi;

  if ((info->plated == 0 && !TEST_FLAG (HOLEFLAG, pv)) ||
      (info->plated == 1 &&  TEST_FLAG (HOLEFLAG, pv)))
    return 1;

  if (TEST_FLAG (THINDRAWFLAG, PCB))
    {
      if (!TEST_FLAG (HOLEFLAG, pv))
        {
          dapi->graphics->set_line_cap (dapi->fg_gc, Round_Cap);
          dapi->graphics->set_line_width (dapi->fg_gc, 0);
          dapi->graphics->draw_arc (dapi->fg_gc, pv->X, pv->Y, pv->DrillingHole / 2,
                                    pv->DrillingHole / 2, 0, 360);
        }
    }
  else
    dapi->graphics->fill_circle (dapi->bg_gc, pv->X, pv->Y, pv->DrillingHole / 2);

  if (TEST_FLAG (HOLEFLAG, pv))
    {
      if (TEST_FLAG (WARNFLAG, pv))
        dapi->graphics->set_color (dapi->fg_gc, PCB->WarnColor);
      else if (TEST_FLAG (SELECTEDFLAG, pv))
        dapi->graphics->set_color (dapi->fg_gc, PCB->ViaSelectedColor);
      else
        dapi->graphics->set_color (dapi->fg_gc, Settings.BlackColor);

      dapi->graphics->set_line_cap (dapi->fg_gc, Round_Cap);
      dapi->graphics->set_line_width (dapi->fg_gc, 0);
      dapi->graphics->draw_arc (dapi->fg_gc, pv->X, pv->Y, pv->DrillingHole / 2,
                                pv->DrillingHole / 2, 0, 360);
    }
  return 1;
}

void
DrawHoles (DrawAPI *dapi, bool draw_plated, bool draw_unplated)
{
  struct hole_info info;

  info.dapi = dapi;
  info.plated = -1;

  if ( draw_plated && !draw_unplated) info.plated = 1;
  if (!draw_plated &&  draw_unplated) info.plated = 0;

  r_search (PCB->Data->pin_tree, dapi->clip_box, NULL, hole_callback, &info);
  r_search (PCB->Data->via_tree, dapi->clip_box, NULL, hole_callback, &info);
}

static void
_draw_line (DrawAPI *dapi, LineType *line)
{
  dapi->graphics->set_line_cap (dapi->fg_gc, Trace_Cap);
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    dapi->graphics->set_line_width (dapi->fg_gc, 0);
  else
    dapi->graphics->set_line_width (dapi->fg_gc, line->Thickness);

  dapi->graphics->draw_line (dapi->fg_gc, line->Point1.X, line->Point1.Y,
                                          line->Point2.X, line->Point2.Y);
}

static void
draw_line (DrawAPI *dapi, LayerType *layer, LineType *line)
{
  if (TEST_FLAG (SELECTEDFLAG | FOUNDFLAG, line))
    {
      if (TEST_FLAG (SELECTEDFLAG, line))
        dapi->graphics->set_color (dapi->fg_gc, layer->SelectedColor);
      else
        dapi->graphics->set_color (dapi->fg_gc, PCB->ConnectedColor);
    }
  else
    dapi->graphics->set_color (dapi->fg_gc, layer->Color);
  _draw_line (dapi, line);
}

struct layer_info {
  DrawAPI *dapi;
  LayerType *layer;
};

static int
line_callback (const BoxType * b, void *cl)
{
  struct layer_info *info = cl;

  DrawAPI *dapi = info->dapi;
  draw_line (dapi, info->layer, (LineType *) b);
  return 1;
}

static int
rat_callback (const BoxType * b, void *cl)
{
  RatType *rat = (RatType *)b;
  DrawAPI *dapi = cl;

  if (TEST_FLAG (SELECTEDFLAG | FOUNDFLAG, rat))
    {
      if (TEST_FLAG (SELECTEDFLAG, rat))
        dapi->graphics->set_color (dapi->fg_gc, PCB->RatSelectedColor);
      else
        dapi->graphics->set_color (dapi->fg_gc, PCB->ConnectedColor);
    }
  else
    dapi->graphics->set_color (dapi->fg_gc, PCB->RatColor);

  if (Settings.RatThickness < 100)
    rat->Thickness = pixel_slop * Settings.RatThickness;
  /* rats.c set VIAFLAG if this rat goes to a containing poly: draw a donut */
  if (TEST_FLAG(VIAFLAG, rat))
    {
      int w = rat->Thickness;

      if (TEST_FLAG (THINDRAWFLAG, PCB))
        dapi->graphics->set_line_width (dapi->fg_gc, 0);
      else
        dapi->graphics->set_line_width (dapi->fg_gc, w);
      dapi->graphics->draw_arc (dapi->fg_gc, rat->Point1.X, rat->Point1.Y, w * 2, w * 2, 0, 360);
    }
  else
    _draw_line (dapi, (LineType *) rat);
  return 1;
}

static void
_draw_arc (DrawAPI *dapi, ArcType *arc)
{
  if (!arc->Thickness)
    return;

  if (TEST_FLAG (THINDRAWFLAG, PCB))
    dapi->graphics->set_line_width (dapi->fg_gc, 0);
  else
    dapi->graphics->set_line_width (dapi->fg_gc, arc->Thickness);
  dapi->graphics->set_line_cap (dapi->fg_gc, Trace_Cap);

  dapi->graphics->draw_arc (dapi->fg_gc, arc->X, arc->Y, arc->Width,
                            arc->Height, arc->StartAngle, arc->Delta);
}

static void
draw_arc (DrawAPI *dapi, LayerType *layer, ArcType *arc)
{
  if (TEST_FLAG (SELECTEDFLAG | FOUNDFLAG, arc))
    {
      if (TEST_FLAG (SELECTEDFLAG, arc))
        dapi->graphics->set_color (dapi->fg_gc, layer->SelectedColor);
      else
        dapi->graphics->set_color (dapi->fg_gc, PCB->ConnectedColor);
    }
  else
    dapi->graphics->set_color (dapi->fg_gc, layer->Color);

  _draw_arc (dapi, arc);
}

static int
arc_callback (const BoxType * b, void *cl)
{
  struct layer_info *info = cl;
  DrawAPI *dapi = info->dapi;

  draw_arc (dapi, info->layer, (ArcType *) b);
  return 1;
}

static void
draw_element_package (DrawAPI *dapi, ElementType *element)
{
  /* set color and draw lines, arcs, text and pins */
  if (dapi->doing_pinout || dapi->doing_assy)
    dapi->graphics->set_color (dapi->fg_gc, PCB->ElementColor);
  else if (TEST_FLAG (SELECTEDFLAG, element))
    dapi->graphics->set_color (dapi->fg_gc, PCB->ElementSelectedColor);
  else if (FRONT (element))
    dapi->graphics->set_color (dapi->fg_gc, PCB->ElementColor);
  else
    dapi->graphics->set_color (dapi->fg_gc, PCB->InvisibleObjectsColor);

  /* draw lines, arcs, text and pins */
  ELEMENTLINE_LOOP (element);
  {
    _draw_line (dapi, line);
  }
  END_LOOP;
  ARC_LOOP (element);
  {
    _draw_arc (dapi, arc);
  }
  END_LOOP;
}

static int
element_callback (const BoxType * b, void *cl)
{
  ElementType *element = (ElementType *) b;
  struct side_info *info = cl;
  DrawAPI *dapi = info->dapi;

  if (ON_SIDE (element, info->side))
    draw_element_package (dapi, element);
  return 1;
}

/* ---------------------------------------------------------------------------
 * prints assembly drawing.
 */

static void
PrintAssembly (DrawAPI *dapi, int side)
{
  int side_group = GetLayerGroupNumberByNumber (max_copper_layer + side);

  g_warn_if_fail (dapi->doing_assy == false);
  dapi->doing_assy = true;
  dapi->graphics->set_draw_faded (dapi->fg_gc, 1);
  dapi->draw_pcb_layer_group (dapi, side_group);
  dapi->graphics->set_draw_faded (dapi->fg_gc, 0);

  /* draw package */
  dapi->draw_silk_layer (dapi, side);
  dapi->doing_assy = false;
}

/* ---------------------------------------------------------------------------
 * initializes some identifiers for a new zoom factor and redraws whole screen
 */
static void
DrawEverything (DrawAPI *dapi)
{
  int i, ngroups;
  int component, solder;
  /* This is the list of layer groups we will draw.  */
  int do_group[MAX_LAYER];
  /* This is the reverse of the order in which we draw them.  */
  int drawn_groups[MAX_LAYER];
  int plated, unplated;
  bool paste_empty;

  PCB->Data->SILKLAYER.Color = PCB->ElementColor;
  PCB->Data->BACKSILKLAYER.Color = PCB->InvisibleObjectsColor;

  memset (do_group, 0, sizeof (do_group));
  for (ngroups = 0, i = 0; i < max_copper_layer; i++)
    {
      LayerType *l = LAYER_ON_STACK (i);
      int group = GetLayerGroupNumberByNumber (LayerStack[i]);
      if (l->On && !do_group[group])
	{
	  do_group[group] = 1;
	  drawn_groups[ngroups++] = group;
	}
    }

  component = GetLayerGroupNumberByNumber (component_silk_layer);
  solder = GetLayerGroupNumberByNumber (solder_silk_layer);

  /*
   * first draw all 'invisible' stuff
   */
  if (!TEST_FLAG (CHECKPLANESFLAG, PCB)
      && gui->set_layer ("invisible", SL (INVISIBLE, 0), 0))
    {
      struct side_info side_info;

      side_info.dapi = dapi;
      side_info.side = SWAP_IDENT ? COMPONENT_LAYER : SOLDER_LAYER;

      if (PCB->ElementOn)
	{
	  r_search (PCB->Data->element_tree, dapi->clip_box, NULL, element_callback, &side_info);
	  r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], dapi->clip_box, NULL, name_callback, &side_info);
	  dapi->draw_pcb_layer (dapi, &(PCB->Data->Layer[max_copper_layer + side_info.side]));
	}
      r_search (PCB->Data->pad_tree, dapi->clip_box, NULL, pad_callback, &side_info);
      gui->end_layer ();
    }

  /* draw all layers in layerstack order */
  for (i = ngroups - 1; i >= 0; i--)
    {
      int group = drawn_groups[i];

      if (gui->set_layer (0, group, 0))
        {
          dapi->draw_pcb_layer_group (dapi, group);
          gui->end_layer ();
        }
    }

  if (TEST_FLAG (CHECKPLANESFLAG, PCB) && gui->gui)
    return;

  /* Draw pins, pads, vias below silk */
  if (gui->gui)
    DrawPPV (dapi, SWAP_IDENT ? solder : component);
  else
    {
      CountHoles (&plated, &unplated, dapi->clip_box);

      if (plated && gui->set_layer ("plated-drill", SL (PDRILL, 0), 0))
        {
          DrawHoles (dapi, true, false);
          gui->end_layer ();
        }

      if (unplated && gui->set_layer ("unplated-drill", SL (UDRILL, 0), 0))
        {
          DrawHoles (dapi, false, true);
          gui->end_layer ();
        }
    }

  /* Draw the solder mask if turned on */
  if (gui->set_layer ("componentmask", SL (MASK, TOP), 0))
    {
      dapi->draw_mask_layer (dapi, COMPONENT_LAYER);
      gui->end_layer ();
    }

  if (gui->set_layer ("soldermask", SL (MASK, BOTTOM), 0))
    {
      dapi->draw_mask_layer (dapi, SOLDER_LAYER);
      gui->end_layer ();
    }

  if (gui->set_layer ("topsilk", SL (SILK, TOP), 0))
    {
      dapi->draw_silk_layer (dapi, COMPONENT_LAYER);
      gui->end_layer ();
    }

  if (gui->set_layer ("bottomsilk", SL (SILK, BOTTOM), 0))
    {
      dapi->draw_silk_layer (dapi, SOLDER_LAYER);
      gui->end_layer ();
    }

  if (gui->gui)
    {
      /* Draw element Marks */
      if (PCB->PinOn)
	r_search (PCB->Data->element_tree, dapi->clip_box, NULL, EMark_callback, dapi);
      /* Draw rat lines on top */
      if (gui->set_layer ("rats", SL (RATS, 0), 0))
        {
          DrawRats (dapi);
          gui->end_layer ();
        }
    }

  paste_empty = IsPasteEmpty (COMPONENT_LAYER);
  if (gui->set_layer ("toppaste", SL (PASTE, TOP), paste_empty))
    {
      dapi->draw_paste_layer (dapi, COMPONENT_LAYER);
      gui->end_layer ();
    }

  paste_empty = IsPasteEmpty (SOLDER_LAYER);
  if (gui->set_layer ("bottompaste", SL (PASTE, BOTTOM), paste_empty))
    {
      dapi->draw_paste_layer (dapi, SOLDER_LAYER);
      gui->end_layer ();
    }

  if (gui->set_layer ("topassembly", SL (ASSY, TOP), 0))
    {
      PrintAssembly (dapi, COMPONENT_LAYER);
      gui->end_layer ();
    }

  if (gui->set_layer ("bottomassembly", SL (ASSY, BOTTOM), 0))
    {
      PrintAssembly (dapi, SOLDER_LAYER);
      gui->end_layer ();
    }

  if (gui->set_layer ("fab", SL (FAB, 0), 0))
    {
      PrintFab (dapi);
      gui->end_layer ();
    }
}

static void
DrawEMark (DrawAPI *dapi, ElementType *e, Coord X, Coord Y, bool invisible)
{
  Coord mark_size = EMARK_SIZE;
  if (!PCB->InvisibleObjectsOn && invisible)
    return;

  if (e->Pin != NULL)
    {
      PinType *pin0 = e->Pin->data;
      if (TEST_FLAG (HOLEFLAG, pin0))
	mark_size = MIN (mark_size, pin0->DrillingHole / 2);
      else
	mark_size = MIN (mark_size, pin0->Thickness / 2);
    }

  if (e->Pad != NULL)
    {
      PadType *pad0 = e->Pad->data;
      mark_size = MIN (mark_size, pad0->Thickness / 2);
    }

  dapi->graphics->set_color (dapi->fg_gc, invisible ? PCB->InvisibleMarkColor : PCB->ElementColor);
  dapi->graphics->set_line_cap (dapi->fg_gc, Trace_Cap);
  dapi->graphics->set_line_width (dapi->fg_gc, 0);
  dapi->graphics->draw_line (dapi->fg_gc, X - mark_size, Y, X, Y - mark_size);
  dapi->graphics->draw_line (dapi->fg_gc, X + mark_size, Y, X, Y - mark_size);
  dapi->graphics->draw_line (dapi->fg_gc, X - mark_size, Y, X, Y + mark_size);
  dapi->graphics->draw_line (dapi->fg_gc, X + mark_size, Y, X, Y + mark_size);

  /*
   * If an element is locked, place a "L" on top of the "diamond".
   * This provides a nice visual indication that it is locked that
   * works even for color blind users.
   */
  if (TEST_FLAG (LOCKFLAG, e) )
    {
      dapi->graphics->draw_line (dapi->fg_gc, X, Y, X + 2 * mark_size, Y);
      dapi->graphics->draw_line (dapi->fg_gc, X, Y, X, Y - 4* mark_size);
    }
}

/* ---------------------------------------------------------------------------
 * Draws pins pads and vias - Always draws for non-gui HIDs,
 * otherwise drawing depends on PCB->PinOn and PCB->ViaOn
 */
static void
DrawPPV (DrawAPI *dapi, int group)
{
  int component_group = GetLayerGroupNumberByNumber (component_silk_layer);
  int solder_group = GetLayerGroupNumberByNumber (solder_silk_layer);
  struct hole_info hole_info;

  hole_info.dapi = dapi;
  hole_info.plated = -1;

  if (PCB->PinOn || !gui->gui)
    {
      struct side_info side_info;
      side_info.dapi = dapi;

      /* draw element pins */
      r_search (PCB->Data->pin_tree, dapi->clip_box, NULL, pin_callback, dapi);

      /* draw element pads */
      if (group == component_group)
        {
          side_info.side = COMPONENT_LAYER;
          r_search (PCB->Data->pad_tree, dapi->clip_box, NULL, pad_callback, &side_info);
        }

      if (group == solder_group)
        {
          side_info.side = SOLDER_LAYER;
          r_search (PCB->Data->pad_tree, dapi->clip_box, NULL, pad_callback, &side_info);
        }
    }

  /* draw vias */
  if (PCB->ViaOn || !gui->gui)
    {
      r_search (PCB->Data->via_tree, dapi->clip_box, NULL, via_callback, dapi);
      r_search (PCB->Data->via_tree, dapi->clip_box, NULL, hole_callback, &hole_info);
    }
  if (PCB->PinOn || dapi->doing_assy)
    r_search (PCB->Data->pin_tree, dapi->clip_box, NULL, hole_callback, &hole_info);
}

static int
poly_callback (const BoxType * b, void *cl)
{
  struct layer_info *info = cl;
  DrawAPI *dapi = info->dapi;
  PolygonType *polygon = (PolygonType *)b;
  static char *color;

  if (!polygon->Clipped)
    return 0;

  if (TEST_FLAG (SELECTEDFLAG, polygon))
    color = info->layer->SelectedColor;
  else if (TEST_FLAG (FOUNDFLAG, polygon))
    color = PCB->ConnectedColor;
  else
    color = info->layer->Color;
  dapi->graphics->set_color (dapi->fg_gc, color);

  dapi->draw_pcb_polygon (dapi, info->layer, polygon);

  /* If checking planes, thin-draw any pieces which have been clipped away */
  if ( TEST_FLAG (CHECKPLANESFLAG, PCB) &&
      !TEST_FLAG (FULLPOLYFLAG, polygon))
    {
      PolygonType poly = *polygon;

      /* XXX: SET FLAG TO MAKE THE GUI THINDRAW */

      for (poly.Clipped = polygon->Clipped->f;
           poly.Clipped != polygon->Clipped;
           poly.Clipped = poly.Clipped->f)
        dapi->draw_pcb_polygon (dapi, info->layer, &poly);
    }

  return 1;
}

static int
pin_mask_callback (const BoxType * b, void *cl)
{
  PinType *pin = (PinType *) b;
  struct side_info *info = cl;

  info->dapi->draw_pcb_pin_mask (info->dapi, pin);
  return 1;
}

static int
pad_mask_callback (const BoxType * b, void *cl)
{
  PadType *pad = (PadType *) b;
  struct side_info *info = cl;

  if (!ON_SIDE (pad, info->side))
    return 0;

  info->dapi->draw_pcb_pad_mask (info->dapi, NULL, pad);
  return 1;
}

/* ---------------------------------------------------------------------------
 * Draws silk layer.
 */

void
draw_silk_layer (DrawAPI *dapi, int side)
{
  struct side_info side_info;

  side_info.dapi = dapi;
  side_info.side = side;

#if 0
  /* This code is used when you want to mask silk to avoid exposed
   * pins and pads.  We decided it was a bad idea to do this
   * unconditionally, but the code remains.
   *
   * Note that many exporters (notably gerber, ps, eps), do not support the
   * masking API, so this code won't actually work.
   */

  if (gui->poly_before)
    {
      dapi->graphics->use_mask (HID_MASK_BEFORE);
#endif
      dapi->draw_pcb_layer (dapi, LAYER_PTR (max_copper_layer + side));
      /* draw package */
      r_search (PCB->Data->element_tree, dapi->clip_box, NULL, element_callback, &side_info);
      r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], dapi->clip_box, NULL, name_callback, &side_info);
#if 0
    }

  dapi->graphics->use_mask (HID_MASK_CLEAR);
  r_search (PCB->Data->pin_tree, drawn_area, NULL, pin_mask_callback, NULL);
  r_search (PCB->Data->via_tree, drawn_area, NULL, pin_mask_callback, NULL);
  r_search (PCB->Data->pad_tree, drawn_area, NULL, pad_mask_callback, &side);

  if (gui->poly_after)
    {
      dapi->graphics->use_mask (HID_MASK_AFTER);
      DrawLayer (dapi, LAYER_PTR (max_copper_layer + layer), drawn_area);
      /* draw package */
      r_search (PCB->Data->element_tree, drawn_area, NULL, element_callback, &side);
      r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], drawn_area, NULL, name_callback, &side_info);
    }
  dapi->graphics->use_mask (HID_MASK_OFF);
#endif
}


static void
DrawMaskBoardArea (DrawAPI *dapi, int mask_type)
{
  /* Skip the mask drawing if the GUI doesn't want this type */
  if ((mask_type == HID_MASK_BEFORE && !gui->poly_before) ||
      (mask_type == HID_MASK_AFTER  && !gui->poly_after))
    return;

  dapi->graphics->use_mask (mask_type);
  dapi->graphics->set_color (dapi->fg_gc, PCB->MaskColor);
  if (dapi->clip_box == NULL)
    dapi->graphics->fill_rect (dapi->fg_gc, 0, 0, PCB->MaxWidth, PCB->MaxHeight);
  else
    dapi->graphics->fill_rect (dapi->fg_gc, dapi->clip_box->X1, dapi->clip_box->Y1,
                                            dapi->clip_box->X2, dapi->clip_box->Y2);
}

/* ---------------------------------------------------------------------------
 * draws solder mask layer - this will cover nearly everything
 */
static void
draw_mask_layer (DrawAPI *dapi, int side)
{
  int thin = TEST_FLAG(THINDRAWFLAG, PCB) || TEST_FLAG(THINDRAWPOLYFLAG, PCB);
  struct side_info info;

  info.dapi = dapi;
  info.side = side;

  if (thin)
    dapi->graphics->set_color (dapi->fg_gc, PCB->MaskColor);
  else
    {
      DrawMaskBoardArea (dapi, HID_MASK_BEFORE);
      dapi->graphics->use_mask (HID_MASK_CLEAR);
      dapi->graphics->set_color (dapi->fg_gc, "erase"); /* <-- XXX: This might not be needed */
    }

  r_search (PCB->Data->pin_tree, dapi->clip_box, NULL, pin_mask_callback, &info);
  r_search (PCB->Data->via_tree, dapi->clip_box, NULL, pin_mask_callback, &info);
  r_search (PCB->Data->pad_tree, dapi->clip_box, NULL, pad_mask_callback, &info);

  if (!thin)
    {
      DrawMaskBoardArea (dapi, HID_MASK_AFTER);
      dapi->graphics->use_mask (HID_MASK_OFF);
    }
}

/* ---------------------------------------------------------------------------
 * draws solder paste layer for a given side of the board
 */
static void
draw_paste_layer (DrawAPI *dapi, int side)
{
  dapi->graphics->set_color (dapi->fg_gc, PCB->ElementColor); /* XXX: DO WE NEED THIS? */
  ALLPAD_LOOP (PCB->Data);
  {
    if (ON_SIDE (pad, side))
      dapi->draw_pcb_pad_paste (dapi, NULL, pad);
  }
  ENDALL_LOOP;
}

static void
DrawRats (DrawAPI *dapi)
{
  /*
   * XXX lesstif allows positive AND negative drawing in HID_MASK_CLEAR.
   * XXX gtk only allows negative drawing.
   * XXX using the mask here is to get rat transparency
   */
  int can_mask = strcmp(gui->name, "lesstif") == 0;

  if (can_mask)
    dapi->graphics->use_mask (HID_MASK_CLEAR);
  r_search (PCB->Data->rat_tree, dapi->clip_box, NULL, rat_callback, dapi);
  if (can_mask)
    dapi->graphics->use_mask (HID_MASK_OFF);
}

static int
text_callback (const BoxType * b, void *cl)
{
  struct layer_info *info = cl;
  DrawAPI *dapi = info->dapi;
  TextType *text = (TextType *)b;
  int min_silk_line;

  if (TEST_FLAG (SELECTEDFLAG, text))
    dapi->graphics->set_color (dapi->fg_gc, info->layer->SelectedColor);
  else
    dapi->graphics->set_color (dapi->fg_gc, info->layer->Color);

  if (info->layer == &PCB->Data->SILKLAYER ||
      info->layer == &PCB->Data->BACKSILKLAYER)
    min_silk_line = PCB->minSlk;
  else
    min_silk_line = PCB->minWid;

  dapi->draw_pcb_text (dapi, info->layer, text, min_silk_line);
  return 1;
}

static void
draw_pcb_layer (DrawAPI *dapi, LayerType *layer)
{
  struct layer_info info;

  info.dapi = dapi;
  info.layer = layer;

  r_search (layer->polygon_tree, dapi->clip_box, NULL, poly_callback, &info);

  if (TEST_FLAG (CHECKPLANESFLAG, PCB))
    return;

  r_search (layer->line_tree,    dapi->clip_box, NULL, line_callback, &info);
  r_search (layer->arc_tree,     dapi->clip_box, NULL, arc_callback,  &info);
  r_search (layer->text_tree,    dapi->clip_box, NULL, text_callback, &info);

  /* We should check for gui->gui here, but it's kinda cool seeing the
     auto-outline magically disappear when you first add something to
     the "outline" layer.  */
  if (IsLayerEmpty (layer) && (strcmp (layer->Name, "outline") == 0 ||
                               strcmp (layer->Name, "route")   == 0))
    {
      dapi->graphics->set_color (dapi->fg_gc, layer->Color);
      dapi->graphics->set_line_width (dapi->fg_gc, PCB->minWid);
      dapi->graphics->draw_rect (dapi->fg_gc, 0, 0, PCB->MaxWidth, PCB->MaxHeight);
    }
}

/* ---------------------------------------------------------------------------
 * draws one layer group.  If the exporter is not a GUI,
 * also draws the pins / pads / vias in this layer group.
 */
static void
draw_pcb_layer_group (DrawAPI *dapi, int group)
{
  int i;
  int layernum;
  bool draw_ppv = false;
  LayerType *layer;
  int n_entries = PCB->LayerGroups.Number[group];
  Cardinal *layers = PCB->LayerGroups.Entries[group];

  for (i = n_entries - 1; i >= 0; i--)
    {
      layernum = layers[i];
      layer = PCB->Data->Layer + layers[i];
      if (strcmp (layer->Name, "outline") != 0 &&
          strcmp (layer->Name, "route")   != 0)
        draw_ppv = !gui->gui;
      if (layernum < max_copper_layer && layer->On)
        dapi->draw_pcb_layer (dapi, layer);
    }

  if (draw_ppv)
    DrawPPV (dapi, group);
}

static void
GatherPVName (PinType *Ptr)
{
  BoxType box;
  bool vert = TEST_FLAG (EDGE2FLAG, Ptr);

  if (vert)
    {
      box.X1 = Ptr->X - Ptr->Thickness / 2 + Settings.PinoutTextOffsetY;
      box.Y1 = Ptr->Y - Ptr->DrillingHole / 2 - Settings.PinoutTextOffsetX;
    }
  else
    {
      box.X1 = Ptr->X + Ptr->DrillingHole / 2 + Settings.PinoutTextOffsetX;
      box.Y1 = Ptr->Y - Ptr->Thickness / 2 + Settings.PinoutTextOffsetY;
    }

  if (vert)
    {
      box.X2 = box.X1;
      box.Y2 = box.Y1;
    }
  else
    {
      box.X2 = box.X1;
      box.Y2 = box.Y1;
    }
  AddPart (&box);
}

static void
GatherPadName (PadType *Pad)
{
  BoxType box;
  bool vert;

  /* should text be vertical ? */
  vert = (Pad->Point1.X == Pad->Point2.X);

  if (vert)
    {
      box.X1 = Pad->Point1.X                      - Pad->Thickness / 2;
      box.Y1 = MAX (Pad->Point1.Y, Pad->Point2.Y) + Pad->Thickness / 2;
      box.X1 += Settings.PinoutTextOffsetY;
      box.Y1 -= Settings.PinoutTextOffsetX;
      box.X2 = box.X1;
      box.Y2 = box.Y1;
    }
  else
    {
      box.X1 = MIN (Pad->Point1.X, Pad->Point2.X) - Pad->Thickness / 2;
      box.Y1 = Pad->Point1.Y                      - Pad->Thickness / 2;
      box.X1 += Settings.PinoutTextOffsetX;
      box.Y1 += Settings.PinoutTextOffsetY;
      box.X2 = box.X1;
      box.Y2 = box.Y1;
    }

  AddPart (&box);
  return;
}

/* ---------------------------------------------------------------------------
 * lowlevel drawing routine for text objects
 */
void
draw_pcb_text (DrawAPI *dapi, LayerType *layer, TextType *Text, Coord min_line_width)
{
  Coord x = 0;
  unsigned char *string = (unsigned char *) Text->TextString;
  Cardinal n;
  FontType *font = &PCB->Font;

  while (string && *string)
    {
      /* draw lines if symbol is valid and data is present */
      if (*string <= MAX_FONTPOSITION && font->Symbol[*string].Valid)
	{
	  LineType *line = font->Symbol[*string].Line;
	  LineType newline;

	  for (n = font->Symbol[*string].LineN; n; n--, line++)
	    {
	      /* create one line, scale, move, rotate and swap it */
	      newline = *line;
	      newline.Point1.X = SCALE_TEXT (newline.Point1.X + x, Text->Scale);
	      newline.Point1.Y = SCALE_TEXT (newline.Point1.Y, Text->Scale);
	      newline.Point2.X = SCALE_TEXT (newline.Point2.X + x, Text->Scale);
	      newline.Point2.Y = SCALE_TEXT (newline.Point2.Y, Text->Scale);
	      newline.Thickness = SCALE_TEXT (newline.Thickness, Text->Scale / 2);
	      if (newline.Thickness < min_line_width)
		newline.Thickness = min_line_width;

	      RotateLineLowLevel (&newline, 0, 0, Text->Direction);

	      /* the labels of SMD objects on the bottom
	       * side haven't been swapped yet, only their offset
	       */
	      if (TEST_FLAG (ONSOLDERFLAG, Text))
		{
		  newline.Point1.X = SWAP_SIGN_X (newline.Point1.X);
		  newline.Point1.Y = SWAP_SIGN_Y (newline.Point1.Y);
		  newline.Point2.X = SWAP_SIGN_X (newline.Point2.X);
		  newline.Point2.Y = SWAP_SIGN_Y (newline.Point2.Y);
		}
	      /* add offset and draw line */
	      newline.Point1.X += Text->X;
	      newline.Point1.Y += Text->Y;
	      newline.Point2.X += Text->X;
	      newline.Point2.Y += Text->Y;
	      _draw_line (dapi, &newline);
	    }

	  /* move on to next cursor position */
	  x += (font->Symbol[*string].Width + font->Symbol[*string].Delta);
	}
      else
	{
	  /* the default symbol is a filled box */
	  BoxType defaultsymbol = PCB->Font.DefaultSymbol;
	  Coord size = (defaultsymbol.X2 - defaultsymbol.X1) * 6 / 5;

	  defaultsymbol.X1 = SCALE_TEXT (defaultsymbol.X1 + x, Text->Scale);
	  defaultsymbol.Y1 = SCALE_TEXT (defaultsymbol.Y1, Text->Scale);
	  defaultsymbol.X2 = SCALE_TEXT (defaultsymbol.X2 + x, Text->Scale);
	  defaultsymbol.Y2 = SCALE_TEXT (defaultsymbol.Y2, Text->Scale);

	  RotateBoxLowLevel (&defaultsymbol, 0, 0, Text->Direction);

	  /* add offset and draw box */
	  defaultsymbol.X1 += Text->X;
	  defaultsymbol.Y1 += Text->Y;
	  defaultsymbol.X2 += Text->X;
	  defaultsymbol.Y2 += Text->Y;
	  dapi->graphics->fill_rect (dapi->fg_gc, defaultsymbol.X1, defaultsymbol.Y1,
	                                          defaultsymbol.X2, defaultsymbol.Y2);

	  /* move on to next cursor position */
	  x += size;
	}
      string++;
    }
}

/* ---------------------------------------------------------------------------
 * draw a via object
 */
void
DrawVia (PinType *Via)
{
  AddPart (Via);
  if (!TEST_FLAG (HOLEFLAG, Via) && TEST_FLAG (DISPLAYNAMEFLAG, Via))
    DrawViaName (Via);
}

/* ---------------------------------------------------------------------------
 * draws the name of a via
 */
void
DrawViaName (PinType *Via)
{
  GatherPVName (Via);
}

/* ---------------------------------------------------------------------------
 * draw a pin object
 */
void
DrawPin (PinType *Pin)
{
  AddPart (Pin);
  if ((!TEST_FLAG (HOLEFLAG, Pin) && TEST_FLAG (DISPLAYNAMEFLAG, Pin)))
    DrawPinName (Pin);
}

/* ---------------------------------------------------------------------------
 * draws the name of a pin
 */
void
DrawPinName (PinType *Pin)
{
  GatherPVName (Pin);
}

/* ---------------------------------------------------------------------------
 * draw a pad object
 */
void
DrawPad (PadType *Pad)
{
  AddPart (Pad);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Pad))
    DrawPadName (Pad);
}

/* ---------------------------------------------------------------------------
 * draws the name of a pad
 */
void
DrawPadName (PadType *Pad)
{
  GatherPadName (Pad);
}

/* ---------------------------------------------------------------------------
 * draws a line on a layer
 */
void
DrawLine (LayerType *Layer, LineType *Line)
{
  AddPart (Line);
}

/* ---------------------------------------------------------------------------
 * draws a ratline
 */
void
DrawRat (RatType *Rat)
{
  if (Settings.RatThickness < 100)
    Rat->Thickness = pixel_slop * Settings.RatThickness;
  /* rats.c set VIAFLAG if this rat goes to a containing poly: draw a donut */
  if (TEST_FLAG(VIAFLAG, Rat))
    {
      Coord w = Rat->Thickness;

      BoxType b;

      b.X1 = Rat->Point1.X - w * 2 - w / 2;
      b.X2 = Rat->Point1.X + w * 2 + w / 2;
      b.Y1 = Rat->Point1.Y - w * 2 - w / 2;
      b.Y2 = Rat->Point1.Y + w * 2 + w / 2;
      AddPart (&b);
    }
  else
    DrawLine (NULL, (LineType *)Rat);
}

/* ---------------------------------------------------------------------------
 * draws an arc on a layer
 */
void
DrawArc (LayerType *Layer, ArcType *Arc)
{
  AddPart (Arc);
}

/* ---------------------------------------------------------------------------
 * draws a text on a layer
 */
void
DrawText (LayerType *Layer, TextType *Text)
{
  AddPart (Text);
}


/* ---------------------------------------------------------------------------
 * draws a polygon on a layer
 */
void
DrawPolygon (LayerType *Layer, PolygonType *Polygon)
{
  AddPart (Polygon);
}

/* ---------------------------------------------------------------------------
 * draws an element
 */
void
DrawElement (ElementType *Element)
{
  DrawElementPackage (Element);
  DrawElementName (Element);
  DrawElementPinsAndPads (Element);
}

/* ---------------------------------------------------------------------------
 * draws the name of an element
 */
void
DrawElementName (ElementType *Element)
{
  if (TEST_FLAG (HIDENAMEFLAG, Element))
    return;
  DrawText (NULL, &ELEMENT_TEXT (PCB, Element));
}

/* ---------------------------------------------------------------------------
 * draws the package of an element
 */
void
DrawElementPackage (ElementType *Element)
{
  ELEMENTLINE_LOOP (Element);
  {
    DrawLine (NULL, line);
  }
  END_LOOP;
  ARC_LOOP (Element);
  {
    DrawArc (NULL, arc);
  }
  END_LOOP;
}

/* ---------------------------------------------------------------------------
 * draw pins of an element
 */
void
DrawElementPinsAndPads (ElementType *Element)
{
  PAD_LOOP (Element);
  {
    if (FRONT (pad) || PCB->InvisibleObjectsOn)
      DrawPad (pad);
  }
  END_LOOP;
  PIN_LOOP (Element);
  {
    DrawPin (pin);
  }
  END_LOOP;
}

/* ---------------------------------------------------------------------------
 * erase a via
 */
void
EraseVia (PinType *Via)
{
  AddPart (Via);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Via))
    EraseViaName (Via);
}

/* ---------------------------------------------------------------------------
 * erase a ratline
 */
void
EraseRat (RatType *Rat)
{
  if (TEST_FLAG(VIAFLAG, Rat))
    {
      Coord w = Rat->Thickness;

      BoxType b;

      b.X1 = Rat->Point1.X - w * 2 - w / 2;
      b.X2 = Rat->Point1.X + w * 2 + w / 2;
      b.Y1 = Rat->Point1.Y - w * 2 - w / 2;
      b.Y2 = Rat->Point1.Y + w * 2 + w / 2;
      AddPart (&b);
    }
  else
    EraseLine ((LineType *)Rat);
}


/* ---------------------------------------------------------------------------
 * erase a via name
 */
void
EraseViaName (PinType *Via)
{
  GatherPVName (Via);
}

/* ---------------------------------------------------------------------------
 * erase a pad object
 */
void
ErasePad (PadType *Pad)
{
  AddPart (Pad);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Pad))
    ErasePadName (Pad);
}

/* ---------------------------------------------------------------------------
 * erase a pad name
 */
void
ErasePadName (PadType *Pad)
{
  GatherPadName (Pad);
}

/* ---------------------------------------------------------------------------
 * erase a pin object
 */
void
ErasePin (PinType *Pin)
{
  AddPart (Pin);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Pin))
    ErasePinName (Pin);
}

/* ---------------------------------------------------------------------------
 * erase a pin name
 */
void
ErasePinName (PinType *Pin)
{
  GatherPVName (Pin);
}

/* ---------------------------------------------------------------------------
 * erases a line on a layer
 */
void
EraseLine (LineType *Line)
{
  AddPart (Line);
}

/* ---------------------------------------------------------------------------
 * erases an arc on a layer
 */
void
EraseArc (ArcType *Arc)
{
  if (!Arc->Thickness)
    return;
  AddPart (Arc);
}

/* ---------------------------------------------------------------------------
 * erases a text on a layer
 */
void
EraseText (LayerType *Layer, TextType *Text)
{
  AddPart (Text);
}

/* ---------------------------------------------------------------------------
 * erases a polygon on a layer
 */
void
ErasePolygon (PolygonType *Polygon)
{
  AddPart (Polygon);
}

/* ---------------------------------------------------------------------------
 * erases an element
 */
void
EraseElement (ElementType *Element)
{
  ELEMENTLINE_LOOP (Element);
  {
    EraseLine (line);
  }
  END_LOOP;
  ARC_LOOP (Element);
  {
    EraseArc (arc);
  }
  END_LOOP;
  EraseElementName (Element);
  EraseElementPinsAndPads (Element);
}

/* ---------------------------------------------------------------------------
 * erases all pins and pads of an element
 */
void
EraseElementPinsAndPads (ElementType *Element)
{
  PIN_LOOP (Element);
  {
    ErasePin (pin);
  }
  END_LOOP;
  PAD_LOOP (Element);
  {
    ErasePad (pad);
  }
  END_LOOP;
}

/* ---------------------------------------------------------------------------
 * erases the name of an element
 */
void
EraseElementName (ElementType *Element)
{
  if (TEST_FLAG (HIDENAMEFLAG, Element))
    return;
  DrawText (NULL, &ELEMENT_TEXT (PCB, Element));
}


void
EraseObject (int type, void *lptr, void *ptr)
{
  switch (type)
    {
    case VIA_TYPE:
    case PIN_TYPE:
      ErasePin ((PinType *) ptr);
      break;
    case TEXT_TYPE:
    case ELEMENTNAME_TYPE:
      EraseText ((LayerType *)lptr, (TextType *) ptr);
      break;
    case POLYGON_TYPE:
      ErasePolygon ((PolygonType *) ptr);
      break;
    case ELEMENT_TYPE:
      EraseElement ((ElementType *) ptr);
      break;
    case LINE_TYPE:
    case ELEMENTLINE_TYPE:
    case RATLINE_TYPE:
      EraseLine ((LineType *) ptr);
      break;
    case PAD_TYPE:
      ErasePad ((PadType *) ptr);
      break;
    case ARC_TYPE:
    case ELEMENTARC_TYPE:
      EraseArc ((ArcType *) ptr);
      break;
    default:
      Message ("hace: Internal ERROR, trying to erase an unknown type\n");
    }
}



void
DrawObject (int type, void *ptr1, void *ptr2)
{
  switch (type)
    {
    case VIA_TYPE:
      if (PCB->ViaOn)
	DrawVia ((PinType *) ptr2);
      break;
    case LINE_TYPE:
      if (((LayerType *) ptr1)->On)
	DrawLine ((LayerType *) ptr1, (LineType *) ptr2);
      break;
    case ARC_TYPE:
      if (((LayerType *) ptr1)->On)
	DrawArc ((LayerType *) ptr1, (ArcType *) ptr2);
      break;
    case TEXT_TYPE:
      if (((LayerType *) ptr1)->On)
	DrawText ((LayerType *) ptr1, (TextType *) ptr2);
      break;
    case POLYGON_TYPE:
      if (((LayerType *) ptr1)->On)
	DrawPolygon ((LayerType *) ptr1, (PolygonType *) ptr2);
      break;
    case ELEMENT_TYPE:
      if (PCB->ElementOn &&
	  (FRONT ((ElementType *) ptr2) || PCB->InvisibleObjectsOn))
	DrawElement ((ElementType *) ptr2);
      break;
    case RATLINE_TYPE:
      if (PCB->RatOn)
	DrawRat ((RatType *) ptr2);
      break;
    case PIN_TYPE:
      if (PCB->PinOn)
	DrawPin ((PinType *) ptr2);
      break;
    case PAD_TYPE:
      if (PCB->PinOn)
	DrawPad ((PadType *) ptr2);
      break;
    case ELEMENTNAME_TYPE:
      if (PCB->ElementOn &&
	  (FRONT ((ElementType *) ptr2) || PCB->InvisibleObjectsOn))
	DrawElementName ((ElementType *) ptr1);
      break;
    }
}

static void
draw_pcb_element (DrawAPI *dapi, ElementType *element)
{
  draw_element_package (dapi, element);
  draw_element_name (dapi, element);

  PAD_LOOP (element);
  {
    draw_pad (dapi, pad);
  }
  END_LOOP;
  PIN_LOOP (element);
  {
    dapi->set_color_for_pin (dapi, pin);
    dapi->draw_pcb_pin (dapi, pin);

    if (!TEST_FLAG (HOLEFLAG, pin) && TEST_FLAG (DISPLAYNAMEFLAG, pin))
      _draw_pv_name (dapi, pin);
    dapi->draw_pcb_pin_hole (dapi, pin);
  }
  END_LOOP;
}

/* ---------------------------------------------------------------------------
 * HID drawing callback.
 */

static void
draw_everything (DrawAPI *dapi)
{
  dapi->fg_gc = dapi->graphics->make_gc ();
  dapi->bg_gc = dapi->graphics->make_gc ();

  dapi->graphics->set_color (dapi->bg_gc, "drill");

  DrawEverything (dapi);

  dapi->graphics->destroy_gc (dapi->fg_gc);
  dapi->graphics->destroy_gc (dapi->bg_gc);
}

static void
draw_pinout_preview (DrawAPI *dapi, ElementType *element)
{
  dapi->fg_gc = dapi->graphics->make_gc ();
  dapi->bg_gc = dapi->graphics->make_gc ();

  dapi->graphics->set_color (dapi->bg_gc, "drill");

  g_warn_if_fail (dapi->doing_pinout == false);
  dapi->doing_pinout = true;
  dapi->draw_pcb_element (dapi, element);
  dapi->doing_pinout = false;

  dapi->graphics->destroy_gc (dapi->fg_gc);
  dapi->graphics->destroy_gc (dapi->bg_gc);
}

static void
set_clip_box (DrawAPI *dapi, const BoxType *clip_box)
{
  dapi->clip_box = clip_box;
}

DrawAPI *
draw_api_new (void)
{
  DrawAPI *dapi;

  dapi = g_new0 (DrawAPI, 1);

#if 0
  dapi->draw_pcb_pin          =
  dapi->draw_pcb_pin_mask     =
  dapi->draw_pcb_pin_hole     =
  dapi->draw_pcb_via          =
  dapi->draw_pcb_via_mask     =
  dapi->draw_pcb_via_hole     =
  dapi->draw_pcb_pad          =
  dapi->draw_pcb_pad_mask     =
  dapi->draw_pcb_pad_paste    =
  dapi->draw_pcb_line         =
  dapi->draw_pcb_arc          =
#endif
  dapi->draw_pcb_text         = draw_pcb_text;
#if 0
  dapi->draw_pcb_polygon      =
#endif
  dapi->set_color_for_pin     = set_color_for_pin;
  dapi->set_color_for_via     = set_color_for_via;
  dapi->set_color_for_pad     = set_color_for_pad;

  /* The following types can be on copper layer, silk layers, selected, found,
   * warned, they can be drawn as overlay text. We sould consisder whether
   * any of these need to be split up into multiple hooks.
   */
  dapi->set_color_for_line    = NULL; // set_color_for_line;
  dapi->set_color_for_arc     = NULL; // set_color_for_arc;
  dapi->set_color_for_text    = NULL; // set_color_for_text;
  dapi->set_color_for_polygon = NULL; // set_color_for_polygon;
#if 0
  dapi->draw_rat              =
#endif
  dapi->draw_pcb_element      = draw_pcb_element;
  dapi->draw_pcb_layer        = draw_pcb_layer;
  dapi->draw_pcb_layer_group  = draw_pcb_layer_group;
  dapi->draw_mask_layer       = draw_mask_layer;
  dapi->draw_paste_layer      = draw_paste_layer;
  dapi->draw_silk_layer       = draw_silk_layer; /* Hmm - should be able to do this by layer number, even if not layer group */
  /* But it would mean special casing diving into element silk from teh draw_pcb_layer function */
  dapi->draw_everything       = draw_everything;
  dapi->draw_pinout_preview   = draw_pinout_preview;
#if 0
  dapi->draw_pcb_buffer       =
  dapi->set_draw_offset       =
#endif
  dapi->set_clip_box          = set_clip_box;

  return dapi;
}
