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
<<<<<<< current
/* static */ void DrawEMark (ElementType *, Coord, Coord, bool);
/* static */ void DrawRats (const BoxType *);

=======
static void SetPVColor (PinTypePtr, int);
/* static */ void DrawEMark (ElementTypePtr, LocationType, LocationType, bool);
static void ClearPad (PadTypePtr, bool);
/* static */ void DrawHole (PinTypePtr);
static void DrawMask (BoxType *);
/* static */ void DrawRats (BoxType *);
/* static */ void DrawSilk (int, int, const BoxType *);
static int pin_callback (const BoxType * b, void *cl);
static int pad_callback (const BoxType * b, void *cl);

void hidgl_hack_ignore_stencil (int ignore);

/*--------------------------------------------------------------------------------------
 * setup color for pin or via
 */
>>>>>>> patched
static void
set_object_color (AnyObjectType *obj, char *warn_color, char *selected_color,
                  char *connected_color, char *found_color, char *normal_color)
{
  char *color;

<<<<<<< current
  if      (warn_color      != NULL && TEST_FLAG (WARNFLAG,      obj)) color = warn_color;
  else if (selected_color  != NULL && TEST_FLAG (SELECTEDFLAG,  obj)) color = selected_color;
  else if (connected_color != NULL && TEST_FLAG (CONNECTEDFLAG, obj)) color = connected_color;
  else if (found_color     != NULL && TEST_FLAG (FOUNDFLAG,     obj)) color = found_color;
  else                                                                color = normal_color;
=======
  if (Type == VIA_TYPE)
    {
      if (!doing_pinout
	  && TEST_FLAG (WARNFLAG | SELECTEDFLAG | FOUNDFLAG, Pin))
	{
	  if (TEST_FLAG (WARNFLAG, Pin))
	    color = PCB->WarnColor;
	  else if (TEST_FLAG (SELECTEDFLAG, Pin))
	    color = PCB->ViaSelectedColor;
	  else
	    color = PCB->ConnectedColor;
          hidgl_hack_ignore_stencil (1);
	}
      else
	color = PCB->ViaColor;
    }
  else
    {
      if (!doing_pinout
	  && TEST_FLAG (WARNFLAG | SELECTEDFLAG | FOUNDFLAG, Pin))
	{
	  if (TEST_FLAG (WARNFLAG, Pin))
	    color = PCB->WarnColor;
	  else if (TEST_FLAG (SELECTEDFLAG, Pin))
	    color = PCB->PinSelectedColor;
	  else
	    color = PCB->ConnectedColor;
          hidgl_hack_ignore_stencil (1);
	}
      else
	color = PCB->PinColor;
    }
>>>>>>> patched

  hid_draw_set_color (Output.fgGC, color);
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

static int
pad_callback (const BoxType * b, void *cl)
{
  PadType *pad = (PadType *) b;
  int *side = cl;

  if (ON_SIDE (pad, *side))
    {
      set_object_color ((AnyObjectType *)pad, PCB->WarnColor,
                        PCB->PinSelectedColor, PCB->ConnectedColor, PCB->FoundColor,
                        FRONT (pad) ? PCB->PinColor : PCB->InvisibleObjectsColor);

      dapi->draw_pad (pad, NULL, NULL);
    }
  return 1;
}

static void
draw_element_name (ElementType *element)
{
  if ((TEST_FLAG (HIDENAMESFLAG, PCB) && gui->gui) ||
      TEST_FLAG (HIDENAMEFLAG, element))
    return;
  if (doing_pinout || doing_assy)
    hid_draw_set_color (Output.fgGC, PCB->ElementColor);
  else if (TEST_FLAG (SELECTEDFLAG, &ELEMENT_TEXT (PCB, element)))
    hid_draw_set_color (Output.fgGC, PCB->ElementSelectedColor);
  else if (FRONT (element))
    hid_draw_set_color (Output.fgGC, PCB->ElementColor);
  else
    hid_draw_set_color (Output.fgGC, PCB->InvisibleObjectsColor);
  hid_draw_pcb_text (Output.fgGC, &ELEMENT_TEXT (PCB, element), PCB->minSlk);
}

static int
name_callback (const BoxType * b, void *cl)
{
  TextType *text = (TextType *) b;
  ElementType *element = (ElementType *) text->Element;
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
        set_object_color ((AnyObjectType *)pad, PCB->WarnColor,
                          PCB->PinSelectedColor, PCB->ConnectedColor, PCB->FoundColor,
                          FRONT (pad) ? PCB->PinColor : PCB->InvisibleObjectsColor);

        dapi->draw_pad (pad, NULL, NULL);
      }
  }
  END_LOOP;
  PIN_LOOP (element);
  {
    set_object_color ((AnyObjectType *)pin, PCB->WarnColor, PCB->PinSelectedColor,
                      PCB->ConnectedColor, PCB->FoundColor, PCB->PinColor);

    dapi->draw_pin (pin, NULL, NULL);

    set_object_color ((AnyObjectType *)pin, PCB->WarnColor,
                      PCB->PinSelectedColor, NULL, NULL, Settings.BlackColor);

    dapi->draw_pin_hole (pin, NULL, NULL);
  }
  END_LOOP;
}

static int
EMark_callback (const BoxType * b, void *cl)
{
  ElementType *element = (ElementType *) b;

  DrawEMark (element, element->MarkX, element->MarkY, !FRONT (element));
  return 1;
}

static int
rat_callback (const BoxType * b, void *cl)
{
  RatType *rat = (RatType *)b;

  set_object_color ((AnyObjectType *) rat, NULL, PCB->RatSelectedColor,
                    PCB->ConnectedColor, PCB->FoundColor, PCB->RatColor);

  dapi->draw_rat (rat, NULL, NULL);
  return 1;
}

