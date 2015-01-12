/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996 Thomas Nau
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


/* memory management functions
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "global.h"

#include "data.h"
#include "error.h"
#include "mymem.h"
#include "misc.h"
#include "rats.h"
#include "rtree.h"

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

/* ---------------------------------------------------------------------------
 * local prototypes
 */
static void DSRealloc (DynamicStringType *, size_t);


/* This API is quite new, provide a version here */
#if !GLIB_CHECK_VERSION (2, 28, 0)
static void
g_list_free_full (GList *list, GDestroyNotify free_func)
{
  g_list_foreach (list, (GFunc) free_func, NULL);
  g_list_free (list);
}
#endif

/* ---------------------------------------------------------------------------
 * get next slot for a rubberband connection, allocates memory if necessary
 */
RubberbandType *
GetRubberbandMemory (void)
{
  AttachedObjectType *attached = &Crosshair.AttachedObject;
  RubberbandType *new_obj;

  new_obj = g_slice_new0 (RubberbandType);
  attached->Rubberband = g_list_append (attached->Rubberband, new_obj);
  attached->RubberbandN ++;

  return new_obj;
}

void **
GetPointerMemory (PointerListType *list)
{
  void **new_obj;

  new_obj = g_slice_new0 (void *);
  list->Ptr = g_list_append (list->Ptr, new_obj);
  list->PtrN ++;

  return new_obj;
}

static void
FreePointer (gpointer data)
{
  g_slice_free (void *, data);
}

void
FreePointerListMemory (PointerListType *list)
{
  g_list_free_full (list->Ptr, (GDestroyNotify)FreePointer);
  memset (list, 0, sizeof (PointerListType));
}

/* ---------------------------------------------------------------------------
 * get next slot for a box, allocates memory if necessary
 */
BoxType *
GetBoxMemory (BoxListType *boxlist)
{
  BoxType *new_obj;

  new_obj = g_slice_new0 (BoxType);
  boxlist->Box = g_list_append (boxlist->Box, new_obj);
  boxlist->BoxN ++;

  return new_obj;
}

