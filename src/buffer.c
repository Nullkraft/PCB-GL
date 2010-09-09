/* $Id$ */

/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996, 2005 Thomas Nau
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

/* functions used by paste- and move/copy buffer
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <memory.h>
#include <math.h>

#include "global.h"

#include "buffer.h"
#include "copy.h"
#include "create.h"
#include "crosshair.h"
#include "data.h"
#include "error.h"
#include "mymem.h"
#include "mirror.h"
#include "misc.h"
#include "parse_l.h"
#include "polygon.h"
#include "pour.h"
#include "rats.h"
#include "rotate.h"
#include "remove.h"
#include "rtree.h"
#include "search.h"
#include "select.h"
#include "set.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

RCSID ("$Id$");

/* ---------------------------------------------------------------------------
 * some local prototypes
 */
static void *AddViaToBuffer (PinTypePtr);
static void *AddLineToBuffer (LayerTypePtr, LineTypePtr);
static void *AddArcToBuffer (LayerTypePtr, ArcTypePtr);
static void *AddRatToBuffer (RatTypePtr);
static void *AddTextToBuffer (LayerTypePtr, TextTypePtr);
static void *AddPourToBuffer (LayerTypePtr, PourTypePtr);
static void *AddElementToBuffer (ElementTypePtr);
static void *MoveViaToBuffer (PinTypePtr);
static void *MoveLineToBuffer (LayerTypePtr, LineTypePtr);
static void *MoveArcToBuffer (LayerTypePtr, ArcTypePtr);
static void *MoveRatToBuffer (RatTypePtr);
static void *MoveTextToBuffer (LayerTypePtr, TextTypePtr);
static void *MovePourToBuffer (LayerTypePtr, PourTypePtr);
static void *MoveElementToBuffer (ElementTypePtr);
static void SwapBuffer (BufferTypePtr);

#define ARG(n) (argc > (n) ? argv[n] : 0)

/* ---------------------------------------------------------------------------
 * some local identifiers
 */
static DataTypePtr Dest, Source;

static ObjectFunctionType AddBufferFunctions = {
  AddLineToBuffer,
  AddTextToBuffer,
  NULL,
  AddPourToBuffer,
  AddViaToBuffer,
  AddElementToBuffer,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  AddArcToBuffer,
  AddRatToBuffer
}, MoveBufferFunctions =

{
MoveLineToBuffer,
    MoveTextToBuffer,
    NULL,
    MovePourToBuffer,
    MoveViaToBuffer,
    MoveElementToBuffer,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    MoveArcToBuffer,
    MoveRatToBuffer};

static int ExtraFlag = 0;

/* ---------------------------------------------------------------------------
 * copies a via to paste buffer
 */
static void *
AddViaToBuffer (PinTypePtr Via)
{
  return (CreateNewVia (Dest, Via->X, Via->Y, Via->Thickness, Via->Clearance,
			Via->Mask, Via->DrillingHole, Via->Name,
			MaskFlags (Via->Flags, FOUNDFLAG | ExtraFlag)));
}

/* ---------------------------------------------------------------------------
 * copies a rat-line to paste buffer
 */
static void *
AddRatToBuffer (RatTypePtr Rat)
{
  return (CreateNewRat (Dest, Rat->Point1.X, Rat->Point1.Y,
			Rat->Point2.X, Rat->Point2.Y, Rat->group1,
			Rat->group2, Rat->Thickness,
			MaskFlags (Rat->Flags, FOUNDFLAG | ExtraFlag)));
}

/* ---------------------------------------------------------------------------
 * copies a line to buffer  
 */
static void *
AddLineToBuffer (LayerTypePtr Layer, LineTypePtr Line)
{
  LineTypePtr line;
  LayerTypePtr layer = &Dest->Layer[GetLayerNumber (Source, Layer)];

  line = CreateNewLineOnLayer (layer, Line->Point1.X, Line->Point1.Y,
			       Line->Point2.X, Line->Point2.Y,
			       Line->Thickness, Line->Clearance,
			       MaskFlags (Line->Flags,
					  FOUNDFLAG | ExtraFlag));
  if (line && Line->Number)
    line->Number = MyStrdup (Line->Number, "AddLineToBuffer");
  return (line);
}

/* ---------------------------------------------------------------------------
 * copies an arc to buffer  
 */
static void *
AddArcToBuffer (LayerTypePtr Layer, ArcTypePtr Arc)
{
  LayerTypePtr layer = &Dest->Layer[GetLayerNumber (Source, Layer)];

  return (CreateNewArcOnLayer (layer, Arc->X, Arc->Y,
			       Arc->Width, Arc->Height, Arc->StartAngle, Arc->Delta,
			       Arc->Thickness, Arc->Clearance,
			       MaskFlags (Arc->Flags,
					  FOUNDFLAG | ExtraFlag)));
}

/* ---------------------------------------------------------------------------
 * copies a text to buffer
 */
static void *
AddTextToBuffer (LayerTypePtr Layer, TextTypePtr Text)
{
  LayerTypePtr layer = &Dest->Layer[GetLayerNumber (Source, Layer)];

  return (CreateNewText (layer, &PCB->Font, Text->X, Text->Y,
			 Text->Direction, Text->Scale, Text->TextString,
			 MaskFlags (Text->Flags, ExtraFlag)));
}

/* ---------------------------------------------------------------------------
 * copies a pour to buffer
 */
static void *
AddPourToBuffer (LayerTypePtr Layer, PourTypePtr Pour)
{
  LayerTypePtr layer = &Dest->Layer[GetLayerNumber (Source, Layer)];
  PourTypePtr pour;

  pour = CreateNewPour (layer, Pour->Flags);
  CopyPourLowLevel (pour, Pour);
  CLEAR_FLAG (FOUNDFLAG | ExtraFlag, pour);
  return (pour);
}

/* ---------------------------------------------------------------------------
 * copies a element to buffer
 */
static void *
AddElementToBuffer (ElementTypePtr Element)
{
  ElementTypePtr element;

  element = GetElementMemory (Dest);
  CopyElementLowLevel (Dest, element, Element, false, 0, 0);
  CLEAR_FLAG (ExtraFlag, element);
  if (ExtraFlag)
    {
      ELEMENTTEXT_LOOP (element);
      {
	CLEAR_FLAG (ExtraFlag, text);
      }
      END_LOOP;
      PIN_LOOP (element);
      {
	CLEAR_FLAG (FOUNDFLAG | ExtraFlag, pin);
      }
      END_LOOP;
      PAD_LOOP (element);
      {
	CLEAR_FLAG (FOUNDFLAG | ExtraFlag, pad);
      }
      END_LOOP;
    }
  return (element);
}

/* ---------------------------------------------------------------------------
 * moves a via to paste buffer without allocating memory for the name
 */
static void *
MoveViaToBuffer (PinTypePtr Via)
{
  PinTypePtr via;

  RestoreToPours (Source, VIA_TYPE, Via, Via);
  r_delete_entry (Source->via_tree, (BoxType *) Via);
  via = GetViaMemory (Dest);
  *via = *Via;
  CLEAR_FLAG (WARNFLAG | FOUNDFLAG, via);
  if (Via != &Source->Via[--Source->ViaN])
  {
  *Via = Source->Via[Source->ViaN];
  r_substitute (Source->via_tree, (BoxType *) & Source->Via[Source->ViaN],
		(BoxType *) Via);
  }
  memset (&Source->Via[Source->ViaN], 0, sizeof (PinType));
  if (!Dest->via_tree)
    Dest->via_tree = r_create_tree (NULL, 0, 0);
  r_insert_entry (Dest->via_tree, (BoxType *) via, 0);
  ClearFromPours (Dest, VIA_TYPE, via, via);
  return (via);
}

