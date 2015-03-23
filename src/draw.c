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
#include "hid_draw.h"

/*#include "clip.h"*/
#include "compat.h"
#include "crosshair.h"
#include "data.h"
#include "draw.h"
#include "draw_funcs.h"
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

void ghid_set_lock_effects (hidGC gc, AnyObjectType *object);

/* ---------------------------------------------------------------------------
 * some local identifiers
 */
static BoxType Block = {MAXINT, MAXINT, -MAXINT, -MAXINT};

static int doing_pinout = 0;
static bool doing_assy = false;

/* ---------------------------------------------------------------------------
 * some local prototypes
 */
static void DrawEverything (hidGC gc);
static void AddPart (void *);
/* static */ void DrawEMark (hidGC gc, ElementType *, Coord, Coord, bool);
/* static */ void DrawRats (hidGC gc);

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

struct side_info {
  hidGC gc;
  int side;
};

static int
pad_callback (const BoxType * b, void *cl)
{
  PadType *pad = (PadType *) b;
  struct side_info *info = cl;

  if (ON_SIDE (pad, info->side))
    {
      set_object_color (info->gc, (AnyObjectType *)pad, PCB->WarnColor,
                        PCB->PinSelectedColor, PCB->ConnectedColor, PCB->FoundColor,
                        FRONT (pad) ? PCB->PinColor : PCB->InvisibleObjectsColor);

      dapi->draw_pad (info->gc, pad, NULL);
    }
  return 1;
}

static void
draw_element_name (hidGC gc, ElementType *element)
{
  HID_DRAW *hid_draw = gc->hid_draw;

  if ((TEST_FLAG (HIDENAMESFLAG, PCB) && hid_draw_is_gui (hid_draw)) ||
      TEST_FLAG (HIDENAMEFLAG, element))
    return;
  ghid_set_lock_effects (gc, (AnyObjectType *)element);
  if (doing_pinout || doing_assy)
    hid_draw_set_color (gc, PCB->ElementColor);
  else if (TEST_FLAG (SELECTEDFLAG, &ELEMENT_TEXT (PCB, element)))
    hid_draw_set_color (gc, PCB->ElementSelectedColor);
  else if (FRONT (element))
    hid_draw_set_color (gc, PCB->ElementColor);
  else
    hid_draw_set_color (gc, PCB->InvisibleObjectsColor);
  hid_draw_pcb_text (gc, &ELEMENT_TEXT (PCB, element), PCB->minSlk);
}

static int
name_callback (const BoxType * b, void *cl)
{
  TextType *text = (TextType *) b;
  ElementType *element = (ElementType *) text->Element;
  struct side_info *info = cl;

  if (TEST_FLAG (HIDENAMEFLAG, element))
    return 0;

  if (ON_SIDE (element, info->side))
    draw_element_name (info->gc, element);
  return 0;
}

static void
draw_element_pins_and_pads (hidGC gc, ElementType *element)
{
  PAD_LOOP (element);
  {
    if (doing_pinout || doing_assy || FRONT (pad) || PCB->InvisibleObjectsOn)
      {
        set_object_color (gc, (AnyObjectType *)pad, PCB->WarnColor,
                          PCB->PinSelectedColor, PCB->ConnectedColor, PCB->FoundColor,
                          FRONT (pad) ? PCB->PinColor : PCB->InvisibleObjectsColor);

        dapi->draw_pad (gc, pad, NULL);
      }
  }
  END_LOOP;
  PIN_LOOP (element);
  {
    set_object_color (gc, (AnyObjectType *)pin, PCB->WarnColor, PCB->PinSelectedColor,
                      PCB->ConnectedColor, PCB->FoundColor, PCB->PinColor);

    dapi->draw_pin (gc, pin, NULL);

    set_object_color (gc, (AnyObjectType *)pin, PCB->WarnColor,
                      PCB->PinSelectedColor, NULL, NULL, Settings.BlackColor);

    dapi->draw_pin_hole (gc, pin, NULL);
  }
  END_LOOP;
}

static int
EMark_callback (const BoxType * b, void *cl)
{
  ElementType *element = (ElementType *) b;
  hidGC gc = cl;

  DrawEMark (gc, element, element->MarkX, element->MarkY, !FRONT (element));
  return 1;
}

static int
rat_callback (const BoxType * b, void *cl)
{
  RatType *rat = (RatType *)b;
  hidGC gc = cl;

  set_object_color (gc, (AnyObjectType *) rat, NULL, PCB->RatSelectedColor,
                    PCB->ConnectedColor, PCB->FoundColor, PCB->RatColor);

  dapi->draw_rat (gc, rat, NULL);
  return 1;
}

