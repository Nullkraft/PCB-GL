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

/* prototypes for memory routines
 */

#ifndef	PCB_MYMEM_H
#define	PCB_MYMEM_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include "global.h"

#define STRDUP(x) (((x) != NULL) ? strdup (x) : NULL)

/* ---------------------------------------------------------------------------
 * some memory types
 */
typedef struct
{
  size_t MaxLength;
  char *Data;
} DynamicStringType;

RubberbandType *GetRubberbandMemory (void);
PinType *GetPinMemory (ElementType *);
PadType *GetPadMemory (ElementType *);
PinType *GetViaMemory (DataType *);
LineType *GetLineMemory (LayerType *);
ArcType *GetArcMemory (LayerType *);
RatType *GetRatMemory (DataType *);
TextType *GetTextMemory (LayerType *);
PolygonType *GetPolygonMemory (LayerType *);
PointType *GetPointMemoryInPolygon (PolygonType *);
Cardinal *GetHoleIndexMemoryInPolygon (PolygonType *);
ElementType *GetElementMemory (DataType *);
BoxType *GetBoxMemory (BoxListType *);
ConnectionType *GetConnectionMemory (NetType *);
NetType *GetNetMemory (NetListType *);
NetListType *GetNetListMemory (NetListListType *);
LibraryMenuType *GetLibraryMenuMemory (LibraryType *);
LibraryEntryType *GetLibraryEntryMemory (LibraryMenuType *);
ElementTypeHandle GetDrillElementMemory (DrillType *);
PinTypeHandle GetDrillPinMemory (DrillType *);
DrillType *GetDrillInfoDrillMemory (DrillInfoType *);
void FreeDrillInfo (DrillInfoType *);
void **GetPointerMemory (PointerListType *);
AttributeType *GetAttributeMemory (AttributeListType *);
void FreeAttribute (AttributeType *);
void FreePolygonMemory (PolygonType *);
void FreeAttributeListMemory (AttributeListType *);
void FreeElementMemory (ElementType *);
void FreePCBMemory (PCBType *);
void FreeBoxListMemory (BoxListType *);
void FreeNetListListMemory (NetListListType *);
void FreeNetListMemory (NetListType *);
void FreeNetMemory (NetType *);
void FreeDataMemory (DataType *);
void FreeLibraryMemory (LibraryType *);
void FreePointerListMemory (PointerListType *);
void DSAddCharacter (DynamicStringType *, char);
void DSAddString (DynamicStringType *, const char *);
void DSClearString (DynamicStringType *);
char *StripWhiteSpaceAndDup (const char *);

#ifdef NEED_STRDUP
char *strdup (const char *);
#endif

#ifndef HAVE_LIBDMALLOC
#define malloc(x) calloc(1,(x))
#endif

#endif