/* ---------------------------------------------------------------------------
 * moves a rat-line to paste buffer
 */
static void *
MoveRatToBuffer (RatTypePtr Rat)
{
  RatTypePtr rat;

  r_delete_entry (Source->rat_tree, &Rat->BoundingBox);
  rat = GetRatMemory (Dest);
  *rat = *Rat;
  CLEAR_FLAG (FOUNDFLAG, rat);
  if (Rat != &Source->Rat[--Source->RatN])
  {
  *Rat = Source->Rat[Source->RatN];
  r_substitute (Source->rat_tree, &Source->Rat[Source->RatN].BoundingBox,
		&Rat->BoundingBox);
  }
  memset (&Source->Rat[Source->RatN], 0, sizeof (RatType));
  if (!Dest->rat_tree)
    Dest->rat_tree = r_create_tree (NULL, 0, 0);
  r_insert_entry (Dest->rat_tree, &rat->BoundingBox, 0);
  return (rat);
}

/* ---------------------------------------------------------------------------
 * moves a line to buffer  
 */
static void *
MoveLineToBuffer (LayerTypePtr Layer, LineTypePtr Line)
{
  LayerTypePtr lay;
  LineTypePtr line;

  RestoreToPours (Source, LINE_TYPE, Layer, Line);
  r_delete_entry (Layer->line_tree, (BoxTypePtr) Line);
  lay = &Dest->Layer[GetLayerNumber (Source, Layer)];
  line = GetLineMemory (lay);
  *line = *Line;
  CLEAR_FLAG (FOUNDFLAG, line);
  /* line pointers being shuffled */
  if (Line != &Layer->Line[--Layer->LineN])
  {
  *Line = Layer->Line[Layer->LineN];
  r_substitute (Layer->line_tree, (BoxTypePtr) & Layer->Line[Layer->LineN],
		(BoxTypePtr) Line);
  }
  memset (&Layer->Line[Layer->LineN], 0, sizeof (LineType));
  if (!lay->line_tree)
    lay->line_tree = r_create_tree (NULL, 0, 0);
  r_insert_entry (lay->line_tree, (BoxTypePtr) line, 0);
  ClearFromPours (Dest, LINE_TYPE, lay, line);
  return (line);
}

/* ---------------------------------------------------------------------------
 * moves an arc to buffer  
 */
static void *
MoveArcToBuffer (LayerTypePtr Layer, ArcTypePtr Arc)
{
  LayerTypePtr lay;
  ArcTypePtr arc;

  RestoreToPours (Source, ARC_TYPE, Layer, Arc);
  r_delete_entry (Layer->arc_tree, (BoxTypePtr) Arc);
  lay = &Dest->Layer[GetLayerNumber (Source, Layer)];
  arc = GetArcMemory (lay);
  *arc = *Arc;
  CLEAR_FLAG (FOUNDFLAG, arc);
  /* arc pointers being shuffled */
  if (Arc != &Layer->Arc[--Layer->ArcN])
  {
  *Arc = Layer->Arc[Layer->ArcN];
  r_substitute (Layer->arc_tree, (BoxTypePtr) & Layer->Arc[Layer->ArcN],
		(BoxTypePtr) Arc);
  }
  memset (&Layer->Arc[Layer->ArcN], 0, sizeof (ArcType));
  if (!lay->arc_tree)
    lay->arc_tree = r_create_tree (NULL, 0, 0);
  r_insert_entry (lay->arc_tree, (BoxTypePtr) arc, 0);
  ClearFromPours (Dest, ARC_TYPE, lay, arc);
  return (arc);
}

/* ---------------------------------------------------------------------------
 * moves a text to buffer without allocating memory for the name
 */
static void *
MoveTextToBuffer (LayerTypePtr Layer, TextTypePtr Text)
{
  TextTypePtr text;
  LayerTypePtr lay;

  r_delete_entry (Layer->text_tree, (BoxTypePtr) Text);
  RestoreToPours (Source, TEXT_TYPE, Layer, Text);
  lay = &Dest->Layer[GetLayerNumber (Source, Layer)];
  text = GetTextMemory (lay);
  *text = *Text;
  if (Text != &Layer->Text[--Layer->TextN])
  {
  *Text = Layer->Text[Layer->TextN];
  r_substitute (Layer->text_tree, (BoxTypePtr) & Layer->Text[Layer->TextN],
		(BoxTypePtr) Text);
  }
  memset (&Layer->Text[Layer->TextN], 0, sizeof (TextType));
  if (!lay->text_tree)
    lay->text_tree = r_create_tree (NULL, 0, 0);
  r_insert_entry (lay->text_tree, (BoxTypePtr) text, 0);
  ClearFromPours (Dest, TEXT_TYPE, lay, text);
  return (text);
}

/* ---------------------------------------------------------------------------
 * moves a pour to buffer. Doesn't allocate memory for the points
 */
static void *
MovePourToBuffer (LayerTypePtr Layer, PourTypePtr Pour)
{
  LayerTypePtr lay;
  PourTypePtr pour;
  Cardinal i;

  r_delete_entry (Layer->pour_tree, (BoxTypePtr) Pour);
  lay = &Dest->Layer[GetLayerNumber (Source, Layer)];
  pour = GetPourMemory (lay);
  *pour = *Pour;
  CLEAR_FLAG (FOUNDFLAG, pour);
  if (Pour != &Layer->Pour[--Layer->PourN])
    {
      *Pour = Layer->Pour[Layer->PourN];
      r_substitute (Layer->pour_tree,
                    (BoxTypePtr) & Layer->Pour[Layer->PourN],
                    (BoxTypePtr) Pour);
      for (i = 0; i < Pour->PolygonN; i++)
        Pour->Polygons[i].ParentPour = Pour;
    }
  memset (&Layer->Pour[Layer->PourN], 0, sizeof (PourType));
  if (!lay->pour_tree)
    lay->pour_tree = r_create_tree (NULL, 0, 0);
  r_insert_entry (lay->pour_tree, (BoxTypePtr) pour, 0);
  return (pour);
}

/* ---------------------------------------------------------------------------
 * moves a element to buffer without allocating memory for pins/names
 */