static void
draw_element_package (hidGC gc, ElementType *element)
{
  ghid_set_lock_effects (gc, (AnyObjectType *)element);
  /* set color and draw lines, arcs, text and pins */
  if (doing_pinout || doing_assy)
    hid_draw_set_color (gc, PCB->ElementColor);
  else if (TEST_FLAG (SELECTEDFLAG, element))
    hid_draw_set_color (gc, PCB->ElementSelectedColor);
  else if (FRONT (element))
    hid_draw_set_color (gc, PCB->ElementColor);
  else
    hid_draw_set_color (gc, PCB->InvisibleObjectsColor);

  /* draw lines, arcs, text and pins */
  ELEMENTLINE_LOOP (element);
  {
    //hid_draw_pcb_line (gc, line);
    dapi->draw_line (gc, line, NULL);
  }
  END_LOOP;
  ARC_LOOP (element);
  {
    //hid_draw_pcb_arc (gc, arc);
    dapi->draw_arc (gc, arc, NULL);
  }
  END_LOOP;
}

static int
element_callback (const BoxType * b, void *cl)
{
  ElementType *element = (ElementType *) b;
  struct side_info *info = cl;

  if (ON_SIDE (element, info->side))
    draw_element_package (info->gc, element);
  return 1;
}

/* ---------------------------------------------------------------------------
 * prints assembly drawing.
 */

void
PrintAssembly (hidGC gc, int side)
{
  int side_group = GetLayerGroupNumberBySide (side);

  doing_assy = true;
  hid_draw_set_draw_faded (gc, 1);
  DrawLayerGroup (gc, side_group);
  hid_draw_set_draw_faded (gc, 0);

  /* draw package */
  DrawSilk (gc, side);
  doing_assy = false;
}

/* ---------------------------------------------------------------------------
 * initializes some identifiers for a new zoom factor and redraws whole screen
 */
