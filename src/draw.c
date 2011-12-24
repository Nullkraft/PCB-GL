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


/* ---------------------------------------------------------------------------
 * some local identifiers
 */
static BoxType Block = {MAXINT, MAXINT, -MAXINT, -MAXINT};

static int doing_pinout = 0;
static bool doing_assy = false;

/* ---------------------------------------------------------------------------
 * some local prototypes
 */
static void DrawEverything (const BoxType *);
static void AddPart (void *);
/* static */ void DrawEMark (ElementTypePtr, Coord, Coord, bool);
/* static */ void DrawRats (const BoxType *);


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
DrawStrippedText (ElementTypePtr Element, int min_width)
{
  TextType text;
  TextType *text_ptr;
  char *end_string;

  if (TEST_FLAG (NAMEONPCBFLAG, PCB) &&
      TEST_FLAG (STRIPHIERFLAG, Element))
    {
      text_ptr = &text;
      memcpy (text_ptr, &ELEMENT_TEXT (PCB, Element), sizeof (TextType));

      /* Strip hierarchy */
      end_string = strrchr (text.TextString, '/');
      if (end_string != NULL)
        text.TextString = end_string + 1;
    }
  else
    {
      text_ptr = &ELEMENT_TEXT (PCB, Element);
    }

  DrawTextLowLevel (text_ptr, min_width);
}

static void
draw_element_name (ElementType *element)
{
  if ((TEST_FLAG (HIDENAMESFLAG, PCB) && gui->gui) ||
      TEST_FLAG (HIDENAMEFLAG, element))
    return;
  if (doing_pinout || doing_assy)
    gui->set_color (Output.fgGC, PCB->ElementColor);
  else if (TEST_FLAG (SELECTEDFLAG, &ELEMENT_TEXT (PCB, element)))
    gui->set_color (Output.fgGC, PCB->ElementSelectedColor);
  else if (FRONT (element))
    gui->set_color (Output.fgGC, PCB->ElementColor);
  else
    gui->set_color (Output.fgGC, PCB->InvisibleObjectsColor);
  DrawStrippedText (element, PCB->minSlk);
}

static int
name_callback (const BoxType * b, void *cl)
{
  TextTypePtr text = (TextTypePtr) b;
  ElementTypePtr element = (ElementTypePtr) text->Element;
  int *side = cl;

  if (TEST_FLAG (HIDENAMEFLAG, element))
    return 0;

  if (ON_SIDE (element, *side))
    draw_element_name (element);
  return 0;
}

static void
draw_element_pins_and_pads (ElementType *element)
{
  PAD_LOOP (element);
  {
    if (doing_pinout || doing_assy || FRONT (pad) || PCB->InvisibleObjectsOn)
      {
        if (TEST_FLAG (WARNFLAG, pad))          gui->set_color (Output.fgGC, PCB->WarnColor);
        else if (TEST_FLAG (SELECTEDFLAG, pad)) gui->set_color (Output.fgGC, PCB->PinSelectedColor);
        else if (TEST_FLAG (FOUNDFLAG, pad))    gui->set_color (Output.fgGC, PCB->ConnectedColor);
        else if (FRONT (pad))                   gui->set_color (Output.fgGC, PCB->PinColor);
        else                                    gui->set_color (Output.fgGC, PCB->InvisibleObjectsColor);

        dapi->draw_pad (pad, NULL, NULL);
      }
  }
  END_LOOP;
  PIN_LOOP (element);
  {
    set_pv_color (pin, PIN_TYPE);
    dapi->draw_pin (pin, NULL, NULL);

    if (TEST_FLAG (WARNFLAG, pin))          gui->set_color (Output.fgGC, PCB->WarnColor);
    else if (TEST_FLAG (SELECTEDFLAG, pin)) gui->set_color (Output.fgGC, PCB->PinSelectedColor);
    else                                    gui->set_color (Output.fgGC, Settings.BlackColor);

    dapi->draw_pin_hole (pin, NULL, NULL);
  }
  END_LOOP;
}

static int
EMark_callback (const BoxType * b, void *cl)
{
  ElementTypePtr element = (ElementTypePtr) b;

  DrawEMark (element, element->MarkX, element->MarkY, !FRONT (element));
  return 1;
}