static void *
MoveElementToBuffer (ElementTypePtr Element)
{
  ElementTypePtr element;
  int i;

  /*
   * Two steps at once:  Delete the element from the source (remove it
   * from trees, restore to polygons) and simultaneously adjust its
   * component pointers to the new storage in Dest
   */
  r_delete_element (Source, Element);
  element = GetElementMemory (Dest);
  *element = *Element;
  PIN_LOOP (element);
  {
    RestoreToPours(Source, PIN_TYPE, Element, pin);
    CLEAR_FLAG (WARNFLAG | FOUNDFLAG, pin);
    pin->Element = element;
  }
  END_LOOP;
  PAD_LOOP (element);
  {
    RestoreToPours(Source, PAD_TYPE, Element, pad);
    CLEAR_FLAG (WARNFLAG | FOUNDFLAG, pad);
    pad->Element = element;
  }
  END_LOOP;
  ELEMENTTEXT_LOOP (element);
  {
    text->Element = element;
  }
  END_LOOP;
  SetElementBoundingBox (Dest, element, &PCB->Font);
  /*
   * Now clear the from the polygons in the destination
   */
  PIN_LOOP (element);
  {
    ClearFromPours (Dest, PIN_TYPE, element, pin);
  }
  END_LOOP;
  PAD_LOOP (element);
  {
    ClearFromPours (Dest, PAD_TYPE, element, pad);
  }
  END_LOOP;

  /*
   * Now compact the Source's Element array by moving the last element
   * to the hole created by the removal above.  Then make a pass adjusting
   * *its* component pointers.  Confusingly, this element (which is of no
   * particular relation to this removal) becomes `Element' while the
   * original Element is now in `element'.
   */
  if (Element != &Source->Element[--Source->ElementN])
    {
  *Element = Source->Element[Source->ElementN];
  r_substitute (Source->element_tree,
		(BoxType *) & Source->Element[Source->ElementN],
		(BoxType *) Element);
  for (i = 0; i < MAX_ELEMENTNAMES; i++)
    r_substitute (Source->name_tree[i],
		  (BoxType *) & Source->Element[Source->ElementN].Name[i],
		  (BoxType *) & Element->Name[i]);
  ELEMENTTEXT_LOOP (Element);
  {
    text->Element = Element;
  }
  END_LOOP;
  PIN_LOOP (Element);
  {
    pin->Element = Element;
  }
  END_LOOP;
  PAD_LOOP (Element);
  {
    pad->Element = Element;
  }
  END_LOOP;
  }
  memset (&Source->Element[Source->ElementN], 0, sizeof (ElementType));
  return (element);
}

/* ---------------------------------------------------------------------------
 * calculates the bounding box of the buffer
 */
void
SetBufferBoundingBox (BufferTypePtr Buffer)
{
  BoxTypePtr box = GetDataBoundingBox (Buffer->Data);

  if (box)
    Buffer->BoundingBox = *box;
}

/* ---------------------------------------------------------------------------
 * clears the contents of the paste buffer
 */
void
ClearBuffer (BufferTypePtr Buffer)
{
  if (Buffer && Buffer->Data)
    {
      FreeDataMemory (Buffer->Data);
      Buffer->Data->pcb = PCB;
    }
}

/* ----------------------------------------------------------------------
 * copies all selected and visible objects to the paste buffer
 * returns true if any objects have been removed
 */
void
AddSelectedToBuffer (BufferTypePtr Buffer, LocationType X, LocationType Y,
		     bool LeaveSelected)
{
  /* switch crosshair off because adding objects to the pastebuffer
   * may change the 'valid' area for the cursor
   */
  if (!LeaveSelected)
    ExtraFlag = SELECTEDFLAG;
  HideCrosshair (true);
  Source = PCB->Data;
  Dest = Buffer->Data;
  SelectedOperation (&AddBufferFunctions, false, ALL_TYPES);

  /* set origin to passed or current position */
  if (X || Y)
    {
      Buffer->X = X;
      Buffer->Y = Y;
    }
  else
    {
      Buffer->X = Crosshair.X;
      Buffer->Y = Crosshair.Y;
    }
  RestoreCrosshair (true);
  ExtraFlag = 0;
}

/* ---------------------------------------------------------------------------
 * loads element data from file/library into buffer
 * parse the file with disabled 'PCB mode' (see parser)
 * returns false on error
 * if successful, update some other stuff and reposition the pastebuffer
 */
bool
LoadElementToBuffer (BufferTypePtr Buffer, char *Name, bool FromFile)
{
  ElementTypePtr element;

  ClearBuffer (Buffer);
  if (FromFile)
    {
      if (!ParseElementFile (Buffer->Data, Name))
	{
	  if (Settings.ShowSolderSide)
	    SwapBuffer (Buffer);
	  SetBufferBoundingBox (Buffer);
	  if (Buffer->Data->ElementN)
	    {
	      element = &(Buffer->Data->Element[0]);
	      Buffer->X = element->MarkX;
	      Buffer->Y = element->MarkY;
	    }
	  else
	    {
	      Buffer->X = 0;
	      Buffer->Y = 0;
	    }
	  return (true);
	}
    }
  else
    {
      if (!ParseLibraryEntry (Buffer->Data, Name)
	  && Buffer->Data->ElementN != 0)
	{
	  element = &(Buffer->Data->Element[0]);

	  /* always add elements using top-side coordinates */
	  if (Settings.ShowSolderSide)
	    MirrorElementCoordinates (Buffer->Data, element, 0);
	  SetElementBoundingBox (Buffer->Data, element, &PCB->Font);

	  /* set buffer offset to 'mark' position */
	  Buffer->X = element->MarkX;
	  Buffer->Y = element->MarkY;
	  SetBufferBoundingBox (Buffer);
	  return (true);
	}
    }
  /* release memory which might have been acquired */
  ClearBuffer (Buffer);
  return (false);
}


/*---------------------------------------------------------------------------
 * Searches for the given element by "footprint" name, and loads it
 * into the buffer.
 */

/* Figuring out which library entry is the one we want is a little
   tricky.  For file-based footprints, it's just a matter of finding
   the first match in the search list.  For m4-based footprints you
   need to know what magic to pass to the m4 functions.  Fortunately,
   the footprint needed is determined when we build the m4 libraries
   and stored as a comment in the description, so we can search for
   that to find the magic we need.  We use a hash to store the
   corresponding footprints and pointers to the library tree so we can
   quickly find the various bits we need to load a given
   footprint.  */

typedef struct {
  char *footprint;
  int footprint_allocated;
  int menu_idx;
  int entry_idx;
} FootprintHashEntry;

static FootprintHashEntry *footprint_hash = 0;
int footprint_hash_size = 0;

void
clear_footprint_hash ()
{
  int i;
  if (!footprint_hash)
    return;
  for (i=0; i<footprint_hash_size; i++)
    if (footprint_hash[i].footprint_allocated)
      free (footprint_hash[i].footprint);
  free (footprint_hash);
  footprint_hash = NULL;
  footprint_hash_size = 0;
}

/* Used to sort footprint pointer entries.  Note we include the index
   numbers so that same-named footprints are sorted by the library
   search order.  */
static int
footprint_hash_cmp (const void *va, const void *vb)
{
  int i;
  FootprintHashEntry *a = (FootprintHashEntry *)va;
  FootprintHashEntry *b = (FootprintHashEntry *)vb;

  i = strcmp (a->footprint, b->footprint);
  if (i == 0)
    i = a->menu_idx - b->menu_idx;
  if (i == 0)
    i = a->entry_idx - b->entry_idx;
  return i;
}