static void
DrawEverything (hidGC gc)
{
  int i, ngroups;
  int top_group, bottom_group;
  /* This is the list of layer groups we will draw.  */
  int do_group[MAX_GROUP];
  /* This is the reverse of the order in which we draw them.  */
  int drawn_groups[MAX_GROUP];
  int plated, unplated;
  bool paste_empty;
  struct side_info info;
  HID_DRAW *hid_draw = gc->hid_draw;

  info.gc = gc;

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

  top_group = GetLayerGroupNumberBySide (TOP_SIDE);
  bottom_group = GetLayerGroupNumberBySide (BOTTOM_SIDE);

  /*
   * first draw all 'invisible' stuff
   */
  if (!TEST_FLAG (CHECKPLANESFLAG, PCB)
      && hid_draw_set_layer (hid_draw, "invisible", SL (INVISIBLE, 0), 0))
    {
      info.side = SWAP_IDENT ? TOP_SIDE : BOTTOM_SIDE;
      if (PCB->ElementOn)
	{
	  r_search (PCB->Data->element_tree, hid_draw->clip_box, NULL, element_callback, &info);
	  r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], hid_draw->clip_box, NULL, name_callback, &info);
	  dapi->draw_layer (gc, &(PCB->Data->Layer[max_copper_layer + info.side]), NULL);
	}
      r_search (PCB->Data->pad_tree, hid_draw->clip_box, NULL, pad_callback, &info);
      hid_draw_end_layer (hid_draw);
    }

  /* draw all layers in layerstack order */
  for (i = ngroups - 1; i >= 0; i--)
    {
      int group = drawn_groups[i];

      if (hid_draw_set_layer (hid_draw, 0, group, 0))
        {
          DrawLayerGroup (gc, group);
          hid_draw_end_layer (hid_draw);
        }
    }

  if (TEST_FLAG (CHECKPLANESFLAG, PCB) && hid_draw_is_gui (hid_draw))
    return;

  /* Draw pins, pads, vias below silk */
  if (hid_draw_is_gui (hid_draw))
    dapi->draw_ppv (gc, SWAP_IDENT ? bottom_group : top_group, NULL);
  else
    {
      CountHoles (&plated, &unplated, hid_draw->clip_box);

      if (plated && hid_draw_set_layer (hid_draw, "plated-drill", SL (PDRILL, 0), 0))
        {
          dapi->draw_holes (gc, 1, NULL);
          hid_draw_end_layer (hid_draw);
        }

      if (unplated && hid_draw_set_layer (hid_draw, "unplated-drill", SL (UDRILL, 0), 0))
        {
          dapi->draw_holes (gc, 0, NULL);
          hid_draw_end_layer (hid_draw);
        }
    }

  /* Draw the solder mask if turned on */
  if (hid_draw_set_layer (hid_draw, "componentmask", SL (MASK, TOP), 0))
    {
      DrawMask (gc, TOP_SIDE);
      hid_draw_end_layer (hid_draw);
    }

  if (hid_draw_set_layer (hid_draw, "soldermask", SL (MASK, BOTTOM), 0))
    {
      DrawMask (gc, BOTTOM_SIDE);
      hid_draw_end_layer (hid_draw);
    }

  if (hid_draw_set_layer (hid_draw, "topsilk", SL (SILK, TOP), 0))
    {
      DrawSilk (gc, TOP_SIDE);
      hid_draw_end_layer (hid_draw);
    }

  if (hid_draw_set_layer (hid_draw, "bottomsilk", SL (SILK, BOTTOM), 0))
    {
      DrawSilk (gc, BOTTOM_SIDE);
      hid_draw_end_layer (hid_draw);
    }

  if (hid_draw_is_gui (hid_draw))
    {
      /* Draw element Marks */
      if (PCB->PinOn)
	r_search (PCB->Data->element_tree, hid_draw->clip_box, NULL, EMark_callback, gc);
      /* Draw rat lines on top */
      if (hid_draw_set_layer (hid_draw, "rats", SL (RATS, 0), 0))
        {
          DrawRats (gc);
          hid_draw_end_layer (hid_draw);
        }
    }

  paste_empty = IsPasteEmpty (TOP_SIDE);
  if (hid_draw_set_layer (hid_draw, "toppaste", SL (PASTE, TOP), paste_empty))
    {
      DrawPaste (gc, TOP_SIDE);
      hid_draw_end_layer (hid_draw);
    }

  paste_empty = IsPasteEmpty (BOTTOM_SIDE);
  if (hid_draw_set_layer (hid_draw, "bottompaste", SL (PASTE, BOTTOM), paste_empty))
    {
      DrawPaste (gc, BOTTOM_SIDE);
      hid_draw_end_layer (hid_draw);
    }

  if (hid_draw_set_layer (hid_draw, "topassembly", SL (ASSY, TOP), 0))
    {
      PrintAssembly (gc, TOP_SIDE);
      hid_draw_end_layer (hid_draw);
    }

  if (hid_draw_set_layer (hid_draw, "bottomassembly", SL (ASSY, BOTTOM), 0))
    {
      PrintAssembly (gc, BOTTOM_SIDE);
      hid_draw_end_layer (hid_draw);
    }

  if (hid_draw_set_layer (hid_draw, "fab", SL (FAB, 0), 0))
    {
      PrintFab (gc);
      hid_draw_end_layer (hid_draw);
    }
}

/* static */ void
DrawEMark (hidGC gc, ElementType *e, Coord X, Coord Y, bool invisible)
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

  ghid_set_lock_effects (gc, (AnyObjectType *)e);
  hid_draw_set_color (gc, invisible ? PCB->InvisibleMarkColor : PCB->ElementColor);
  hid_draw_set_line_cap (gc, Trace_Cap);
  hid_draw_set_line_width (gc, 0);
  hid_draw_line (gc, X - mark_size, Y, X, Y - mark_size);
  hid_draw_line (gc, X + mark_size, Y, X, Y - mark_size);
  hid_draw_line (gc, X - mark_size, Y, X, Y + mark_size);
  hid_draw_line (gc, X + mark_size, Y, X, Y + mark_size);

  /*
   * If an element is locked, place a "L" on top of the "diamond".
   * This provides a nice visual indication that it is locked that
   * works even for color blind users.
   */
  if (TEST_FLAG (LOCKFLAG, e) )
    {
      hid_draw_line (gc, X, Y, X + 2 * mark_size, Y);
      hid_draw_line (gc, X, Y, X, Y - 4* mark_size);
    }
}

static int
pin_mask_callback (const BoxType * b, void *cl)
{
  hidGC gc = cl;

  dapi->draw_pin_mask (gc, (PinType *) b, NULL);
  return 1;
}

static int
via_mask_callback (const BoxType * b, void *cl)
{
  hidGC gc = cl;

  dapi->draw_via_mask (gc, (PinType *) b, NULL);
  return 1;
}

static int
pad_mask_callback (const BoxType * b, void *cl)
{
  PadType *pad = (PadType *) b;
  struct side_info *info = cl;

  if (ON_SIDE (pad, info->side))
    dapi->draw_pad_mask (info->gc, pad, NULL);
  return 1;
}

/* ---------------------------------------------------------------------------
 * Draws silk layer.
 */