static int
rat_callback (const BoxType * b, void *cl)
{
  RatType *rat = (RatType *)b;

  if (TEST_FLAG (SELECTEDFLAG, rat))   gui->set_color (Output.fgGC, PCB->RatSelectedColor);
  else if (TEST_FLAG (FOUNDFLAG, rat)) gui->set_color (Output.fgGC, PCB->ConnectedColor);
  else                                 gui->set_color (Output.fgGC, PCB->RatColor);

  dapi->draw_rat (rat, NULL, NULL);
  return 1;
}

static void
draw_element_package (ElementType *element)
{
  /* set color and draw lines, arcs, text and pins */
  if (doing_pinout || doing_assy)
    gui->set_color (Output.fgGC, PCB->ElementColor);
  else if (TEST_FLAG (SELECTEDFLAG, element))
    gui->set_color (Output.fgGC, PCB->ElementSelectedColor);
  else if (FRONT (element))
    gui->set_color (Output.fgGC, PCB->ElementColor);
  else
    gui->set_color (Output.fgGC, PCB->InvisibleObjectsColor);

  /* draw lines, arcs, text and pins */
  ELEMENTLINE_LOOP (element);
  {
    //_draw_line (line);
    dapi->draw_line (line, NULL, NULL);
  }
  END_LOOP;
  ARC_LOOP (element);
  {
    //_draw_arc (arc);
    dapi->draw_arc (arc, NULL, NULL);
  }
  END_LOOP;
}

static int
element_callback (const BoxType * b, void *cl)
{
  ElementTypePtr element = (ElementTypePtr) b;
  int *side = cl;

  if (ON_SIDE (element, *side))
    draw_element_package (element);
  return 1;
}

/* ---------------------------------------------------------------------------
 * prints assembly drawing.
 */

void
PrintAssembly (int side, const BoxType * drawn_area)
{
  int side_group = GetLayerGroupNumberByNumber (max_copper_layer + side);

  doing_assy = true;
  gui->set_draw_faded (Output.fgGC, 1);
  DrawLayerGroup (side_group, drawn_area);
  gui->set_draw_faded (Output.fgGC, 0);

  /* draw package */
  DrawSilk (side, drawn_area);
  doing_assy = false;
}

/* ---------------------------------------------------------------------------
 * initializes some identifiers for a new zoom factor and redraws whole screen
 */