void
make_footprint_hash ()
{
  int i, j;
  char *fp;
  int num_entries = 0;

  clear_footprint_hash ();

  for (i=0; i<Library.MenuN; i++)
    for (j=0; j<Library.Menu[i].EntryN; j++)
      num_entries ++;
  footprint_hash = (FootprintHashEntry *)malloc (num_entries * sizeof(FootprintHashEntry));
  num_entries = 0;

  /* There are two types of library entries.  The file-based types
     have a Template of (char *)-1 and the AllocatedMemory is the full
     path to the footprint file.  The m4 ones have the footprint name
     in brackets in the description.  */
  for (i=0; i<Library.MenuN; i++)
    {
#ifdef DEBUG
  printf("In make_footprint_hash, looking for footprints in %s\n", 
	 Library.Menu[i].directory);
#endif

    for (j=0; j<Library.Menu[i].EntryN; j++)
	{
	  footprint_hash[num_entries].menu_idx = i;
	  footprint_hash[num_entries].entry_idx = j;
	  if (Library.Menu[i].Entry[j].Template == (char *) -1) 
          /* file */
	    {
#ifdef DEBUG
/*	      printf(" ... Examining file %s\n", Library.Menu[i].Entry[j].AllocatedMemory); */
#endif
	      fp = strrchr (Library.Menu[i].Entry[j].AllocatedMemory, '/');

	      if (!fp)
		fp = strrchr (Library.Menu[i].Entry[j].AllocatedMemory, '\\');

	      if (fp)
		fp ++;
	      else 
		fp = Library.Menu[i].Entry[j].AllocatedMemory;

#ifdef DEBUG
/* 	      printf(" ... found file footprint %s\n",  fp); */
#endif
	        
	      footprint_hash[num_entries].footprint = fp;
	      footprint_hash[num_entries].footprint_allocated = 0;
	    }
	  else 
          /* m4 */
	    {
	      fp = strrchr (Library.Menu[i].Entry[j].Description, '[');
	      if (fp)
		{
		  footprint_hash[num_entries].footprint = strdup (fp+1);
		  footprint_hash[num_entries].footprint_allocated = 1;
		  fp = strchr (footprint_hash[num_entries].footprint, ']');
		  if (fp)
		    *fp = 0;
		}
	      else
		{
		  fp = Library.Menu[i].Entry[j].Description;
		  footprint_hash[num_entries].footprint = fp;
		  footprint_hash[num_entries].footprint_allocated = 0;
		}
	    }
	  num_entries ++;
	}
    }

  footprint_hash_size = num_entries;
  qsort (footprint_hash, num_entries, sizeof(footprint_hash[0]), footprint_hash_cmp);
/*
#ifdef DEBUG
  printf("       found footprints:  \n");
  for (i=0; i<num_entries; i++)
    printf("[%s]\n", footprint_hash[i].footprint);
#endif
*/
}

FootprintHashEntry *
search_footprint_hash (const char *footprint)
{
  int i, min, max, c;

  /* Standard binary search */

  min = -1;
  max = footprint_hash_size;

  while (max - min > 1)
    {
      i = (min+max)/2;
      c = strcmp (footprint, footprint_hash[i].footprint);
      if (c < 0)
	max = i;
      else if (c > 0)
	min = i;
      else
	{
	  /* We want to return the first match, not just any match.  */
	  while (i > 0
		 && strcmp (footprint, footprint_hash[i-1].footprint) == 0)
	    i--;
	  return & footprint_hash[i];
	}
    }
  return NULL;
}

/* Returns zero on success, non-zero on error.  */
int
LoadFootprintByName (BufferTypePtr Buffer, char *Footprint)
{
  int i;
  FootprintHashEntry *fpe;
  LibraryMenuType *menu;
  LibraryEntryType *entry;
  char *with_fp = NULL;

  if (!footprint_hash)
    make_footprint_hash ();

  fpe = search_footprint_hash (Footprint);
  if (!fpe)
    {
      with_fp = Concat (Footprint, ".fp", NULL);
      fpe = search_footprint_hash (with_fp);
      if (fpe)
	Footprint = with_fp;
    }
  if (!fpe)
    {
      Message("Unable to load footprint %s\n", Footprint);
      return 1;
    }

  menu = & Library.Menu[fpe->menu_idx];
  entry = & menu->Entry[fpe->entry_idx];

  if (entry->Template == (char *) -1)
    {
      i = LoadElementToBuffer (Buffer, entry->AllocatedMemory, true);
      if (with_fp)
	free (with_fp);
      return i ? 0 : 1;
    }
  else
    {
      char *args;

      args = Concat("'", EMPTY (entry->Template), "' '",
		    EMPTY (entry->Value), "' '", EMPTY (entry->Package), "'", NULL);
      i = LoadElementToBuffer (Buffer, args, false);

      free (args);
      if (with_fp)
	free (with_fp);
      return i ? 0 : 1;
    }

#ifdef DEBUG
  {
    int j;
    printf("Library path: %s\n", Settings.LibraryPath);
    printf("Library tree: %s\n", Settings.LibraryTree);

    printf("Library:\n");
    for (i=0; i<Library.MenuN; i++)
      {
	printf("  [%02d] Name: %s\n", i, Library.Menu[i].Name);
	printf("       Dir:  %s\n", Library.Menu[i].directory);
	printf("       Sty:  %s\n", Library.Menu[i].Style);
	for (j=0; j<Library.Menu[i].EntryN; j++)
	  {
	    printf("       [%02d] E: %s\n", j, Library.Menu[i].Entry[j].ListEntry);
	    if (Library.Menu[i].Entry[j].Template == (char *) -1)
	      printf("            A: %s\n", Library.Menu[i].Entry[j].AllocatedMemory);
	    else
	      {
		printf("            T: %s\n", Library.Menu[i].Entry[j].Template);
		printf("            P: %s\n", Library.Menu[i].Entry[j].Package);
		printf("            V: %s\n", Library.Menu[i].Entry[j].Value);
		printf("            D: %s\n", Library.Menu[i].Entry[j].Description);
	      }
	    if (j == 10)
	      break;
	  }
      }
  }
#endif
}


static const char loadfootprint_syntax[] = "LoadFootprint(filename[,refdes,value])";

static const char loadfootprint_help[] = "Loads a single footprint by name";

/* %start-doc actions LoadFootprint

Loads a single footprint by name, rather than by reference or through
the library.  If a refdes and value are specified, those are inserted
into the footprint as well.  The footprint remains in the paste buffer.

%end-doc */

int
LoadFootprint (int argc, char **argv, int x, int y)
{
  char *name = ARG(0);
  char *refdes = ARG(1);
  char *value = ARG(2);
  ElementTypePtr e;

  if (!name)
    AFAIL (loadfootprint);

  if (LoadFootprintByName (PASTEBUFFER, name))
    return 1;

  if (PASTEBUFFER->Data->ElementN == 0)
    {
      Message("Footprint %s contains no elements", name);
      return 1;
    }
  if (PASTEBUFFER->Data->ElementN > 1)
    {
      Message("Footprint %s contains multiple elements", name);
      return 1;
    }

  e = & PASTEBUFFER->Data->Element[0];

  if (e->Name[0].TextString)
    free (e->Name[0].TextString);
  e->Name[0].TextString = strdup (name);

  if (e->Name[1].TextString)
    free (e->Name[1].TextString);
  e->Name[1].TextString = refdes ? strdup (refdes) : 0;

  if (e->Name[2].TextString)
    free (e->Name[2].TextString);
  e->Name[2].TextString = value ? strdup (value) : 0;

  return 0;
}

/*---------------------------------------------------------------------------
 *
 * break buffer element into pieces
 */