static void
draw_element_package (ElementType *element)
{
  /* set color and draw lines, arcs, text and pins */
  if (doing_pinout || doing_assy)
    hid_draw_set_color (Output.fgGC, PCB->ElementColor);
  else if (TEST_FLAG (SELECTEDFLAG, element))
    hid_draw_set_color (Output.fgGC, PCB->ElementSelectedColor);
  else if (FRONT (element))
    hid_draw_set_color (Output.fgGC, PCB->ElementColor);
  else
    hid_draw_set_color (Output.fgGC, PCB->InvisibleObjectsColor);

  /* draw lines, arcs, text and pins */
  ELEMENTLINE_LOOP (element);
  {
    //hid_draw_pcb_line (Output.fgGC, line);
    dapi->draw_line (line, NULL, NULL);
  }
  END_LOOP;
  ARC_LOOP (element);
  {
    //hid_draw_pcb_arc (Output.fgGC, arc);
    dapi->draw_arc (arc, NULL, NULL);
  }
  END_LOOP;
}

static int
element_callback (const BoxType * b, void *cl)
{
  ElementType *element = (ElementType *) b;
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
  int side_group = GetLayerGroupNumberBySide (side);

  doing_assy = true;
  hid_draw_set_draw_faded (Output.fgGC, 1);
  DrawLayerGroup (side_group, drawn_area);
  hid_draw_set_draw_faded (Output.fgGC, 0);

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
  int top_group, bottom_group;
  /* This is the list of layer groups we will draw.  */
  int do_group[MAX_GROUP];
  /* This is the reverse of the order in which we draw them.  */
  int drawn_groups[MAX_GROUP];
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

  top_group = GetLayerGroupNumberBySide (TOP_SIDE);
  bottom_group = GetLayerGroupNumberBySide (BOTTOM_SIDE);

  /*
   * first draw all 'invisible' stuff
   */
  if (!TEST_FLAG (CHECKPLANESFLAG, PCB)
      && gui->set_layer ("invisible", SL (INVISIBLE, 0), 0))
    {
      side = SWAP_IDENT ? TOP_SIDE : BOTTOM_SIDE;
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
    dapi->draw_ppv (SWAP_IDENT ? bottom_group : top_group, drawn_area, NULL);
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
      DrawMask (TOP_SIDE, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("soldermask", SL (MASK, BOTTOM), 0))
    {
      DrawMask (BOTTOM_SIDE, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("topsilk", SL (SILK, TOP), 0))
    {
      DrawSilk (TOP_SIDE, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("bottomsilk", SL (SILK, BOTTOM), 0))
    {
      DrawSilk (BOTTOM_SIDE, drawn_area);
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

  paste_empty = IsPasteEmpty (TOP_SIDE);
  if (gui->set_layer ("toppaste", SL (PASTE, TOP), paste_empty))
    {
      DrawPaste (TOP_SIDE, drawn_area);
      gui->end_layer ();
    }

  paste_empty = IsPasteEmpty (BOTTOM_SIDE);
  if (gui->set_layer ("bottompaste", SL (PASTE, BOTTOM), paste_empty))
    {
      DrawPaste (BOTTOM_SIDE, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("topassembly", SL (ASSY, TOP), 0))
    {
      PrintAssembly (TOP_SIDE, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("bottomassembly", SL (ASSY, BOTTOM), 0))
    {
      PrintAssembly (BOTTOM_SIDE, drawn_area);
      gui->end_layer ();
    }

  if (gui->set_layer ("fab", SL (FAB, 0), 0))
    {
      PrintFab (Output.fgGC);
      gui->end_layer ();
    }
}

/* static */ void
DrawEMark (ElementType *e, Coord X, Coord Y, bool invisible)
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

  hid_draw_set_color (Output.fgGC, invisible ? PCB->InvisibleMarkColor : PCB->ElementColor);
  hid_draw_set_line_cap (Output.fgGC, Trace_Cap);
  hid_draw_set_line_width (Output.fgGC, 0);
  hid_draw_line (Output.fgGC, X - mark_size, Y, X, Y - mark_size);
  hid_draw_line (Output.fgGC, X + mark_size, Y, X, Y - mark_size);
  hid_draw_line (Output.fgGC, X - mark_size, Y, X, Y + mark_size);
  hid_draw_line (Output.fgGC, X + mark_size, Y, X, Y + mark_size);

  /*
   * If an element is locked, place a "L" on top of the "diamond".
   * This provides a nice visual indication that it is locked that
   * works even for color blind users.
   */
  if (TEST_FLAG (LOCKFLAG, e) )
    {
      hid_draw_line (Output.fgGC, X, Y, X + 2 * mark_size, Y);
      hid_draw_line (Output.fgGC, X, Y, X, Y - 4* mark_size);
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
  PadType *pad = (PadType *) b;
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
      hid_draw_use_mask (gui->graphics, HID_MASK_BEFORE);
#endif
      dapi->draw_layer (LAYER_PTR (max_copper_layer + side), drawn_area, NULL);
      /* draw package */
      r_search (PCB->Data->element_tree, drawn_area, NULL, element_callback, &side);
      r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], drawn_area, NULL, name_callback, &side);
#if 0
    }

  hid_draw_use_mask (gui->graphics, HID_MASK_CLEAR);
  r_search (PCB->Data->pin_tree, drawn_area, NULL, pin_mask_callback, NULL);
  r_search (PCB->Data->via_tree, drawn_area, NULL, via_mask_callback, NULL);
  r_search (PCB->Data->pad_tree, drawn_area, NULL, pad_mask_callback, &side);

  if (gui->poly_after)
    {
      hid_draw_use_mask (gui->graphics, HID_MASK_AFTER);
      dapi->draw_layer (LAYER_PTR (max_copper_layer + layer), drawn_area, NULL);
      /* draw package */
      r_search (PCB->Data->element_tree, drawn_area, NULL, element_callback, &side);
      r_search (PCB->Data->name_tree[NAME_INDEX (PCB)], drawn_area, NULL, name_callback, &side);
    }
  hid_draw_use_mask (gui->graphics, HID_MASK_OFF);
#endif
}


<<<<<<< current
=======
  if (thin)
    gui->set_color (Output.pmGC, "erase");
  else
    {
      if (gui->poly_after)
	{
	  gui->use_mask (HID_MASK_AFTER);
	  gui->set_color (out->fgGC, PCB->MaskColor);
	  gui->fill_rect (out->fgGC, 0, 0, PCB->MaxWidth, PCB->MaxHeight);
	}
      gui->use_mask (HID_MASK_OFF);
    }

#if 0
  /* Some fabs want the board outline on the solder mask layer.  If
     you need this, change the '0' above to '1', and the code below
     will copy the outline layer to the mask layers.  */
  if (!gui->gui)
    {
      int i;
      for (i=PCB->Data->LayerN; i>=0; i--)
	{
	  LayerTypePtr Layer = PCB->Data->Layer + i;
	  if (strcmp (Layer->Name, "outline") == 0)
	    DrawLayer (Layer, screen);
	}
    }
#endif
}

/* static */ void
DrawRats (BoxTypePtr drawn_area)
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

static int
line_callback (const BoxType * b, void *cl)
{
  DrawLine ((LayerTypePtr) cl, (LineTypePtr) b, 0);
  return 1;
}

static int
arc_callback (const BoxType * b, void *cl)
{
  DrawArc ((LayerTypePtr) cl, (ArcTypePtr) b, 0);
  return 1;
}

static int
text_callback (const BoxType * b, void *cl)
{
  DrawRegularText ((LayerTypePtr) cl, (TextTypePtr) b, 0);
  return 1;
}


/* ---------------------------------------------------------------------------
 * draws one non-copper layer
 */
void
DrawLayer (LayerTypePtr Layer, const BoxType * screen)
{
  struct pin_info info;

  /* print the non-clearing polys */
  info.Layer = Layer;
  info.arg = false;
  clip_box = screen;
  r_search (Layer->polygon_tree, screen, NULL, poly_callback, &info);

  /* draw all visible lines this layer */
  r_search (Layer->line_tree, screen, NULL, line_callback, Layer);

  /* draw the layer arcs on screen */
  r_search (Layer->arc_tree, screen, NULL, arc_callback, Layer);

  /* draw the layer text on screen */
  r_search (Layer->text_tree, screen, NULL, text_callback, Layer);
  clip_box = NULL;
}

/* ---------------------------------------------------------------------------
 * draws one layer group.  Returns non-zero if pins and pads should be
 * drawn with this group.
 */
static int
DrawLayerGroup (int group, const BoxType * screen)
{
  int i, rv = 1;
  int layernum;
  struct pin_info info;
  LayerTypePtr Layer;
  int n_entries = PCB->LayerGroups.Number[group];
  Cardinal *layers = PCB->LayerGroups.Entries[group];

  clip_box = screen;
  for (i = n_entries - 1; i >= 0;
      i--, gui->set_layer (0, group, 0)) /* HACK: Subcomposite each layer in a layer group separately */
    {
      layernum = layers[i];
      Layer = PCB->Data->Layer + layers[i];
      if (strcmp (Layer->Name, "outline") == 0 ||
	  strcmp (Layer->Name, "route") == 0)
	rv = 0;
      if (layernum < max_layer && Layer->On)
	{
	  /* draw all polygons on this layer */
	  if (Layer->PolygonN)
	    {
	      info.Layer = Layer;
	      info.arg = true;
	      r_search (Layer->polygon_tree, screen, NULL, poly_callback,
			&info);
	      info.arg = false;

	      /* HACK: Subcomposite polygons separately from other layer primitives */
	      /* Reset the compositing */
	      gui->set_layer (0, group, 0);
	    }

	  if (TEST_FLAG (CHECKPLANESFLAG, PCB))
	    continue;

	  /* draw all visible lines this layer */
	  r_search (Layer->line_tree, screen, NULL, line_callback, Layer);

	  /* draw the layer arcs on screen */
	  r_search (Layer->arc_tree, screen, NULL, arc_callback, Layer);

	  /* draw the layer text on screen */
	  r_search (Layer->text_tree, screen, NULL, text_callback, Layer);

	}
    }
  if (n_entries > 1)
    rv = 1;
  return rv;
}

/* ---------------------------------------------------------------------------
 * draws one polygon
 * x and y are already in display coordinates
 * the points are numbered:
 *
 *          5 --- 6
 *         /       \
 *        4         7
 *        |         |
 *        3         0
 *         \       /
 *          2 --- 1
  */
/* static */ void
DrawSpecialPolygon (hidGC DrawGC,
		    LocationType X, LocationType Y, int Thickness,
		    int thin_draw)
{
  static FloatPolyType p[8] = {
    {
     0.5, -TAN_22_5_DEGREE_2},
    {
     TAN_22_5_DEGREE_2, -0.5},
    {
     -TAN_22_5_DEGREE_2, -0.5},
    {
     -0.5, -TAN_22_5_DEGREE_2},
    {
     -0.5, TAN_22_5_DEGREE_2},
    {
     -TAN_22_5_DEGREE_2, 0.5},
    {
     TAN_22_5_DEGREE_2, 0.5},
    {
     0.5, TAN_22_5_DEGREE_2}
  };
  static int special_size = 0;
  static int scaled_x[8];
  static int scaled_y[8];
  int polygon_x[9];
  int polygon_y[9];
  int i;


  if (Thickness != special_size)
    {
      special_size = Thickness;
      for (i = 0; i < 8; i++)
	{
	  scaled_x[i] = p[i].X * special_size;
	  scaled_y[i] = p[i].Y * special_size;
	}
    }
  /* add line offset */
  for (i = 0; i < 8; i++)
    {
      polygon_x[i] = X + scaled_x[i];
      polygon_y[i] = Y + scaled_y[i];
    }
  if (thin_draw)
    {
      int i;
      gui->set_line_cap (DrawGC, Round_Cap);
      gui->set_line_width (DrawGC, 0);
      polygon_x[8] = X + scaled_x[0];
      polygon_y[8] = Y + scaled_y[0];
      for (i = 0; i < 8; i++)
	gui->draw_line (DrawGC, polygon_x[i], polygon_y[i],
			polygon_x[i + 1], polygon_y[i + 1]);
    }
  else
    gui->fill_polygon (DrawGC, 8, polygon_x, polygon_y);
}

/* ---------------------------------------------------------------------------
 * lowlevel drawing routine for pins and vias
 */
/* static */ void
DrawPinOrViaLowLevel (PinTypePtr Ptr, bool drawHole)
{
  if (Gathering)
    {
      AddPart (Ptr);
      return;
    }

  if (TEST_FLAG (HOLEFLAG, Ptr))
    {
      if (drawHole)
	{
	  gui->fill_circle (Output.bgGC, Ptr->X, Ptr->Y, Ptr->Thickness / 2);
	  gui->set_line_cap (Output.fgGC, Round_Cap);
	  gui->set_line_width (Output.fgGC, 0);
	  gui->draw_arc (Output.fgGC, Ptr->X, Ptr->Y,
			 Ptr->Thickness / 2, Ptr->Thickness / 2, 0, 360);
	}
      return;
    }
  if (TEST_FLAG (SQUAREFLAG, Ptr))
    {
      int l, r, t, b;
      l = Ptr->X - Ptr->Thickness / 2;
      b = Ptr->Y - Ptr->Thickness / 2;
      r = l + Ptr->Thickness;
      t = b + Ptr->Thickness;
      if (TEST_FLAG (THINDRAWFLAG, PCB))
        {
          gui->set_line_cap (Output.fgGC, Round_Cap);
          gui->set_line_width (Output.fgGC, 0);
          gui->draw_line (Output.fgGC, r, t, r, b);
          gui->draw_line (Output.fgGC, l, t, l, b);
          gui->draw_line (Output.fgGC, r, t, l, t);
          gui->draw_line (Output.fgGC, r, b, l, b);
        }
      else
        {
          gui->fill_rect (Output.fgGC, l, b, r, t);
        }
    }
  else if (TEST_FLAG (OCTAGONFLAG, Ptr))
    {
      DrawSpecialPolygon (Output.fgGC, Ptr->X, Ptr->Y, Ptr->Thickness,
			  TEST_FLAG (THINDRAWFLAG, PCB));
    }
  else
    {				/* draw a round pin or via */
      if (TEST_FLAG (THINDRAWFLAG, PCB))
	{
	  gui->set_line_cap (Output.fgGC, Round_Cap);
	  gui->set_line_width (Output.fgGC, 0);
	  gui->draw_arc (Output.fgGC, Ptr->X, Ptr->Y,
			 Ptr->Thickness / 2, Ptr->Thickness / 2, 0, 360);
	}
      else
	{
	  gui->fill_circle (Output.fgGC, Ptr->X, Ptr->Y, Ptr->Thickness / 2);
	}
    }

  /* and the drilling hole  (which is always round */
  if (drawHole)
    {
      if (TEST_FLAG (THINDRAWFLAG, PCB))
	{
	  gui->set_line_cap (Output.fgGC, Round_Cap);
	  gui->set_line_width (Output.fgGC, 0);
	  gui->draw_arc (Output.fgGC,
			 Ptr->X, Ptr->Y, Ptr->DrillingHole / 2,
			 Ptr->DrillingHole / 2, 0, 360);
	}
      else
	{
	  gui->fill_circle (Output.bgGC, Ptr->X, Ptr->Y,
			    Ptr->DrillingHole / 2);
	}
    }
}

/**************************************************************
 * draw pin/via hole
 */
/* static */ void
DrawHole (PinTypePtr Ptr)
{
  if (TEST_FLAG (THINDRAWFLAG, PCB))
    {
      if (!TEST_FLAG (HOLEFLAG, Ptr))
	{
	  gui->set_line_cap (Output.fgGC, Round_Cap);
	  gui->set_line_width (Output.fgGC, 0);
	  gui->draw_arc (Output.fgGC,
			 Ptr->X, Ptr->Y, Ptr->DrillingHole / 2,
			 Ptr->DrillingHole / 2, 0, 360);
	}
    }
  else
    {
      gui->fill_circle (Output.bgGC, Ptr->X, Ptr->Y, Ptr->DrillingHole / 2);
    }
  if (TEST_FLAG (HOLEFLAG, Ptr))
    {
      if (TEST_FLAG (WARNFLAG, Ptr))
        {
          gui->set_color (Output.fgGC, PCB->WarnColor);
          hidgl_hack_ignore_stencil (1);
        }
      else if (TEST_FLAG (SELECTEDFLAG, Ptr))
        {
          gui->set_color (Output.fgGC, PCB->ViaSelectedColor);
          hidgl_hack_ignore_stencil (1);
        }
      else
	gui->set_color (Output.fgGC, Settings.BlackColor);

      gui->set_line_cap (Output.fgGC, Round_Cap);
      gui->set_line_width (Output.fgGC, 0);
      gui->draw_arc (Output.fgGC,
		     Ptr->X, Ptr->Y, Ptr->DrillingHole / 2,
		     Ptr->DrillingHole / 2, 0, 360);
      hidgl_hack_ignore_stencil (0);
    }
}

/*******************************************************************
 * draw clearance in pixmask around pins and vias that pierce polygons
 */
static void
ClearOnlyPin (PinTypePtr Pin, bool mask)
{
  BDimension half =
    (mask ? Pin->Mask / 2 : (Pin->Thickness + Pin->Clearance) / 2);

  if (!mask && TEST_FLAG (HOLEFLAG, Pin))
    return;
  if (half == 0)
    return;
  if (!mask && Pin->Clearance <= 0)
    return;

  /* Clear the area around the pin */
  if (TEST_FLAG (SQUAREFLAG, Pin))
    {
      int l, r, t, b;
      l = Pin->X - half;
      b = Pin->Y - half;
      r = l + half * 2;
      t = b + half * 2;
      if (TEST_FLAG (THINDRAWFLAG, PCB) || TEST_FLAG (THINDRAWPOLYFLAG, PCB))
        {
          gui->set_line_cap (Output.pmGC, Round_Cap);
          gui->set_line_width (Output.pmGC, 0);
          gui->draw_line (Output.pmGC, r, t, r, b);
          gui->draw_line (Output.pmGC, l, t, l, b);
          gui->draw_line (Output.pmGC, r, t, l, t);
          gui->draw_line (Output.pmGC, r, b, l, b);
        }
      else
	gui->fill_rect (Output.pmGC, l, b, r, t);
    }
  else if (TEST_FLAG (OCTAGONFLAG, Pin))
    {
      DrawSpecialPolygon (Output.pmGC, Pin->X, Pin->Y, half * 2,
			  TEST_FLAG (THINDRAWFLAG, PCB) ||
			  TEST_FLAG (THINDRAWPOLYFLAG, PCB));
    }
  else
    {
      if (TEST_FLAG (THINDRAWFLAG, PCB) || TEST_FLAG (THINDRAWPOLYFLAG, PCB))
	{
	  gui->set_line_cap (Output.pmGC, Round_Cap);
	  gui->set_line_width (Output.pmGC, 0);
	  gui->draw_arc (Output.pmGC, Pin->X, Pin->Y, half, half, 0, 360);
	}
      else
	gui->fill_circle (Output.pmGC, Pin->X, Pin->Y, half);
    }
}


#if VERTICAL_TEXT
/* vertical text handling provided by Martin Devera with fixes by harry eaton */

/* draw vertical text; xywh is bounding, de is text's descend used for
   positioning */
static void
DrawVText (int x, int y, int w, int h, char *str)
{
  GdkPixmap *pm;
  GdkImage *im;
  GdkGCValues values;
  guint32 pixel;
  int i, j;

  if (!str || !*str)
    return;

  pm = gdk_pixmap_new (DrawingWindow, w, h, -1);

  /* draw into pixmap */
  gdk_draw_rectangle (pm, Output.bgGC, TRUE, 0, 0, w, h);

  gui_draw_string_markup (DrawingWindow, Output.font_desc, Output.fgGC,
			  0, 0, str);

  im = gdk_drawable_get_image (pm, 0, 0, w, h);
  gdk_gc_get_values (Output.fgGC, &values);

  /* draw Transpose(im).  TODO: Pango should be doing vertical text soon */
  for (i = 0; i < w; i++)
    for (j = 0; j < h; j++)
      {
	pixel = gdk_image_get_pixel (im, i, j);
	if (pixel == values.foreground.pixel)
	  gdk_draw_point (DrawingWindow, Output.fgGC, x + j, y + w - i - 1);
      }
  g_object_unref (G_OBJECT (pm));
}
#endif

/* ---------------------------------------------------------------------------
 * lowlevel drawing routine for pin and via names
 */
>>>>>>> patched
static void
DrawMaskBoardArea (int mask_type, const BoxType *drawn_area)
{
  /* Skip the mask drawing if the GUI doesn't want this type */
  if ((mask_type == HID_MASK_BEFORE && !gui->poly_before) ||
      (mask_type == HID_MASK_AFTER  && !gui->poly_after))
    return;

  hid_draw_use_mask (gui->graphics, mask_type);
  hid_draw_set_color (Output.fgGC, PCB->MaskColor);
  if (drawn_area == NULL)
    hid_draw_fill_rect (Output.fgGC, 0, 0, PCB->MaxWidth, PCB->MaxHeight);
  else
    hid_draw_fill_rect (Output.fgGC, drawn_area->X1, drawn_area->Y1,
                                     drawn_area->X2, drawn_area->Y2);
}

struct poly_info {
  const const BoxType *drawn_area;
  LayerType *layer;
};

static int
mask_poly_callback (const BoxType * b, void *cl)
{
  struct poly_info *i = cl;
  PolygonType *polygon = (PolygonType *)b;

  hid_draw_pcb_polygon (Output.pmGC, polygon, i->drawn_area);
  return 1;
}

static int
mask_line_callback (const BoxType * b, void *cl)
{
  LineType *line = (LineType *)b;

  hid_draw_pcb_line (Output.pmGC, line);
  return 1;
}

static int
mask_arc_callback (const BoxType * b, void *cl)
{
  ArcType *arc = (ArcType *)b;

  hid_draw_pcb_arc (Output.pmGC, arc);
  return 1;
}

static int
mask_text_callback (const BoxType * b, void *cl)
{
  LayerType *layer = cl;
  TextType *text = (TextType *)b;
  int min_silk_line;

  if (layer == &PCB->Data->SILKLAYER ||
      layer == &PCB->Data->BACKSILKLAYER)
    min_silk_line = PCB->minSlk;
  else
    min_silk_line = PCB->minWid;
  hid_draw_pcb_text (Output.fgGC, text, min_silk_line);
  return 1;
}

/* ---------------------------------------------------------------------------
 * draws solder mask layer - this will cover nearly everything
 */
void
DrawMask (int side, const BoxType *screen)
{
  int thin = TEST_FLAG(THINDRAWFLAG, PCB) || TEST_FLAG(THINDRAWPOLYFLAG, PCB);
  LayerType *Layer = LAYER_PTR (side == TOP_SIDE ? top_soldermask_layer : bottom_soldermask_layer);
  struct poly_info info;

  if (thin)
    hid_draw_set_color (Output.pmGC, PCB->MaskColor);
  else
    {
      DrawMaskBoardArea (HID_MASK_BEFORE, screen);
      hid_draw_use_mask (gui->graphics, HID_MASK_CLEAR);
    }

  info.layer = Layer;
  info.drawn_area = screen;
  r_search (Layer->polygon_tree, screen, NULL, mask_poly_callback, &info);
  r_search (Layer->line_tree,    screen, NULL, mask_line_callback, Layer);
  r_search (Layer->arc_tree,     screen, NULL, mask_arc_callback,  Layer);
  r_search (Layer->text_tree,    screen, NULL, mask_text_callback, Layer);

  r_search (PCB->Data->pin_tree, screen, NULL, pin_mask_callback, NULL);
  r_search (PCB->Data->via_tree, screen, NULL, via_mask_callback, NULL);
  r_search (PCB->Data->pad_tree, screen, NULL, pad_mask_callback, &side);

  if (thin)
    hid_draw_set_color (Output.pmGC, "erase");
  else
    {
      DrawMaskBoardArea (HID_MASK_AFTER, screen);
      hid_draw_use_mask (gui->graphics, HID_MASK_OFF);
    }
}

/* ---------------------------------------------------------------------------
 * draws solder paste layer for a given side of the board
 */
void
DrawPaste (int side, const BoxType *drawn_area)
{
  hid_draw_set_color (Output.fgGC, PCB->ElementColor);
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
    hid_draw_use_mask (gui->graphics, HID_MASK_CLEAR);
  r_search (PCB->Data->rat_tree, drawn_area, NULL, rat_callback, NULL);
  if (can_mask)
    hid_draw_use_mask (gui->graphics, HID_MASK_OFF);
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
  LayerType *Layer;
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
<<<<<<< current
  AddPart (Via);
  if (!TEST_FLAG (HOLEFLAG, Via) && TEST_FLAG (DISPLAYNAMEFLAG, Via))
    DrawViaName (Via);
=======
  if (!Gathering)
    SetPVColor (Via, VIA_TYPE);
  DrawPinOrViaLowLevel (Via, true);
  hidgl_hack_ignore_stencil (0);
  if (!TEST_FLAG (HOLEFLAG, Via) && TEST_FLAG (DISPLAYNAMEFLAG, Via))
    DrawPinOrViaNameLowLevel (Via);
}

/* ---------------------------------------------------------------------------
 * draw a via without dealing with polygon clearance 
 */
/* static */ void
DrawPlainVia (PinTypePtr Via, bool holeToo)
{
  if (!Gathering)
    SetPVColor (Via, VIA_TYPE);
  DrawPinOrViaLowLevel (Via, holeToo);
  hidgl_hack_ignore_stencil (0);
  if (!TEST_FLAG (HOLEFLAG, Via) && TEST_FLAG (DISPLAYNAMEFLAG, Via))
    DrawPinOrViaNameLowLevel (Via);
>>>>>>> patched
}

/* ---------------------------------------------------------------------------
 * draws the name of a via
 */
void
DrawViaName (PinType *Via)
{
<<<<<<< current
  GatherPVName (Via);
=======
  if (!Gathering)
    {
      if (TEST_FLAG (SELECTEDFLAG, Via))
        {
	  gui->set_color (Output.fgGC, PCB->ViaSelectedColor);
          hidgl_hack_ignore_stencil (1);
        }
      else
	gui->set_color (Output.fgGC, PCB->ViaColor);
    }
  DrawPinOrViaNameLowLevel (Via);
  hidgl_hack_ignore_stencil (0);
>>>>>>> patched
}

/* ---------------------------------------------------------------------------
 * draw a pin object
 */
void
DrawPin (PinType *Pin)
{
<<<<<<< current
  AddPart (Pin);
  if ((!TEST_FLAG (HOLEFLAG, Pin) && TEST_FLAG (DISPLAYNAMEFLAG, Pin))
      || doing_pinout)
    DrawPinName (Pin);
=======
  {
    if (!Gathering)
      SetPVColor (Pin, PIN_TYPE);
    DrawPinOrViaLowLevel (Pin, true);
    hidgl_hack_ignore_stencil (0);
  }
  if ((!TEST_FLAG (HOLEFLAG, Pin) && TEST_FLAG (DISPLAYNAMEFLAG, Pin))
      || doing_pinout)
    DrawPinOrViaNameLowLevel (Pin);
}

/* ---------------------------------------------------------------------------
 * draw a pin without clearing around polygons 
 */
/* static */ void
DrawPlainPin (PinTypePtr Pin, bool holeToo)
{
  if (!Gathering)
    SetPVColor (Pin, PIN_TYPE);
  hidgl_hack_ignore_stencil (0);
  DrawPinOrViaLowLevel (Pin, holeToo);
  if (!TEST_FLAG (HOLEFLAG, Pin) && TEST_FLAG (DISPLAYNAMEFLAG, Pin))
    DrawPinOrViaNameLowLevel (Pin);
>>>>>>> patched
}

/* ---------------------------------------------------------------------------
 * draws the name of a pin
 */
void
DrawPinName (PinType *Pin)
{
<<<<<<< current
  GatherPVName (Pin);
=======
  if (!Gathering)
    {
      if (TEST_FLAG (SELECTEDFLAG, Pin))
        {
	  gui->set_color (Output.fgGC, PCB->PinSelectedColor);
          hidgl_hack_ignore_stencil (1);
        }
      else
	gui->set_color (Output.fgGC, PCB->PinColor);
    }
  DrawPinOrViaNameLowLevel (Pin);
  hidgl_hack_ignore_stencil (0);
>>>>>>> patched
}

/* ---------------------------------------------------------------------------
 * draw a pad object
 */
void
DrawPad (PadType *Pad)
{
<<<<<<< current
  AddPart (Pad);
  if (doing_pinout || TEST_FLAG (DISPLAYNAMEFLAG, Pad))
    DrawPadName (Pad);
=======
  if (!Gathering)
    {
      if (doing_pinout)
	gui->set_color (Output.fgGC, PCB->PinColor);
      else if (TEST_FLAG (WARNFLAG | SELECTEDFLAG | FOUNDFLAG, Pad))
	{
	  if (TEST_FLAG (WARNFLAG, Pad))
	    gui->set_color (Output.fgGC, PCB->WarnColor);
	  else if (TEST_FLAG (SELECTEDFLAG, Pad))
	    gui->set_color (Output.fgGC, PCB->PinSelectedColor);
	  else
	    gui->set_color (Output.fgGC, PCB->ConnectedColor);
          hidgl_hack_ignore_stencil (1);
	}
      else if (FRONT (Pad))
	gui->set_color (Output.fgGC, PCB->PinColor);
      else
	gui->set_color (Output.fgGC, PCB->InvisibleObjectsColor);
    }
  DrawPadLowLevel (Output.fgGC, Pad, false, false);
  if (doing_pinout || TEST_FLAG (DISPLAYNAMEFLAG, Pad))
    DrawPadNameLowLevel (Pad);
  hidgl_hack_ignore_stencil (0);
>>>>>>> patched
}

/* ---------------------------------------------------------------------------
 * draws the name of a pad
 */
void
DrawPadName (PadType *Pad)
{
<<<<<<< current
  GatherPadName (Pad);
=======
  if (!Gathering)
    {
      if (TEST_FLAG (SELECTEDFLAG, Pad))
        {
	  gui->set_color (Output.fgGC, PCB->PinSelectedColor);
          hidgl_hack_ignore_stencil (1);
        }
      else if (FRONT (Pad))
	gui->set_color (Output.fgGC, PCB->PinColor);
      else
	gui->set_color (Output.fgGC, PCB->InvisibleObjectsColor);
    }
  DrawPadNameLowLevel (Pad);
  hidgl_hack_ignore_stencil (0);
>>>>>>> patched
}

/* ---------------------------------------------------------------------------
 * draws a line on a layer
 */
void
DrawLine (LayerType *Layer, LineType *Line)
{
<<<<<<< current
  AddPart (Line);
=======
  if (!Gathering)
    {
      if (TEST_FLAG (SELECTEDFLAG | FOUNDFLAG, Line))
	{
	  if (TEST_FLAG (SELECTEDFLAG, Line))
	    gui->set_color (Output.fgGC, Layer->SelectedColor);
	  else
	    gui->set_color (Output.fgGC, PCB->ConnectedColor);
          hidgl_hack_ignore_stencil (1);
	}
      else
	gui->set_color (Output.fgGC, Layer->Color);
    }
  DrawLineLowLevel (Line);
  hidgl_hack_ignore_stencil (0);
>>>>>>> patched
}

/* ---------------------------------------------------------------------------
 * draws a ratline
 */
void
DrawRat (RatType *Rat)
{
<<<<<<< current
  if (Settings.RatThickness < 100)
    Rat->Thickness = pixel_slop * Settings.RatThickness;
=======
  if (!Gathering)
    {
      if (TEST_FLAG (SELECTEDFLAG | FOUNDFLAG, Line))
	{
	  if (TEST_FLAG (SELECTEDFLAG, Line))
	    gui->set_color (Output.fgGC, PCB->RatSelectedColor);
	  else
	    gui->set_color (Output.fgGC, PCB->ConnectedColor);
          hidgl_hack_ignore_stencil (1);
	}
      else
	gui->set_color (Output.fgGC, PCB->RatColor);
    }
  if (Settings.RatThickness < 20)
    Line->Thickness = pixel_slop * Settings.RatThickness;
>>>>>>> patched
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
<<<<<<< current
    DrawLine (NULL, (LineType *)Rat);
=======
    DrawLineLowLevel ((LineTypePtr) Line);
  hidgl_hack_ignore_stencil (0);
>>>>>>> patched
}

/* ---------------------------------------------------------------------------
 * draws an arc on a layer
 */
void
DrawArc (LayerType *Layer, ArcType *Arc)
{
<<<<<<< current
  AddPart (Arc);
=======
  if (!Arc->Thickness)
    return;
  if (!Gathering)
    {
      if (TEST_FLAG (SELECTEDFLAG | FOUNDFLAG, Arc))
	{
	  if (TEST_FLAG (SELECTEDFLAG, Arc))
	    gui->set_color (Output.fgGC, Layer->SelectedColor);
	  else
	    gui->set_color (Output.fgGC, PCB->ConnectedColor);
          hidgl_hack_ignore_stencil (1);
	}
      else
	gui->set_color (Output.fgGC, Layer->Color);
    }
  DrawArcLowLevel (Arc);
  hidgl_hack_ignore_stencil (0);
>>>>>>> patched
}

/* ---------------------------------------------------------------------------
 * draws a text on a layer
 */
void
DrawText (LayerType *Layer, TextType *Text)
{
<<<<<<< current
  AddPart (Text);
}

=======
  int min_silk_line;
  if (!Layer->On)
    return;
  if (TEST_FLAG (SELECTEDFLAG, Text))
    {
      gui->set_color (Output.fgGC, Layer->SelectedColor);
      hidgl_hack_ignore_stencil (1);
    }
  else
    gui->set_color (Output.fgGC, Layer->Color);
  if (Layer == & PCB->Data->SILKLAYER
      || Layer == & PCB->Data->BACKSILKLAYER)
    min_silk_line = PCB->minSlk;
  else
    min_silk_line = PCB->minWid;
  DrawTextLowLevel (Text, min_silk_line);
  hidgl_hack_ignore_stencil (0);
}

/* ---------------------------------------------------------------------------
 * draws text on a layer
 */
/* static */ void
DrawRegularText (LayerTypePtr Layer, TextTypePtr Text, int unused)
{
  int min_silk_line;
  if (TEST_FLAG (SELECTEDFLAG, Text))
    {
      gui->set_color (Output.fgGC, Layer->SelectedColor);
      hidgl_hack_ignore_stencil (1);
    }
  else
    gui->set_color (Output.fgGC, Layer->Color);
  if (Layer == & PCB->Data->SILKLAYER
      || Layer == & PCB->Data->BACKSILKLAYER)
    min_silk_line = PCB->minSlk;
  else
    min_silk_line = PCB->minWid;
  DrawTextLowLevel (Text, min_silk_line);
  hidgl_hack_ignore_stencil (0);
}
>>>>>>> patched

/* ---------------------------------------------------------------------------
 * draws a polygon on a layer
 */
void
<<<<<<< current
DrawPolygon (LayerType *Layer, PolygonType *Polygon)
{
  AddPart (Polygon);
=======
DrawPolygon (LayerTypePtr Layer, PolygonTypePtr Polygon, int unused)
{
  DrawPolygonLowLevel (Polygon);
  hidgl_hack_ignore_stencil (0);
}

int
thin_callback (PLINE * pl, LayerTypePtr lay, PolygonTypePtr poly)
{
  int i, *x, *y;
  VNODE *v;

  i = 0;
  x = (int *) malloc (pl->Count * sizeof (int));
  y = (int *) malloc (pl->Count * sizeof (int));
  for (v = &pl->head; i < pl->Count; v = v->next)
    {
      x[i] = v->point[0];
      y[i++] = v->point[1];
    }
  gui->set_line_cap (Output.fgGC, Round_Cap);
  gui->set_line_width (Output.fgGC, 0);
  for (i = 0; i < pl->Count - 1; i++)
    {
      gui->draw_line (Output.fgGC, x[i], y[i], x[i + 1], y[i + 1]);
      //  gui->fill_circle (Output.fgGC, x[i], y[i], 30);
    }
  gui->draw_line (Output.fgGC, x[pl->Count - 1], y[pl->Count - 1], x[0],
		  y[0]);
  free (x);
  free (y);
  return 0;
}


/* ---------------------------------------------------------------------------
 * draws a polygon
 */
static void
DrawPlainPolygon (LayerTypePtr Layer, PolygonTypePtr Polygon)
{
  static char *color;

  if (!Polygon->Clipped)
    return;

  if (Gathering)
    {
      AddPart (Polygon);
      return;
    }

  if (TEST_FLAG (SELECTEDFLAG, Polygon))
    {
      color = Layer->SelectedColor;
      hidgl_hack_ignore_stencil (1);
    }
  else if (TEST_FLAG (FOUNDFLAG, Polygon))
    color = PCB->ConnectedColor;
  else
    color = Layer->Color;
  gui->set_color (Output.fgGC, color);

  if (gui->thindraw_pcb_polygon != NULL &&
      (TEST_FLAG (THINDRAWFLAG, PCB) ||
       TEST_FLAG (THINDRAWPOLYFLAG, PCB)))
    gui->thindraw_pcb_polygon (Output.fgGC, Polygon, clip_box);
  else
    gui->fill_pcb_polygon (Output.fgGC, Polygon, clip_box);

  /* If checking planes, thin-draw any pieces which have been clipped away */
  if (gui->thindraw_pcb_polygon != NULL &&
      TEST_FLAG (CHECKPLANESFLAG, PCB) &&
      !TEST_FLAG (FULLPOLYFLAG, Polygon))
    {
      PolygonType poly = *Polygon;

      for (poly.Clipped = Polygon->Clipped->f;
           poly.Clipped != Polygon->Clipped;
           poly.Clipped = poly.Clipped->f)
        gui->thindraw_pcb_polygon (Output.fgGC, &poly, clip_box);
    }
  hidgl_hack_ignore_stencil (0);
>>>>>>> patched
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
<<<<<<< current
  DrawText (NULL, &ELEMENT_TEXT (PCB, Element));
=======
  if (doing_pinout || doing_assy)
    gui->set_color (Output.fgGC, PCB->ElementColor);
  else if (TEST_FLAG (SELECTEDFLAG, &ELEMENT_TEXT (PCB, Element)))
    {
      gui->set_color (Output.fgGC, PCB->ElementSelectedColor);
      hidgl_hack_ignore_stencil (1);
    }
  else if (FRONT (Element))
    gui->set_color (Output.fgGC, PCB->ElementColor);
  else
    gui->set_color (Output.fgGC, PCB->InvisibleObjectsColor);
  DrawStrippedText (Element, PCB->minSlk);
  hidgl_hack_ignore_stencil (0);
>>>>>>> patched
}

/* ---------------------------------------------------------------------------
 * draws the package of an element
 */
void
DrawElementPackage (ElementType *Element)
{
<<<<<<< current
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
=======
  /* set color and draw lines, arcs, text and pins */
  if (doing_pinout || doing_assy)
      gui->set_color (Output.fgGC, PCB->ElementColor);
  else if (TEST_FLAG (SELECTEDFLAG, Element))
    {
      gui->set_color (Output.fgGC, PCB->ElementSelectedColor);
      hidgl_hack_ignore_stencil (1);
    }
  else if (FRONT (Element))
    gui->set_color (Output.fgGC, PCB->ElementColor);
  else
    gui->set_color (Output.fgGC, PCB->InvisibleObjectsColor);
  DrawElementPackageLowLevel (Element, unused);
  hidgl_hack_ignore_stencil (0);
>>>>>>> patched
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
draw_element (ElementType *element)
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
  Output.fgGC = hid_draw_make_gc (hid->graphics);
  Output.bgGC = hid_draw_make_gc (hid->graphics);
  Output.pmGC = hid_draw_make_gc (hid->graphics);

  hid->graphics->set_color (Output.pmGC, "erase");
  hid->graphics->set_color (Output.bgGC, "drill");

  if (item)
    {
      doing_pinout = true;
      draw_element ((ElementType *)item);
      doing_pinout = false;
    }
  else
    DrawEverything (region);

  hid_draw_destroy_gc (Output.fgGC);
  hid_draw_destroy_gc (Output.bgGC);
  hid_draw_destroy_gc (Output.pmGC);
  gui = old_gui;
}