static void
DrawEverything (const BoxType *drawn_area)
{
  int i, ngroups, side;
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
      side = SWAP_IDENT ? COMPONENT_LAYER : SOLDER_LAYER;
      if (PCB->ElementOn)
	{
	  r_search (PCB->Data->element_tree, drawn_area, NULL, element_callback, &side);
	  r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], drawn_area, NULL, name_callback, &side);
	  dapi->draw_layer (&(PCB->Data->Layer[max_copper_layer + side]), drawn_area, NULL);
	}
      r_search (PCB->Data->pad_tree, drawn_area, NULL, pad_callback, &side);
      gui->end_layer ();
    }

  /* draw all layers in layerstack order */
  for (i = ngroups - 1; i >= 0; i--)
    {
      int group = drawn_groups[i];

      if (gui->set_layer (0, group, 0))
        {
          DrawLayerGroup (group, drawn_area);
          gui->end_layer ();
        }
    }

  if (TEST_FLAG (CHECKPLANESFLAG, PCB) && gui->gui)
    return;

  /* Draw pins, pads, vias below silk */
  if (gui->gui)
    dapi->draw_ppv (SWAP_IDENT ? solder : component, drawn_area, NULL);
  else
    {
      CountHoles (&plated, &unplated, drawn_area);

      if (plated && gui->set_layer ("plated-drill", SL (PDRILL, 0), 0))
        {
          dapi->draw_holes (1, drawn_area, NULL);
          gui->end_layer ();
        }

      if (unplated && gui->set_layer ("unplated-drill", SL (UDRILL, 0), 0))
        {
          dapi->draw_holes (0, drawn_area, NULL);
          gui->end_layer ();
        }
    }

  /* Draw the solder mask if turned on */
  if (gui->set_layer ("componentmask", SL (MASK, TOP), 0))
    {
      DrawMask (COMPONENT_LAYER, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("soldermask", SL (MASK, BOTTOM), 0))
    {
      DrawMask (SOLDER_LAYER, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("topsilk", SL (SILK, TOP), 0))
    {
      DrawSilk (COMPONENT_LAYER, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("bottomsilk", SL (SILK, BOTTOM), 0))
    {
      DrawSilk (SOLDER_LAYER, drawn_area);
      gui->end_layer ();
    }

  if (gui->gui)
    {
      /* Draw element Marks */
      if (PCB->PinOn)
	r_search (PCB->Data->element_tree, drawn_area, NULL, EMark_callback,
		  NULL);
      /* Draw rat lines on top */
      if (gui->set_layer ("rats", SL (RATS, 0), 0))
        {
          DrawRats(drawn_area);
          gui->end_layer ();
        }
    }

  paste_empty = IsPasteEmpty (COMPONENT_LAYER);
  if (gui->set_layer ("toppaste", SL (PASTE, TOP), paste_empty))
    {
      DrawPaste (COMPONENT_LAYER, drawn_area);
      gui->end_layer ();
    }

  paste_empty = IsPasteEmpty (SOLDER_LAYER);
  if (gui->set_layer ("bottompaste", SL (PASTE, BOTTOM), paste_empty))
    {
      DrawPaste (SOLDER_LAYER, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("topassembly", SL (ASSY, TOP), 0))
    {
      PrintAssembly (COMPONENT_LAYER, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("bottomassembly", SL (ASSY, BOTTOM), 0))
    {
      PrintAssembly (SOLDER_LAYER, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("fab", SL (FAB, 0), 0))
    {
      PrintFab (Output.fgGC);
      gui->end_layer ();
    }
}

/* static */ void
DrawEMark (ElementTypePtr e, Coord X, Coord Y, bool invisible)
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

  gui->set_color (Output.fgGC,
		  invisible ? PCB->InvisibleMarkColor : PCB->ElementColor);
  gui->set_line_cap (Output.fgGC, Trace_Cap);
  gui->set_line_width (Output.fgGC, 0);
  gui->draw_line (Output.fgGC, X - mark_size, Y, X, Y - mark_size);
  gui->draw_line (Output.fgGC, X + mark_size, Y, X, Y - mark_size);
  gui->draw_line (Output.fgGC, X - mark_size, Y, X, Y + mark_size);
  gui->draw_line (Output.fgGC, X + mark_size, Y, X, Y + mark_size);

  /*
   * If an element is locked, place a "L" on top of the "diamond".
   * This provides a nice visual indication that it is locked that
   * works even for color blind users.
   */
  if (TEST_FLAG (LOCKFLAG, e) )
    {
      gui->draw_line (Output.fgGC, X, Y, X + 2 * mark_size, Y);
      gui->draw_line (Output.fgGC, X, Y, X, Y - 4* mark_size);
    }
}

static int
pin_mask_callback (const BoxType * b, void *cl)
{
  dapi->draw_pin_mask ((PinType *) b, NULL, NULL);
  return 1;
}

static int
via_mask_callback (const BoxType * b, void *cl)
{
  dapi->draw_via_mask ((PinType *) b, NULL, NULL);
  return 1;
}

static int
pad_mask_callback (const BoxType * b, void *cl)
{
  PadTypePtr pad = (PadTypePtr) b;
  int *side = cl;
  if (ON_SIDE (pad, *side))
    dapi->draw_pad_mask (pad, NULL, NULL);
  return 1;
}

/* ---------------------------------------------------------------------------
 * Draws silk layer.
 */

void
DrawSilk (int side, const BoxType * drawn_area)
{
#if 0
  /* This code is used when you want to mask silk to avoid exposed
     pins and pads.  We decided it was a bad idea to do this
     unconditionally, but the code remains.  */
#endif

#if 0
  if (gui->poly_before)
    {
      gui->use_mask (HID_MASK_BEFORE);
#endif
      dapi->draw_layer (LAYER_PTR (max_copper_layer + side), drawn_area, NULL);
      /* draw package */
      r_search (PCB->Data->element_tree, drawn_area, NULL, element_callback, &side);
      r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], drawn_area, NULL, name_callback, &side);
#if 0
    }

  gui->use_mask (HID_MASK_CLEAR);
  r_search (PCB->Data->pin_tree, drawn_area, NULL, pin_mask_callback, NULL);
  r_search (PCB->Data->via_tree, drawn_area, NULL, via_mask_callback, NULL);
  r_search (PCB->Data->pad_tree, drawn_area, NULL, pad_mask_callback, &side);

  if (gui->poly_after)
    {
      gui->use_mask (HID_MASK_AFTER);
      dapi->draw_layer (LAYER_PTR (max_copper_layer + layer), drawn_area, NULL);
      /* draw package */
      r_search (PCB->Data->element_tree, drawn_area, NULL, element_callback, &side);
      r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], drawn_area, NULL, name_callback, &side);
    }
  gui->use_mask (HID_MASK_OFF);
#endif
}