bool
SmashBufferElement (BufferTypePtr Buffer)
{
  ElementTypePtr element;
  Cardinal group;
  LayerTypePtr clayer, slayer;

  if (Buffer->Data->ElementN != 1)
    {
      Message (_("Error!  Buffer doesn't contain a single element\n"));
      return (false);
    }
  element = &Buffer->Data->Element[0];
  Buffer->Data->ElementN = 0;
  ClearBuffer (Buffer);
  ELEMENTLINE_LOOP (element);
  {
    CreateNewLineOnLayer (&Buffer->Data->SILKLAYER,
			  line->Point1.X, line->Point1.Y,
			  line->Point2.X, line->Point2.Y,
			  line->Thickness, 0, NoFlags ());
    if (line)
      line->Number = MyStrdup (NAMEONPCB_NAME (element), "SmashBuffer");
  }
  END_LOOP;
  ARC_LOOP (element);
  {
    CreateNewArcOnLayer (&Buffer->Data->SILKLAYER,
			 arc->X, arc->Y, arc->Width, arc->Height, arc->StartAngle,
			 arc->Delta, arc->Thickness, 0, NoFlags ());
  }
  END_LOOP;
  PIN_LOOP (element);
  {
    FlagType f = NoFlags ();
    AddFlags (f, VIAFLAG);
    if (TEST_FLAG (HOLEFLAG, pin))
      AddFlags (f, HOLEFLAG);

    CreateNewVia (Buffer->Data, pin->X, pin->Y,
		  pin->Thickness, pin->Clearance, pin->Mask,
		  pin->DrillingHole, pin->Number, f);
  }
  END_LOOP;
  group =
    GetLayerGroupNumberByNumber (max_layer +
				 (SWAP_IDENT ? SOLDER_LAYER :
				  COMPONENT_LAYER));
  clayer = &Buffer->Data->Layer[PCB->LayerGroups.Entries[group][0]];
  group =
    GetLayerGroupNumberByNumber (max_layer +
				 (SWAP_IDENT ? COMPONENT_LAYER :
				  SOLDER_LAYER));
  slayer = &Buffer->Data->Layer[PCB->LayerGroups.Entries[group][0]];
  PAD_LOOP (element);
  {
    LineTypePtr line;
    line = CreateNewLineOnLayer (TEST_FLAG (ONSOLDERFLAG, pad) ? slayer : clayer,
				 pad->Point1.X, pad->Point1.Y,
				 pad->Point2.X, pad->Point2.Y,
				 pad->Thickness, pad->Clearance, NoFlags ());
    if (line)
      line->Number = MyStrdup (pad->Number, "SmashBuffer");
  }
  END_LOOP;
  FreeElementMemory (element);
  SaveFree (element);
  return (true);
}

#warning FIXME Later
#if 0
/*---------------------------------------------------------------------------
 *
 * see if a polygon is a rectangle.  If so, canonicalize it.
 */

static int
polygon_is_rectangle (PolygonTypePtr poly)
{
  int i, best;
  PointType temp[4];
  if (poly->PointN != 4 || poly->HoleIndexN != 0)
    return 0;
  best = 0;
  for (i=1; i<4; i++)
    if (poly->Points[i].X < poly->Points[best].X
	|| poly->Points[i].Y < poly->Points[best].Y)
      best = i;
  for (i=0; i<4; i++)
    temp[i] = poly->Points[(i+best)%4];
  if (temp[0].X == temp[1].X)
    memcpy (poly->Points, temp, sizeof(temp));
  else
    {
      /* reverse them */
      poly->Points[0] = temp[0];
      poly->Points[1] = temp[3];
      poly->Points[2] = temp[2];
      poly->Points[3] = temp[1];
    }
  if (poly->Points[0].X == poly->Points[1].X
      && poly->Points[1].Y == poly->Points[2].Y
      && poly->Points[2].X == poly->Points[3].X
      && poly->Points[3].Y == poly->Points[0].Y)
    return 1;
  return 0;
}
#endif

/*---------------------------------------------------------------------------
 *
 * convert buffer contents into an element
 */
bool
ConvertBufferToElement (BufferTypePtr Buffer)
{
  ElementTypePtr Element;
  Cardinal group;
  Cardinal pin_n = 1;
  bool hasParts = false, crooked = false;

  if (Buffer->Data->pcb == 0)
    Buffer->Data->pcb = PCB;

  Element = CreateNewElement (PCB->Data, NULL, &PCB->Font, NoFlags (),
			      NULL, NULL, NULL, PASTEBUFFER->X,
			      PASTEBUFFER->Y, 0, 100,
			      MakeFlags (SWAP_IDENT ? ONSOLDERFLAG : NOFLAG),
			      false);
  if (!Element)
    return (false);
  VIA_LOOP (Buffer->Data);
  {
    char num[8];
    if (via->Mask < via->Thickness)
      via->Mask = via->Thickness + 2 * MASKFRAME * 100;	/* MASKFRAME is in mils */
    if (via->Name)
      CreateNewPin (Element, via->X, via->Y, via->Thickness,
		    via->Clearance, via->Mask, via->DrillingHole,
		    NULL, via->Name, MaskFlags (via->Flags,
						VIAFLAG | FOUNDFLAG |
						SELECTEDFLAG | WARNFLAG));
    else
      {
	sprintf (num, "%d", pin_n++);
	CreateNewPin (Element, via->X, via->Y, via->Thickness,
		      via->Clearance, via->Mask, via->DrillingHole,
		      NULL, num, MaskFlags (via->Flags,
					    VIAFLAG | FOUNDFLAG | SELECTEDFLAG
					    | WARNFLAG));
      }
    hasParts = true;
  }
  END_LOOP;
  /* get the component-side SM pads */
  group = GetLayerGroupNumberByNumber (max_layer +
				       (SWAP_IDENT ? SOLDER_LAYER :
					COMPONENT_LAYER));
  GROUP_LOOP (Buffer->Data, group);
  {
    char num[8];
    LINE_LOOP (layer);
    {
      sprintf (num, "%d", pin_n++);
      CreateNewPad (Element, line->Point1.X,
		    line->Point1.Y, line->Point2.X,
		    line->Point2.Y, line->Thickness,
		    line->Clearance,
		    line->Thickness + line->Clearance, NULL,
		    line->Number ? line->Number : num,
		    MakeFlags (SWAP_IDENT ? ONSOLDERFLAG : NOFLAG));
      hasParts = true;
    }
    END_LOOP;
#warning FIXME Later
#if 0
    POUR_LOOP (layer);
    {
      POURPOLYGON_LOOP (pour);
      {
        int x1, y1, x2, y2, w, h, t;

        if (! polygon_is_rectangle (polygon))
          {
            crooked = true;
            continue;
          }

        w = polygon->Points[2].X - polygon->Points[0].X;
        h = polygon->Points[1].Y - polygon->Points[0].Y;
        t = (w < h) ? w : h;
        x1 = polygon->Points[0].X + t/2;
        y1 = polygon->Points[0].Y + t/2;
        x2 = x1 + (w-t);
        y2 = y1 + (h-t);

        sprintf (num, "%d", pin_n++);
        CreateNewPad (Element, x1, y1, x2, y2, t,
                      2 * Settings.Keepaway,
                      t + Settings.Keepaway,
                      NULL, num,
                      MakeFlags (SQUAREFLAG | (SWAP_IDENT ? ONSOLDERFLAG : NOFLAG)));
        hasParts = true;
      }
      END_LOOP;
    }
    END_LOOP;
#endif
  }
  END_LOOP;
  /* now get the opposite side pads */
  group = GetLayerGroupNumberByNumber (max_layer +
				       (SWAP_IDENT ? COMPONENT_LAYER :
					SOLDER_LAYER));
  GROUP_LOOP (Buffer->Data, group);
  {
    bool warned = false;
    char num[8];
    LINE_LOOP (layer);
    {
      sprintf (num, "%d", pin_n++);
      CreateNewPad (Element, line->Point1.X,
		    line->Point1.Y, line->Point2.X,
		    line->Point2.Y, line->Thickness,
		    line->Clearance,
		    line->Thickness + line->Clearance, NULL,
		    line->Number ? line->Number : num,
		    MakeFlags (SWAP_IDENT ? NOFLAG : ONSOLDERFLAG));
      if (!hasParts && !warned)
	{
	  warned = true;
	  Message
	    (_("Warning: All of the pads are on the opposite\n"
	       "side from the component - that's probably not what\n"
	       "you wanted\n"));
	}
      hasParts = true;
    }
    END_LOOP;
  }
  END_LOOP;
  /* now add the silkscreen. NOTE: elements must have pads or pins too */
  LINE_LOOP (&Buffer->Data->SILKLAYER);
  {
    if (line->Number && !NAMEONPCB_NAME (Element))
      NAMEONPCB_NAME (Element) = MyStrdup (line->Number,
					   "ConvertBufferToElement");
    CreateNewLineInElement (Element, line->Point1.X,
			    line->Point1.Y, line->Point2.X,
			    line->Point2.Y, line->Thickness);
    hasParts = true;
  }
  END_LOOP;
  ARC_LOOP (&Buffer->Data->SILKLAYER);
  {
    CreateNewArcInElement (Element, arc->X, arc->Y, arc->Width,
			   arc->Height, arc->StartAngle, arc->Delta,
			   arc->Thickness);
    hasParts = true;
  }
  END_LOOP;
  if (!hasParts)
    {
      DestroyObject (PCB->Data, ELEMENT_TYPE, Element, Element, Element);
      Message (_("There was nothing to convert!\n"
		 "Elements must have some silk, pads or pins.\n"));
      return (false);
    }
  if (crooked)
     Message (_("There were polygons that can't be made into pins!\n"
                "So they were not included in the element\n"));
  Element->MarkX = Buffer->X;
  Element->MarkY = Buffer->Y;
  if (SWAP_IDENT)
    SET_FLAG (ONSOLDERFLAG, Element);
  SetElementBoundingBox (PCB->Data, Element, &PCB->Font);
  ClearBuffer (Buffer);
  MoveObjectToBuffer (Buffer->Data, PCB->Data, ELEMENT_TYPE, Element, Element,
		      Element);
  SetBufferBoundingBox (Buffer);
  return (true);
}