void
DrawSilk (hidGC gc, int side)
{
  struct side_info info;
  HID_DRAW *hid_draw = gc->hid_draw;

  info.gc = gc;
  info.side = side;

#if 0
  /* This code is used when you want to mask silk to avoid exposed
     pins and pads.  We decided it was a bad idea to do this
     unconditionally, but the code remains.  */
#endif

#if 0
  if (hid_draw->poly_before)
    {
      hid_draw_use_mask (hid_draw, HID_MASK_BEFORE);
#endif
      dapi->draw_layer (gc, LAYER_PTR (max_copper_layer + side), NULL);
      /* draw package */
      r_search (PCB->Data->element_tree, hid_draw->clip_box, NULL, element_callback, &info);
      r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], hid_draw->clip_box, NULL, name_callback, &info);
#if 0
    }

  hid_draw_use_mask (hid_draw, HID_MASK_CLEAR);
  r_search (PCB->Data->pin_tree, hid_draw->clip_box, NULL, pin_mask_callback, gc);
  r_search (PCB->Data->via_tree, hid_draw->clip_box, NULL, via_mask_callback, gc);
  r_search (PCB->Data->pad_tree, hid_draw->clip_box, NULL, pad_mask_callback, &info);

  if (hid_draw->poly_after)
    {
      hid_draw_use_mask (gc, hid_draw, HID_MASK_AFTER);
      dapi->draw_layer (gc, LAYER_PTR (max_copper_layer + layer), NULL);
      /* draw package */
      r_search (PCB->Data->element_tree, hid_draw->clip_box, NULL, element_callback, &side);
      r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], hid_draw->clip_box, NULL, name_callback, &info);
    }
  hid_draw_use_mask (hid_draw, HID_MASK_OFF);
#endif
}


static void
DrawMaskBoardArea (hidGC gc, int mask_type)
{
  HID_DRAW *hid_draw = gc->hid_draw;

  /* Skip the mask drawing if the GUI doesn't want this type */
  if ((mask_type == HID_MASK_BEFORE && !hid_draw->poly_before) ||
      (mask_type == HID_MASK_AFTER  && !hid_draw->poly_after))
    return;

  hid_draw_use_mask (hid_draw, mask_type);
  hid_draw_set_color (gc, PCB->MaskColor);
  if (hid_draw->clip_box == NULL)
    hid_draw_fill_rect (gc, 0, 0, PCB->MaxWidth, PCB->MaxHeight);
  else
    hid_draw_fill_rect (gc, hid_draw->clip_box->X1, hid_draw->clip_box->Y1,
                            hid_draw->clip_box->X2, hid_draw->clip_box->Y2);
}

static int
mask_poly_callback (const BoxType * b, void *cl)
{
  PolygonType *polygon = (PolygonType *)b;
  hidGC gc = cl;

  hid_draw_pcb_polygon (Output.pmGC, polygon);
  return 1;
}

static int
mask_line_callback (const BoxType * b, void *cl)
{
  LineType *line = (LineType *)b;
  hidGC gc = cl;

  hid_draw_pcb_line (Output.pmGC, line);
  return 1;
}

static int
mask_arc_callback (const BoxType * b, void *cl)
{
  ArcType *arc = (ArcType *)b;
  hidGC gc = cl;

  hid_draw_pcb_arc (Output.pmGC, arc);
  return 1;
}

static int
mask_text_callback (const BoxType * b, void *cl)
{
  TextType *text = (TextType *)b;
  hidGC gc = cl;
  int min_silk_line;

  min_silk_line = PCB->minSlk;

  hid_draw_pcb_text (gc, text, min_silk_line);
  return 1;
}

/* ---------------------------------------------------------------------------
 * draws solder mask layer - this will cover nearly everything
 */