static void
DrawMaskBoardArea (int mask_type, const BoxType *drawn_area)
{
  /* Skip the mask drawing if the GUI doesn't want this type */
  if ((mask_type == HID_MASK_BEFORE && !gui->poly_before) ||
      (mask_type == HID_MASK_AFTER  && !gui->poly_after))
    return;

  gui->use_mask (mask_type);
  gui->set_color (Output.fgGC, PCB->MaskColor);
  if (drawn_area == NULL)
    gui->fill_rect (Output.fgGC, 0, 0, PCB->MaxWidth, PCB->MaxHeight);
  else
    gui->fill_rect (Output.fgGC, drawn_area->X1, drawn_area->Y1,
                                 drawn_area->X2, drawn_area->Y2);
}

/* ---------------------------------------------------------------------------
 * draws solder mask layer - this will cover nearly everything
 */
void
DrawMask (int side, const BoxType *screen)
{
  int thin = TEST_FLAG(THINDRAWFLAG, PCB) || TEST_FLAG(THINDRAWPOLYFLAG, PCB);

  if (thin)
    gui->set_color (Output.pmGC, PCB->MaskColor);
  else
    {
      DrawMaskBoardArea (HID_MASK_BEFORE, screen);
      gui->use_mask (HID_MASK_CLEAR);
    }

  r_search (PCB->Data->pin_tree, screen, NULL, pin_mask_callback, NULL);
  r_search (PCB->Data->via_tree, screen, NULL, via_mask_callback, NULL);
  r_search (PCB->Data->pad_tree, screen, NULL, pad_mask_callback, &side);

  if (thin)
    gui->set_color (Output.pmGC, "erase");
  else
    {
      DrawMaskBoardArea (HID_MASK_AFTER, screen);
      gui->use_mask (HID_MASK_OFF);
    }
}

/* ---------------------------------------------------------------------------
 * draws solder paste layer for a given side of the board
 */
void
DrawPaste (int side, const BoxType *drawn_area)
{
  gui->set_color (Output.fgGC, PCB->ElementColor);
  ALLPAD_LOOP (PCB->Data);
  {
    if (ON_SIDE (pad, side))
      dapi->draw_pad_paste (pad, NULL, NULL);
  }
  ENDALL_LOOP;
}

/* static */ void
DrawRats (const BoxType *drawn_area)
{
  /*
   * XXX lesstif allows positive AND negative drawing in HID_MASK_CLEAR.
   * XXX gtk only allows negative drawing.
   * XXX using the mask here is to get rat transparency
   */
  int can_mask = strcmp(gui->name, "lesstif") == 0;

  if (can_mask)
    gui->use_mask (HID_MASK_CLEAR);
  r_search (PCB->Data->rat_tree, drawn_area, NULL, rat_callback, NULL);
  if (can_mask)
    gui->use_mask (HID_MASK_OFF);
}

/* ---------------------------------------------------------------------------
 * draws one layer group.  If the exporter is not a GUI,
 * also draws the pins / pads / vias in this layer group.
 */
void
DrawLayerGroup (int group, const BoxType *drawn_area)
{
  int i, rv = 1;
  int layernum;
  LayerTypePtr Layer;
  int n_entries = PCB->LayerGroups.Number[group];
  Cardinal *layers = PCB->LayerGroups.Entries[group];

  for (i = n_entries - 1; i >= 0; i--)
    {
      layernum = layers[i];
      Layer = PCB->Data->Layer + layers[i];
      if (strcmp (Layer->Name, "outline") == 0 ||
          strcmp (Layer->Name, "route") == 0)
        rv = 0;
      if (layernum < max_copper_layer && Layer->On)
        dapi->draw_layer (Layer, drawn_area, NULL);
    }
  if (n_entries > 1)
    rv = 1;

  if (rv && !gui->gui)
    dapi->draw_ppv (group, drawn_area, NULL);
}

static void
GatherPVName (PinTypePtr Ptr)
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
GatherPadName (PadTypePtr Pad)
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

/* ---------------------------------------------------------------------------
 * lowlevel drawing routine for text objects
 */