/* ---------------------------------------------------------------------------
 * load PCB into buffer
 * parse the file with enabled 'PCB mode' (see parser)
 * if successful, update some other stuff
 */
bool
LoadLayoutToBuffer (BufferTypePtr Buffer, char *Filename)
{
  PCBTypePtr newPCB = CreateNewPCB (false);

  /* new data isn't added to the undo list */
  if (!ParsePCB (newPCB, Filename))
    {
      /* clear data area and replace pointer */
      ClearBuffer (Buffer);
      SaveFree (Buffer->Data);
      Buffer->Data = newPCB->Data;
      newPCB->Data = NULL;
      Buffer->X = newPCB->CursorX;
      Buffer->Y = newPCB->CursorY;
      RemovePCB (newPCB);
      Buffer->Data->pcb = PCB;
      return (true);
    }

  /* release unused memory */
  RemovePCB (newPCB);
      Buffer->Data->pcb = PCB;
  return (false);
}

/* ---------------------------------------------------------------------------
 * rotates the contents of the pastebuffer
 */
void
RotateBuffer (BufferTypePtr Buffer, BYTE Number)
{
  /* rotate vias */
  VIA_LOOP (Buffer->Data);
  {
    r_delete_entry (Buffer->Data->via_tree, (BoxTypePtr) via);
    ROTATE_VIA_LOWLEVEL (via, Buffer->X, Buffer->Y, Number);
    SetPinBoundingBox (via);
    r_insert_entry (Buffer->Data->via_tree, (BoxTypePtr) via, 0);
  }
  END_LOOP;

  /* elements */
  ELEMENT_LOOP (Buffer->Data);
  {
    RotateElementLowLevel (Buffer->Data, element, Buffer->X, Buffer->Y,
			   Number);
  }
  END_LOOP;

  /* all layer related objects */
  ALLLINE_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->line_tree, (BoxTypePtr) line);
    RotateLineLowLevel (line, Buffer->X, Buffer->Y, Number);
    r_insert_entry (layer->line_tree, (BoxTypePtr) line, 0);
  }
  ENDALL_LOOP;
  ALLARC_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->arc_tree, (BoxTypePtr) arc);
    RotateArcLowLevel (arc, Buffer->X, Buffer->Y, Number);
    r_insert_entry (layer->arc_tree, (BoxTypePtr) arc, 0);
  }
  ENDALL_LOOP;
  ALLTEXT_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->text_tree, (BoxTypePtr) text);
    RotateTextLowLevel (text, Buffer->X, Buffer->Y, Number);
    r_insert_entry (layer->text_tree, (BoxTypePtr) text, 0);
  }
  ENDALL_LOOP;
#warning FIXME Later
#if 0
  ALLPOLYGON_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->polygon_tree, (BoxTypePtr) polygon);
    RotatePolygonLowLevel (polygon, Buffer->X, Buffer->Y, Number);
    r_insert_entry (layer->polygon_tree, (BoxTypePtr) polygon, 0);
  }
  ENDALL_LOOP;
#endif
  ALLPOUR_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->pour_tree, (BoxTypePtr) pour);
    RotatePourLowLevel (pour, Buffer->X, Buffer->Y, Number);
    r_insert_entry (layer->pour_tree, (BoxTypePtr) pour, 0);
  }
  ENDALL_LOOP;

  /* finally the origin and the bounding box */
  ROTATE (Buffer->X, Buffer->Y, Buffer->X, Buffer->Y, Number);
  RotateBoxLowLevel (&Buffer->BoundingBox, Buffer->X, Buffer->Y, Number);
}

static void
free_rotate (int *x, int *y, int cx, int cy, double cosa, double sina)
{
  double nx, ny;
  int px = *x - cx;
  int py = *y - cy;

  nx = px * cosa + py * sina;
  ny = py * cosa - px * sina;

  *x = nx + cx;
  *y = ny + cy;
}