void
DrawMask (hidGC gc, int side)
{
  int thin = TEST_FLAG(THINDRAWFLAG, PCB) || TEST_FLAG(THINDRAWPOLYFLAG, PCB);
  LayerType *Layer = LAYER_PTR (side == TOP_SIDE ? top_soldermask_layer : bottom_soldermask_layer);
  struct side_info info;
  HID_DRAW *hid_draw = gc->hid_draw;

  info.gc = gc;
  info.side = side;

  if (thin)
    hid_draw_set_color (Output.pmGC, PCB->MaskColor);
  else
    {
      DrawMaskBoardArea (gc, HID_MASK_BEFORE);
      hid_draw_use_mask (hid_draw, HID_MASK_CLEAR);
    }

  r_search (Layer->polygon_tree, hid_draw->clip_box, NULL, mask_poly_callback, gc);
  r_search (Layer->line_tree,    hid_draw->clip_box, NULL, mask_line_callback, gc);
  r_search (Layer->arc_tree,     hid_draw->clip_box, NULL, mask_arc_callback,  gc);
  r_search (Layer->text_tree,    hid_draw->clip_box, NULL, mask_text_callback, gc);

  r_search (PCB->Data->pin_tree, hid_draw->clip_box, NULL, pin_mask_callback, gc);
  r_search (PCB->Data->via_tree, hid_draw->clip_box, NULL, via_mask_callback, gc);
  r_search (PCB->Data->pad_tree, hid_draw->clip_box, NULL, pad_mask_callback, &info);

  if (thin)
    hid_draw_set_color (Output.pmGC, "erase");
  else
    {
      DrawMaskBoardArea (gc, HID_MASK_AFTER);
      hid_draw_use_mask (hid_draw, HID_MASK_OFF);
    }
}

/* ---------------------------------------------------------------------------
 * draws solder paste layer for a given side of the board
 */
void
DrawPaste (hidGC gc, int side)
{
  hid_draw_set_color (gc, PCB->ElementColor);
  ALLPAD_LOOP (PCB->Data);
  {
    if (ON_SIDE (pad, side))
      dapi->draw_pad_paste (gc, pad, NULL);
  }
  ENDALL_LOOP;
}

/* static */ void
DrawRats (hidGC gc)
{
  HID_DRAW *hid_draw = gc->hid_draw;

  /*
   * XXX lesstif allows positive AND negative drawing in HID_MASK_CLEAR.
   * XXX gtk only allows negative drawing.
   * XXX using the mask here is to get rat transparency
   */

  if (hid_draw_can_draw_in_mask_clear (hid_draw))
    hid_draw_use_mask (hid_draw, HID_MASK_CLEAR);
  r_search (PCB->Data->rat_tree, hid_draw->clip_box, NULL, rat_callback, gc);
  if (hid_draw_can_draw_in_mask_clear (hid_draw))
    hid_draw_use_mask (hid_draw, HID_MASK_OFF);
}

/* ---------------------------------------------------------------------------
 * draws one layer group.  If the exporter is not a GUI,
 * also draws the pins / pads / vias in this layer group.
 */
void
DrawLayerGroup (hidGC gc, int group)
{
  int i, rv = 1;
  int layernum;
  LayerType *Layer;
  int n_entries = PCB->LayerGroups.Number[group];
  Cardinal *layers = PCB->LayerGroups.Entries[group];
  HID_DRAW *hid_draw = gc->hid_draw;

  for (i = n_entries - 1; i >= 0; i--)
    {
      layernum = layers[i];
      Layer = PCB->Data->Layer + layers[i];
      if (strcmp (Layer->Name, "outline") == 0 ||
          strcmp (Layer->Name, "route") == 0)
        rv = 0;
      if (layernum < max_copper_layer && Layer->On)
        dapi->draw_layer (gc, Layer, NULL);
    }
  if (n_entries > 1)
    rv = 1;

  if (rv && !hid_draw_is_gui (hid_draw))
    dapi->draw_ppv (gc, group, NULL);
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
  if ((!TEST_FLAG (HOLEFLAG, Pin) && TEST_FLAG (DISPLAYNAMEFLAG, Pin))
      || doing_pinout)
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
  if (doing_pinout || TEST_FLAG (DISPLAYNAMEFLAG, Pad))
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
    if (doing_pinout || doing_assy || FRONT (pad) || PCB->InvisibleObjectsOn)
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
draw_element (hidGC gc, ElementType *element)
{
  draw_element_package (gc, element);
  draw_element_name (gc, element);
  draw_element_pins_and_pads (gc, element);
}

/* ---------------------------------------------------------------------------
 * HID drawing callback.
 */

void
hid_expose_callback (HID_DRAW *hid_draw, void *item)
{
  hidGC gc;

  gc = hid_draw_make_gc (hid_draw);
  Output.bgGC = hid_draw_make_gc (hid_draw);
  Output.pmGC = hid_draw_make_gc (hid_draw);

  hid_draw_set_color (Output.pmGC, "erase");
  hid_draw_set_color (Output.bgGC, "drill");

  if (item)
    {
      doing_pinout = true;
      draw_element (gc, (ElementType *)item);
      doing_pinout = false;
    }
  else
    DrawEverything (gc);

  hid_draw_destroy_gc (gc);
  hid_draw_destroy_gc (Output.bgGC);
  hid_draw_destroy_gc (Output.pmGC);
  hid_draw = NULL;
}
