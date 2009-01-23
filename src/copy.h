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
 *  RCS: $Id: copy.h,v 1.5 2006-10-09 00:35:25 danmc Exp $
 */

/* prototypes for copy routines
 */

#ifndef	__COPY_INCLUDED__
#define	__COPY_INCLUDED__

#include "global.h"

/* ---------------------------------------------------------------------------
 * some defines
 */
#define	COPY_TYPES              \
	(VIA_TYPE | LINE_TYPE | TEXT_TYPE | \
	ELEMENT_TYPE | ELEMENTNAME_TYPE | POUR_TYPE | ARC_TYPE)


PourTypePtr CopyPourLowLevel (PourTypePtr, PourTypePtr);
ElementTypePtr CopyElementLowLevel (DataTypePtr, ElementTypePtr,
				    ElementTypePtr, Boolean, LocationType, LocationType);
Boolean CopyPastebufferToLayout (LocationType, LocationType);
void *CopyObject (int, void *, void *, void *, LocationType, LocationType);

#endif