void
FreeRotateElementLowLevel (DataTypePtr Data, ElementTypePtr Element,
			   LocationType X, LocationType Y,
			   double cosa, double sina, double Angle)
{
  /* solder side objects need a different orientation */

  /* the text subroutine decides by itself if the direction
   * is to be corrected
   */
#if 0
  ELEMENTTEXT_LOOP (Element);
  {
    if (Data && Data->name_tree[n])
      r_delete_entry (Data->name_tree[n], (BoxType *) text);
    RotateTextLowLevel (text, X, Y, Number);
  }
  END_LOOP;
#endif
  ELEMENTLINE_LOOP (Element);
  {
    free_rotate (&line->Point1.X, &line->Point1.Y, X, Y, cosa, sina);
    free_rotate (&line->Point2.X, &line->Point2.Y, X, Y, cosa, sina);
    SetLineBoundingBox (line);
  }
  END_LOOP;
  PIN_LOOP (Element);
  {
    /* pre-delete the pins from the pin-tree before their coordinates change */
    if (Data)
      r_delete_entry (Data->pin_tree, (BoxType *) pin);
    RestoreToPours (Data, PIN_TYPE, Element, pin);
    free_rotate (&pin->X, &pin->Y, X, Y, cosa, sina);
    SetPinBoundingBox (pin);
  }
  END_LOOP;
  PAD_LOOP (Element);
  {
    /* pre-delete the pads before their coordinates change */
    if (Data)
      r_delete_entry (Data->pad_tree, (BoxType *) pad);
    RestoreToPours (Data, PAD_TYPE, Element, pad);
    free_rotate (&pad->Point1.X, &pad->Point1.Y, X, Y, cosa, sina);
    free_rotate (&pad->Point2.X, &pad->Point2.Y, X, Y, cosa, sina);
    SetLineBoundingBox ((LineType *) pad);
  }
  END_LOOP;
  ARC_LOOP (Element);
  {
    free_rotate (&arc->X, &arc->Y, X, Y, cosa, sina);
    arc->StartAngle += Angle;
    arc->StartAngle %= 360;
  }
  END_LOOP;

  free_rotate (&Element->MarkX, &Element->MarkY, X, Y, cosa, sina);
  SetElementBoundingBox (Data, Element, &PCB->Font);
  ClearFromPours (Data, ELEMENT_TYPE, Element, Element);
}

void
FreeRotateBuffer (BufferTypePtr Buffer, double Angle)
{
  double cosa, sina;

  cosa = cos(Angle * M_PI/180.0);
  sina = sin(Angle * M_PI/180.0);

  /* rotate vias */
  VIA_LOOP (Buffer->Data);
  {
    r_delete_entry (Buffer->Data->via_tree, (BoxTypePtr) via);
    free_rotate (&via->X, &via->Y, Buffer->X, Buffer->Y, cosa, sina);
    SetPinBoundingBox (via);
    r_insert_entry (Buffer->Data->via_tree, (BoxTypePtr) via, 0);
  }
  END_LOOP;

  /* elements */
  ELEMENT_LOOP (Buffer->Data);
  {
    FreeRotateElementLowLevel (Buffer->Data, element, Buffer->X, Buffer->Y,
			       cosa, sina, Angle);
  }
  END_LOOP;

  /* all layer related objects */
  ALLLINE_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->line_tree, (BoxTypePtr) line);
    free_rotate (&line->Point1.X, &line->Point1.Y, Buffer->X, Buffer->Y, cosa, sina);
    free_rotate (&line->Point2.X, &line->Point2.Y, Buffer->X, Buffer->Y, cosa, sina);
    SetLineBoundingBox (line);
    r_insert_entry (layer->line_tree, (BoxTypePtr) line, 0);
  }
  ENDALL_LOOP;
  ALLARC_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->arc_tree, (BoxTypePtr) arc);
    free_rotate (&arc->X, &arc->Y, Buffer->X, Buffer->Y, cosa, sina);
    arc->StartAngle += Angle;
    arc->StartAngle %= 360;
    r_insert_entry (layer->arc_tree, (BoxTypePtr) arc, 0);
  }
  ENDALL_LOOP;
  /* FIXME: rotate text */
#warning FIXME Later
#if 0
  ALLPOLYGON_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->polygon_tree, (BoxTypePtr) polygon);
    POLYGONPOINT_LOOP (polygon);
    {
      free_rotate (&point->X, &point->Y, Buffer->X, Buffer->Y, cosa, sina);
    }
    END_LOOP;
    SetPolygonBoundingBox (polygon);
    r_insert_entry (layer->polygon_tree, (BoxTypePtr) polygon, 0);
  }
  ENDALL_LOOP;
#endif
  ALLPOUR_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->pour_tree, (BoxTypePtr) pour);
    POURPOINT_LOOP (pour);
    {
      free_rotate (&point->X, &point->Y, Buffer->X, Buffer->Y, cosa, sina);
    }
    END_LOOP;
    SetPourBoundingBox (pour);
    r_insert_entry (layer->pour_tree, (BoxTypePtr) pour, 0);
  }
  ENDALL_LOOP;

  SetBufferBoundingBox (Buffer);
}


/* -------------------------------------------------------------------------- */

static const char freerotatebuffer_syntax[] =
  "FreeRotateBuffer([Angle])";

static const char freerotatebuffer_help[] =
  "Rotates the current paste buffer contents by the specified angle.  The\n"
  "angle is given in degrees.  If no angle is given, the user is prompted\n"
  "for one.\n";

/* %start-doc actions FreeRotateBuffer
   
Rotates the contents of the pastebuffer by an arbitrary angle.  If no
angle is given, the user is prompted for one.

%end-doc */

int
ActionFreeRotateBuffer(int argc, char **argv, int x, int y)
{
  HideCrosshair(false);
  char *angle_s;

  if (argc < 1)
    angle_s = gui->prompt_for ("Enter Rotation (degrees, CCW):", "0");
  else
    angle_s = argv[0];

  FreeRotateBuffer(PASTEBUFFER, strtod(angle_s, 0));
  RestoreCrosshair(false);
  return 0;
}

/* ---------------------------------------------------------------------------
 * initializes the buffers by allocating memory
 */
void
InitBuffers (void)
{
  int i;

  for (i = 0; i < MAX_BUFFER; i++)
    Buffers[i].Data = CreateNewBuffer ();
}

void
SwapBuffers (void)
{
  int i;

  for (i = 0; i < MAX_BUFFER; i++)
    SwapBuffer (&Buffers[i]);
  SetCrosshairRangeToBuffer ();
}

void
MirrorBuffer (BufferTypePtr Buffer)
{
  int i;

  if (Buffer->Data->ElementN)
    {
      Message (_("You can't mirror a buffer that has elements!\n"));
      return;
    }
  for (i = 0; i < max_layer + 2; i++)
    {
      LayerTypePtr layer = Buffer->Data->Layer + i;
      if (layer->TextN)
	{
	  Message (_("You can't mirror a buffer that has text!\n"));
	  return;
	}
    }
  /* set buffer offset to 'mark' position */
  Buffer->X = SWAP_X (Buffer->X);
  Buffer->Y = SWAP_Y (Buffer->Y);
  VIA_LOOP (Buffer->Data);
  {
    via->X = SWAP_X (via->X);
    via->Y = SWAP_Y (via->Y);
  }
  END_LOOP;
  ALLLINE_LOOP (Buffer->Data);
  {
    line->Point1.X = SWAP_X (line->Point1.X);
    line->Point1.Y = SWAP_Y (line->Point1.Y);
    line->Point2.X = SWAP_X (line->Point2.X);
    line->Point2.Y = SWAP_Y (line->Point2.Y);
  }
  ENDALL_LOOP;
  ALLARC_LOOP (Buffer->Data);
  {
    arc->X = SWAP_X (arc->X);
    arc->Y = SWAP_Y (arc->Y);
    arc->StartAngle = SWAP_ANGLE (arc->StartAngle);
    arc->Delta = SWAP_DELTA (arc->Delta);
    SetArcBoundingBox (arc);
  }
  ENDALL_LOOP;
#warning FIXME Later
#if 0
  ALLPOLYGON_LOOP (Buffer->Data);
  {
    POLYGONPOINT_LOOP (polygon);
    {
      point->X = SWAP_X (point->X);
      point->Y = SWAP_Y (point->Y);
    }
    END_LOOP;
    SetPolygonBoundingBox (polygon);
  }
  ENDALL_LOOP;
#endif
  SetBufferBoundingBox (Buffer);
}