static void
FreeBox (BoxType *data)
{
  g_slice_free (BoxType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a connection, allocates memory if necessary
 */
ConnectionType *
GetConnectionMemory (NetType *net)
{
  ConnectionType *new_obj;

  new_obj = g_slice_new (ConnectionType);
  net->Connection = g_list_append (net->Connection, new_obj);
  net->ConnectionN ++;

  return new_obj;
}

static void
FreeConnection (ConnectionType *data)
{
  g_slice_free (ConnectionType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a subnet, allocates memory if necessary
 */
NetType *
GetNetMemory (NetListType *netlist)
{
  NetType *new_obj;

  new_obj = g_slice_new (NetType);
  netlist->Net = g_list_append (netlist->Net, new_obj);
  netlist->NetN ++;

  return new_obj;
}

static void
FreeNet (NetType *data)
{
  g_slice_free (NetType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a net list, allocates memory if necessary
 */
NetListType *
GetNetListMemory (NetListListType *netlistlist)
{
  NetListType* new_obj;

  new_obj = g_slice_new (NetListType);
  netlistlist->NetList = g_list_append (netlistlist->NetList, new_obj);
  netlistlist->NetListN ++;

  return new_obj;
}

static void
FreeNetList (NetListType *data)
{
  g_slice_free (NetListType, data);
}
/* ---------------------------------------------------------------------------
 * get next slot for a pin, allocates memory if necessary
 */
PinType *
GetPinMemory (ElementType *element)
{
  PinType *new_obj;

  new_obj = g_slice_new0 (PinType);
  element->Pin = g_list_append (element->Pin, new_obj);
  element->PinN ++;

  return new_obj;
}

static void
FreePin (PinType *data)
{
  g_slice_free (PinType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a pad, allocates memory if necessary
 */
PadType *
GetPadMemory (ElementType *element)
{
  PadType *new_obj;

  new_obj = g_slice_new0 (PadType);
  element->Pad = g_list_append (element->Pad, new_obj);
  element->PadN ++;

  return new_obj;
}

static void
FreePad (PadType *data)
{
  g_slice_free (PadType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a via, allocates memory if necessary
 */
PinType *
GetViaMemory (DataType *data)
{
  PinType *new_obj;

  new_obj = g_slice_new0 (PinType);
  data->Via = g_list_append (data->Via, new_obj);
  data->ViaN ++;

  return new_obj;
}

static void
FreeVia (PinType *data)
{
  g_slice_free (PinType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a Rat, allocates memory if necessary
 */
RatType *
GetRatMemory (DataType *data)
{
  RatType *new_obj;

  new_obj = g_slice_new0 (RatType);
  data->Rat = g_list_append (data->Rat, new_obj);
  data->RatN ++;

  return new_obj;
}

static void
FreeRat (RatType *data)
{
  g_slice_free (RatType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a line, allocates memory if necessary
 */
LineType *
GetLineMemory (LayerType *layer)
{
  LineType *new_obj;

  new_obj = g_slice_new0 (LineType);
  layer->Line = g_list_append (layer->Line, new_obj);
  layer->LineN ++;

  return new_obj;
}

static void
FreeLine (LineType *data)
{
  g_slice_free (LineType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for an arc, allocates memory if necessary
 */
ArcType *
GetArcMemory (LayerType *layer)
{
  ArcType *new_obj;

  new_obj = g_slice_new0 (ArcType);
  layer->Arc = g_list_append (layer->Arc, new_obj);
  layer->ArcN ++;

  return new_obj;
}

static void
FreeArc (ArcType *data)
{
  g_slice_free (ArcType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a text object, allocates memory if necessary
 */
TextType *
GetTextMemory (LayerType *layer)
{
  TextType *new_obj;

  new_obj = g_slice_new0 (TextType);
  layer->Text = g_list_append (layer->Text, new_obj);
  layer->TextN ++;

  return new_obj;
}

static void
FreeText (TextType *data)
{
  g_slice_free (TextType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a polygon object, allocates memory if necessary
 */
PolygonType *
GetPolygonMemory (LayerType *layer)
{
  PolygonType *new_obj;

  new_obj = g_slice_new0 (PolygonType);
  layer->Polygon = g_list_append (layer->Polygon, new_obj);
  layer->PolygonN ++;

  return new_obj;
}

static void
FreePolygon (PolygonType *data)
{
  g_slice_free (PolygonType, data);
}

/* ---------------------------------------------------------------------------
 * gets the next slot for a point in a polygon struct, allocates memory
 * if necessary
 */
PointType *
GetPointMemoryInPolygon (PolygonType *polygon)
{
  PointType *new_obj;

  new_obj = g_slice_new (PointType);
  polygon->Points = g_list_append (polygon->Points, new_obj);
  polygon->PointN ++;

  return new_obj;
}

static void
FreePoint (PointType *data)
{
  g_slice_free (PointType, data);
}

/* ---------------------------------------------------------------------------
 * gets the next slot for a point in a polygon struct, allocates memory
 * if necessary
 */
Cardinal *
GetHoleIndexMemoryInPolygon (PolygonType *polygon)
{
  Cardinal *new_obj;

  new_obj = g_slice_new (Cardinal);
  polygon->HoleIndex = g_list_append (polygon->HoleIndex, new_obj);
  polygon->HoleIndexN ++;

  return new_obj;
}

static void
FreeHoleIndex (Cardinal *data)
{
  g_slice_free (Cardinal, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for an element, allocates memory if necessary
 */
ElementType *
GetElementMemory (DataType *data)
{
  ElementType *new_obj;

  new_obj = g_slice_new0 (ElementType);

  if (data != NULL)
    {
      data->Element = g_list_append (data->Element, new_obj);
      data->ElementN ++;
    }

  return new_obj;
}

static void
FreeElement (ElementType *data)
{
  g_slice_free (ElementType, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a library entry, allocates memory if necessary
 */
LibraryEntryType *
GetLibraryEntryMemory (LibraryMenuType *menu)
{
  LibraryEntryType *new_obj;

  new_obj = g_slice_new (LibraryEntryType);
  menu->Entry = g_list_append (menu->Entry, new_obj);
  menu->EntryN ++;

  return new_obj;
}

static void
FreeLibraryEntry (LibraryEntryType *entry)
{
  free (entry->AllocatedMemory);
  free (entry->ListEntry);

  g_slice_free (LibraryEntryType, entry);
}

/* ---------------------------------------------------------------------------
 * get next slot for a library menu, allocates memory if necessary
 */
LibraryMenuType *
GetLibraryMenuMemory (LibraryType *lib)
{
  LibraryMenuType *new_obj;

  new_obj = g_slice_new (LibraryMenuType);
  lib->Menu = g_list_append (lib->Menu, new_obj);
  lib->MenuN ++;

  return new_obj;
}

static void
FreeLibraryMenu (LibraryMenuType *menu)
{
  g_list_free_full (menu->Entry, (GDestroyNotify)FreeLibraryEntry);
  free (menu->Name);
  g_slice_free (LibraryMenuType, menu);
}

/* ---------------------------------------------------------------------------
 * get next slot for a DrillElement, allocates memory if necessary
 */
ElementType **
GetDrillElementMemory (DrillType *drill)
{
  ElementType **new_obj;

  new_obj = g_slice_new (ElementType *);
  drill->Element = g_list_append (drill->Element, new_obj);
  drill->ElementN ++;

  return new_obj;
}

static void
FreeDrillElement (ElementType **data)
{
  g_slice_free (ElementType *, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a DrillPoint, allocates memory if necessary
 */
PinType **
GetDrillPinMemory (DrillType *drill)
{
  PinType **new_obj;

  new_obj = g_slice_new (PinType *);
  drill->Pin = g_list_append (drill->Pin, new_obj);
  drill->PinN ++;

  return new_obj;
}

static void
FreeDrillPin (PinType **data)
{
  g_slice_free (PinType *, data);
}

/* ---------------------------------------------------------------------------
 * get next slot for a Drill, allocates memory if necessary
 */
DrillType *
GetDrillInfoDrillMemory (DrillInfoType *drillinfo)
{
  DrillType *new_obj;

  new_obj = g_slice_new (DrillType);
  drillinfo->Drill = g_list_append (drillinfo->Drill, new_obj);
  drillinfo->DrillN ++;

  return new_obj;
}

static void
FreeDrill (DrillType *drill)
{
  g_list_free_full (drill->Element, (GDestroyNotify)FreeDrillElement);
  g_list_free_full (drill->Pin, (GDestroyNotify)FreeDrillPin);
  g_slice_free (DrillType, drill);
}

void
FreeDrillInfo (DrillInfoType *drill_info)
{
  g_list_free_full (drill_info->Drill, (GDestroyNotify)FreeDrill);
  g_slice_free (DrillInfoType, drill_info);
}

AttributeType *
GetAttributeMemory (AttributeListType *attr_list)
{
  AttributeType *new_obj;

  new_obj = g_slice_new0 (AttributeType);
  attr_list->List = g_list_append (attr_list->List, new_obj);
  attr_list->Number ++;

  return new_obj;
}

void
FreeAttribute (AttributeType *attr)
{
  free (attr->name);
  free (attr->value);
  g_slice_free (AttributeType, attr);
}

/* ---------------------------------------------------------------------------
 * frees memory used by a polygon
 */
void
FreePolygonMemory (PolygonType *polygon)
{
  if (polygon == NULL)
    return;

  free (polygon->Points);
  free (polygon->HoleIndex);

  if (polygon->Clipped)
    poly_Free (&polygon->Clipped);
  poly_FreeContours (&polygon->NoHoles);

  memset (polygon, 0, sizeof (PolygonType));
}

/* ---------------------------------------------------------------------------
 * frees memory used by a box list
 */
void
FreeBoxListMemory (BoxListType *boxlist)
{
  if (boxlist == NULL)
    return;

  g_list_free_full (boxlist->Box, (GDestroyNotify)FreeBox);
  memset (boxlist, 0, sizeof (BoxListType));
}

/* ---------------------------------------------------------------------------
 * frees memory used by a net 
 */
void
FreeNetListMemory (NetListType *netlist)
{
  if (netlist == NULL)
    return;

  NET_LOOP (netlist);
  {
    FreeNetMemory (net);
  }
  END_LOOP;

  g_list_free_full (netlist->Net, (GDestroyNotify)FreeNet);
  memset (netlist, 0, sizeof (NetListType));
}

/* ---------------------------------------------------------------------------
 * frees memory used by a net list
 */
void
FreeNetListListMemory (NetListListType *netlistlist)
{
  if (netlistlist == NULL)
    return;

  NETLIST_LOOP (netlistlist);
  {
    FreeNetListMemory (netlist);
  }
  END_LOOP;

  g_list_free_full (netlistlist->NetList, (GDestroyNotify)FreeNetList);
  memset (netlistlist, 0, sizeof (NetListListType));
}

/* ---------------------------------------------------------------------------
 * frees memory used by a subnet 
 */
void
FreeNetMemory (NetType *net)
{
  if (net == NULL)
    return;

  g_list_free_full (net->Connection, (GDestroyNotify)FreeConnection);
  memset (net, 0, sizeof (NetType));
}

/* ---------------------------------------------------------------------------
 * frees memory used by an attribute list
*/
void
FreeAttributeListMemory (AttributeListType *list)
{
  g_list_free_full (list->List, (GDestroyNotify)FreeAttribute);
  memset (list, 0, sizeof (AttributeListType));
}

/* ---------------------------------------------------------------------------
 * frees memory used by an element
 */
void
FreeElementMemory (ElementType *element)
{
  if (element == NULL)
    return;

  ELEMENTNAME_LOOP (element);
  {
    free (textstring);
  }
  END_LOOP;
  PIN_LOOP (element);
  {
    free (pin->Name);
    free (pin->Number);
  }
  END_LOOP;
  PAD_LOOP (element);
  {
    free (pad->Name);
    free (pad->Number);
  }
  END_LOOP;

  g_list_free_full (element->Pin,  (GDestroyNotify)FreePin);
  g_list_free_full (element->Pad,  (GDestroyNotify)FreePad);
  g_list_free_full (element->Line, (GDestroyNotify)FreeLine);
  g_list_free_full (element->Arc,  (GDestroyNotify)FreeArc);

  FreeAttributeListMemory (&element->Attributes);
  memset (element, 0, sizeof (ElementType));
}

/* ---------------------------------------------------------------------------
 * free memory used by PCB
 */
void
FreePCBMemory (PCBType *pcb)
{
  int i;

  if (pcb == NULL)
    return;

  free (pcb->Name);
  free (pcb->Filename);
  free (pcb->PrintFilename);
  FreeDataMemory (pcb->Data);
  free (pcb->Data);
  /* release font symbols */
  for (i = 0; i <= MAX_FONTPOSITION; i++)
    free (pcb->Font.Symbol[i].Line);
  FreeLibraryMemory (&pcb->NetlistLib);
  NetlistChanged (0);
  FreeAttributeListMemory (&pcb->Attributes);
  /* clear struct */
  memset (pcb, 0, sizeof (PCBType));
}

/* ---------------------------------------------------------------------------
 * free memory used by data struct
 */
void
FreeDataMemory (DataType *data)
{
  LayerType *layer;
  int i;

  if (data == NULL)
    return;

  VIA_LOOP (data);
  {
    free (via->Name);
  }
  END_LOOP;
  g_list_free_full (data->Via, (GDestroyNotify)FreeVia);
  ELEMENT_LOOP (data);
  {
    FreeElementMemory (element);
  }
  END_LOOP;
  g_list_free_full (data->Element, (GDestroyNotify)FreeElement);
  g_list_free_full (data->Rat, (GDestroyNotify)FreeRat);

  for (layer = data->Layer, i = 0; i < MAX_LAYER + EXTRA_LAYERS; layer++, i++)
    {
      FreeAttributeListMemory (&layer->Attributes);
      TEXT_LOOP (layer);
      {
        free (text->TextString);
      }
      END_LOOP;
      if (layer->Name)
        free (layer->Name);
      LINE_LOOP (layer);
      {
        if (line->Number)
          free (line->Number);
      }
      END_LOOP;
      g_list_free_full (layer->Line, (GDestroyNotify)FreeLine);
      g_list_free_full (layer->Arc, (GDestroyNotify)FreeArc);
      g_list_free_full (layer->Text, (GDestroyNotify)FreeText);
      POLYGON_LOOP (layer);
      {
        FreePolygonMemory (polygon);
      }
      END_LOOP;
      g_list_free_full (layer->Polygon, (GDestroyNotify)FreePolygon);
      if (layer->line_tree)
        r_destroy_tree (&layer->line_tree);
      if (layer->arc_tree)
        r_destroy_tree (&layer->arc_tree);
      if (layer->text_tree)
        r_destroy_tree (&layer->text_tree);
      if (layer->polygon_tree)
        r_destroy_tree (&layer->polygon_tree);
    }

  if (data->element_tree)
    r_destroy_tree (&data->element_tree);
  for (i = 0; i < MAX_ELEMENTNAMES; i++)
    if (data->name_tree[i])
      r_destroy_tree (&data->name_tree[i]);
  if (data->via_tree)
    r_destroy_tree (&data->via_tree);
  if (data->pin_tree)
    r_destroy_tree (&data->pin_tree);
  if (data->pad_tree)
    r_destroy_tree (&data->pad_tree);
  if (data->rat_tree)
    r_destroy_tree (&data->rat_tree);
  /* clear struct */
  memset (data, 0, sizeof (DataType));
}

/* ---------------------------------------------------------------------------
 * releases the memory that's allocated by the library
 */
void
FreeLibraryMemory (LibraryType *lib)
{
  g_list_free_full (lib->Menu, (GDestroyNotify)FreeLibraryMenu);
}

/* ---------------------------------------------------------------------------
 * reallocates memory for a dynamic length string if necessary
 */
static void
DSRealloc (DynamicStringType *Ptr, size_t Length)
{
  int input_null = (Ptr->Data == NULL);
  if (input_null || Length >= Ptr->MaxLength)
    {
      Ptr->MaxLength = Length + 512;
      Ptr->Data = (char *)realloc (Ptr->Data, Ptr->MaxLength);
      if (input_null)
	Ptr->Data[0] = '\0';
    }
}

/* ---------------------------------------------------------------------------
 * adds one character to a dynamic string
 */
void
DSAddCharacter (DynamicStringType *Ptr, char Char)
{
  size_t position = Ptr->Data ? strlen (Ptr->Data) : 0;

  DSRealloc (Ptr, position + 1);
  Ptr->Data[position++] = Char;
  Ptr->Data[position] = '\0';
}

/* ---------------------------------------------------------------------------
 * add a string to a dynamic string
 */
void
DSAddString (DynamicStringType *Ptr, const char *S)
{
  size_t position = Ptr->Data ? strlen (Ptr->Data) : 0;

  if (S && *S)
    {
      DSRealloc (Ptr, position + 1 + strlen (S));
      strcat (&Ptr->Data[position], S);
    }
}

/* ----------------------------------------------------------------------
 * clears a dynamic string
 */
void
DSClearString (DynamicStringType *Ptr)
{
  if (Ptr->Data)
    Ptr->Data[0] = '\0';
}

/* ---------------------------------------------------------------------------
 * strips leading and trailing blanks from the passed string and
 * returns a pointer to the new 'duped' one or NULL if the old one
 * holds only white space characters
 */
char *
StripWhiteSpaceAndDup (const char *S)
{
  const char *p1, *p2;
  char *copy;
  size_t length;

  if (!S || !*S)
    return (NULL);

  /* strip leading blanks */
  for (p1 = S; *p1 && isspace ((int) *p1); p1++);

  /* strip trailing blanks and get string length */
  length = strlen (p1);
  for (p2 = p1 + length - 1; length && isspace ((int) *p2); p2--, length--);

  /* string is not empty -> allocate memory */
  if (length)
    {
      copy = (char *)realloc (NULL, length + 1);
      strncpy (copy, p1, length);
      copy[length] = '\0';
      return (copy);
    }
  else
    return (NULL);
}