void
DrawTextLowLevel (TextTypePtr Text, Coord min_line_width)
{
  Coord x = 0;
  unsigned char *string = (unsigned char *) Text->TextString;
  Cardinal n;
  FontTypePtr font = &PCB->Font;

  while (string && *string)
    {
      /* draw lines if symbol is valid and data is present */
      if (*string <= MAX_FONTPOSITION && font->Symbol[*string].Valid)
	{
	  LineTypePtr line = font->Symbol[*string].Line;
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
	      _draw_line (&newline);
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
	  gui->fill_rect (Output.fgGC,
			  defaultsymbol.X1, defaultsymbol.Y1,
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
DrawVia (PinTypePtr Via)
{
  AddPart (Via);
  if (!TEST_FLAG (HOLEFLAG, Via) && TEST_FLAG (DISPLAYNAMEFLAG, Via))
    DrawViaName (Via);
}

/* ---------------------------------------------------------------------------
 * draws the name of a via
 */
void
DrawViaName (PinTypePtr Via)
{
  GatherPVName (Via);
}

/* ---------------------------------------------------------------------------
 * draw a pin object
 */
void
DrawPin (PinTypePtr Pin)
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
DrawPinName (PinTypePtr Pin)
{
  GatherPVName (Pin);
}

/* ---------------------------------------------------------------------------
 * draw a pad object
 */
void
DrawPad (PadTypePtr Pad)
{
  AddPart (Pad);
  if (doing_pinout || TEST_FLAG (DISPLAYNAMEFLAG, Pad))
    DrawPadName (Pad);
}

/* ---------------------------------------------------------------------------
 * draws the name of a pad
 */
void
DrawPadName (PadTypePtr Pad)
{
  GatherPadName (Pad);
}

/* ---------------------------------------------------------------------------
 * draws a line on a layer
 */
void
DrawLine (LayerTypePtr Layer, LineTypePtr Line)
{
  AddPart (Line);
}

/* ---------------------------------------------------------------------------
 * draws a ratline
 */
void
DrawRat (RatTypePtr Rat)
{
  if (Settings.RatThickness < 20)
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
DrawArc (LayerTypePtr Layer, ArcTypePtr Arc)
{
  AddPart (Arc);
}

/* ---------------------------------------------------------------------------
 * draws a text on a layer
 */
void
DrawText (LayerTypePtr Layer, TextTypePtr Text)
{
  AddPart (Text);
}


/* ---------------------------------------------------------------------------
 * draws a polygon on a layer
 */
void
DrawPolygon (LayerTypePtr Layer, PolygonTypePtr Polygon)
{
  AddPart (Polygon);
}

/* ---------------------------------------------------------------------------
 * draws an element
 */
void
DrawElement (ElementTypePtr Element)
{
  DrawElementPackage (Element);
  DrawElementName (Element);
  DrawElementPinsAndPads (Element);
}

/* ---------------------------------------------------------------------------
 * draws the name of an element
 */
void
DrawElementName (ElementTypePtr Element)
{
  if (TEST_FLAG (HIDENAMEFLAG, Element))
    return;
  DrawText (NULL, &ELEMENT_TEXT (PCB, Element));
}

/* ---------------------------------------------------------------------------
 * draws the package of an element
 */
void
DrawElementPackage (ElementTypePtr Element)
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
DrawElementPinsAndPads (ElementTypePtr Element)
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
EraseVia (PinTypePtr Via)
{
  AddPart (Via);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Via))
    EraseViaName (Via);
}

/* ---------------------------------------------------------------------------
 * erase a ratline
 */
void
EraseRat (RatTypePtr Rat)
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
EraseViaName (PinTypePtr Via)
{
  GatherPVName (Via);
}

/* ---------------------------------------------------------------------------
 * erase a pad object
 */
void
ErasePad (PadTypePtr Pad)
{
  AddPart (Pad);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Pad))
    ErasePadName (Pad);
}

/* ---------------------------------------------------------------------------
 * erase a pad name
 */
void
ErasePadName (PadTypePtr Pad)
{
  GatherPadName (Pad);
}

/* ---------------------------------------------------------------------------
 * erase a pin object
 */
void
ErasePin (PinTypePtr Pin)
{
  AddPart (Pin);
  if (TEST_FLAG (DISPLAYNAMEFLAG, Pin))
    ErasePinName (Pin);
}

/* ---------------------------------------------------------------------------
 * erase a pin name
 */
void
ErasePinName (PinTypePtr Pin)
{
  GatherPVName (Pin);
}

/* ---------------------------------------------------------------------------
 * erases a line on a layer
 */
void
EraseLine (LineTypePtr Line)
{
  AddPart (Line);
}

/* ---------------------------------------------------------------------------
 * erases an arc on a layer
 */
void
EraseArc (ArcTypePtr Arc)
{
  if (!Arc->Thickness)
    return;
  AddPart (Arc);
}

/* ---------------------------------------------------------------------------
 * erases a text on a layer
 */
void
EraseText (LayerTypePtr Layer, TextTypePtr Text)
{
  AddPart (Text);
}

/* ---------------------------------------------------------------------------
 * erases a polygon on a layer
 */
void
ErasePolygon (PolygonTypePtr Polygon)
{
  AddPart (Polygon);
}

/* ---------------------------------------------------------------------------
 * erases an element
 */
void
EraseElement (ElementTypePtr Element)
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
EraseElementPinsAndPads (ElementTypePtr Element)
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
EraseElementName (ElementTypePtr Element)
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
      ErasePin ((PinTypePtr) ptr);
      break;
    case TEXT_TYPE:
    case ELEMENTNAME_TYPE:
      EraseText ((LayerTypePtr)lptr, (TextTypePtr) ptr);
      break;
    case POLYGON_TYPE:
      ErasePolygon ((PolygonTypePtr) ptr);
      break;
    case ELEMENT_TYPE:
      EraseElement ((ElementTypePtr) ptr);
      break;
    case LINE_TYPE:
    case ELEMENTLINE_TYPE:
    case RATLINE_TYPE:
      EraseLine ((LineTypePtr) ptr);
      break;
    case PAD_TYPE:
      ErasePad ((PadTypePtr) ptr);
      break;
    case ARC_TYPE:
    case ELEMENTARC_TYPE:
      EraseArc ((ArcTypePtr) ptr);
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
	DrawVia ((PinTypePtr) ptr2);
      break;
    case LINE_TYPE:
      if (((LayerTypePtr) ptr1)->On)
	DrawLine ((LayerTypePtr) ptr1, (LineTypePtr) ptr2);
      break;
    case ARC_TYPE:
      if (((LayerTypePtr) ptr1)->On)
	DrawArc ((LayerTypePtr) ptr1, (ArcTypePtr) ptr2);
      break;
    case TEXT_TYPE:
      if (((LayerTypePtr) ptr1)->On)
	DrawText ((LayerTypePtr) ptr1, (TextTypePtr) ptr2);
      break;
    case POLYGON_TYPE:
      if (((LayerTypePtr) ptr1)->On)
	DrawPolygon ((LayerTypePtr) ptr1, (PolygonTypePtr) ptr2);
      break;
    case ELEMENT_TYPE:
      if (PCB->ElementOn &&
	  (FRONT ((ElementTypePtr) ptr2) || PCB->InvisibleObjectsOn))
	DrawElement ((ElementTypePtr) ptr2);
      break;
    case RATLINE_TYPE:
      if (PCB->RatOn)
	DrawRat ((RatTypePtr) ptr2);
      break;
    case PIN_TYPE:
      if (PCB->PinOn)
	DrawPin ((PinTypePtr) ptr2);
      break;
    case PAD_TYPE:
      if (PCB->PinOn)
	DrawPad ((PadTypePtr) ptr2);
      break;
    case ELEMENTNAME_TYPE:
      if (PCB->ElementOn &&
	  (FRONT ((ElementTypePtr) ptr2) || PCB->InvisibleObjectsOn))
	DrawElementName ((ElementTypePtr) ptr1);
      break;
    }
}

static void
draw_element (ElementTypePtr element)
{
  draw_element_package (element);
  draw_element_name (element);
  draw_element_pins_and_pads (element);
}

/* ---------------------------------------------------------------------------
 * HID drawing callback.
 */

void
hid_expose_callback (HID * hid, BoxType * region, void *item)
{
  HID *old_gui = gui;

  gui = hid;
  Output.fgGC = gui->make_gc ();
  Output.bgGC = gui->make_gc ();
  Output.pmGC = gui->make_gc ();

  hid->set_color (Output.pmGC, "erase");
  hid->set_color (Output.bgGC, "drill");

  if (item)
    {
      doing_pinout = true;
      draw_element ((ElementType *)item);
      doing_pinout = false;
    }
  else
    DrawEverything (region);

  gui->destroy_gc (Output.fgGC);
  gui->destroy_gc (Output.bgGC);
  gui->destroy_gc (Output.pmGC);
  gui = old_gui;
}