/* ---------------------------------------------------------------------------
 * flip components/tracks from one side to the other
 */
static void
SwapBuffer (BufferTypePtr Buffer)
{
  int j, k;
  Cardinal sgroup, cgroup;
  LayerType swap;

  ELEMENT_LOOP (Buffer->Data);
  {
    r_delete_element (Buffer->Data, element);
    MirrorElementCoordinates (Buffer->Data, element, 0);
  }
  END_LOOP;
  /* set buffer offset to 'mark' position */
  Buffer->X = SWAP_X (Buffer->X);
  Buffer->Y = SWAP_Y (Buffer->Y);
  VIA_LOOP (Buffer->Data);
  {
    r_delete_entry (Buffer->Data->via_tree, (BoxTypePtr) via);
    via->X = SWAP_X (via->X);
    via->Y = SWAP_Y (via->Y);
    SetPinBoundingBox (via);
    r_insert_entry (Buffer->Data->via_tree, (BoxTypePtr) via, 0);
  }
  END_LOOP;
  ALLLINE_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->line_tree, (BoxTypePtr) line);
    line->Point1.X = SWAP_X (line->Point1.X);
    line->Point1.Y = SWAP_Y (line->Point1.Y);
    line->Point2.X = SWAP_X (line->Point2.X);
    line->Point2.Y = SWAP_Y (line->Point2.Y);
    SetLineBoundingBox (line);
    r_insert_entry (layer->line_tree, (BoxTypePtr) line, 0);
  }
  ENDALL_LOOP;
  ALLARC_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->arc_tree, (BoxTypePtr) arc);
    arc->X = SWAP_X (arc->X);
    arc->Y = SWAP_Y (arc->Y);
    arc->StartAngle = SWAP_ANGLE (arc->StartAngle);
    arc->Delta = SWAP_DELTA (arc->Delta);
    SetArcBoundingBox (arc);
    r_insert_entry (layer->arc_tree, (BoxTypePtr) arc, 0);
  }
  ENDALL_LOOP;
#warning FIXME Later
#if 0
  ALLPOLYGON_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->polygon_tree, (BoxTypePtr) polygon);
    POLYGONPOINT_LOOP (polygon);
    {
      point->X = SWAP_X (point->X);
      point->Y = SWAP_Y (point->Y);
    }
    END_LOOP;
    SetPolygonBoundingBox (polygon);
    r_insert_entry (layer->polygon_tree, (BoxTypePtr) polygon, 0);
    /* hmmm, how to handle clip */
  }
  ENDALL_LOOP;
#endif
  ALLTEXT_LOOP (Buffer->Data);
  {
    r_delete_entry (layer->text_tree, (BoxTypePtr) text);
    text->X = SWAP_X (text->X);
    text->Y = SWAP_Y (text->Y);
    TOGGLE_FLAG (ONSOLDERFLAG, text);
    SetTextBoundingBox (&PCB->Font, text);
    r_insert_entry (layer->text_tree, (BoxTypePtr) text, 0);
  }
  ENDALL_LOOP;
  /* swap silkscreen layers */
  swap = Buffer->Data->Layer[max_layer + SOLDER_LAYER];
  Buffer->Data->Layer[max_layer + SOLDER_LAYER] =
    Buffer->Data->Layer[max_layer + COMPONENT_LAYER];
  Buffer->Data->Layer[max_layer + COMPONENT_LAYER] = swap;

  /* swap layer groups when balanced */
  sgroup = GetLayerGroupNumberByNumber (max_layer + SOLDER_LAYER);
  cgroup = GetLayerGroupNumberByNumber (max_layer + COMPONENT_LAYER);
  if (PCB->LayerGroups.Number[cgroup] == PCB->LayerGroups.Number[sgroup])
    {
      for (j = k = 0; j < PCB->LayerGroups.Number[sgroup]; j++)
	{
	  int t1, t2;
	  Cardinal cnumber = PCB->LayerGroups.Entries[cgroup][k];
	  Cardinal snumber = PCB->LayerGroups.Entries[sgroup][j];

	  if (snumber >= max_layer)
	    continue;
	  swap = Buffer->Data->Layer[snumber];

	  while (cnumber >= max_layer)
	    {
	      k++;
	      cnumber = PCB->LayerGroups.Entries[cgroup][k];
	    }
	  Buffer->Data->Layer[snumber] = Buffer->Data->Layer[cnumber];
	  Buffer->Data->Layer[cnumber] = swap;
	  k++;
	  /* move the thermal flags with the layers */
	  ALLPIN_LOOP (Buffer->Data);
	  {
	    t1 = TEST_THERM (snumber, pin);
	    t2 = TEST_THERM (cnumber, pin);
	    ASSIGN_THERM (snumber, t2, pin);
	    ASSIGN_THERM (cnumber, t1, pin);
	  }
	  ENDALL_LOOP;
	  VIA_LOOP (Buffer->Data);
	  {
	    t1 = TEST_THERM (snumber, via);
	    t2 = TEST_THERM (cnumber, via);
	    ASSIGN_THERM (snumber, t2, via);
	    ASSIGN_THERM (cnumber, t1, via);
	  }
	  END_LOOP;
	}
    }
  SetBufferBoundingBox (Buffer);
}

/* ----------------------------------------------------------------------
 * moves the passed object to the passed buffer and removes it
 * from its original place
 */
void *
MoveObjectToBuffer (DataTypePtr Destination, DataTypePtr Src,
		    int Type, void *Ptr1, void *Ptr2, void *Ptr3)
{
  /* setup local identifiers used by move operations */
  Dest = Destination;
  Source = Src;
  return (ObjectOperation (&MoveBufferFunctions, Type, Ptr1, Ptr2, Ptr3));
}

/* ----------------------------------------------------------------------
 * Adds the passed object to the passed buffer
 */
void *
CopyObjectToBuffer (DataTypePtr Destination, DataTypePtr Src,
		    int Type, void *Ptr1, void *Ptr2, void *Ptr3)
{
  /* setup local identifiers used by Add operations */
  Dest = Destination;
  Source = Src;
  return (ObjectOperation (&AddBufferFunctions, Type, Ptr1, Ptr2, Ptr3));
}

/* ---------------------------------------------------------------------- */

HID_Action rotate_action_list[] = {
  {"FreeRotateBuffer", 0, ActionFreeRotateBuffer,
   freerotatebuffer_syntax, freerotatebuffer_help},
  {"LoadFootprint", 0, LoadFootprint,
   0,0}
};

REGISTER_ACTIONS (rotate_action_list)
