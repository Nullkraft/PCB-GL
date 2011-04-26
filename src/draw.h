/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 1994,1995,1996, 2004 Thomas Nau
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
 *  RCS: $Id$
 */

/* prototypes for drawing routines
 */

#ifndef	__DRAW_INCLUDED__
#define	__DRAW_INCLUDED__

#include "global.h"

/*bool	SwitchDrawingWindow(double, GdkDrawable *, gboolean, gboolean);*/

void Draw (void);
void Redraw (void);
void DrawVia (PinTypePtr);
void DrawRat (RatTypePtr);
void DrawViaName (PinTypePtr);
void DrawPin (PinTypePtr);
void DrawPinName (PinTypePtr);
void DrawPad (PadTypePtr);
void DrawPadName (PadTypePtr);
void DrawLine (LayerTypePtr, LineTypePtr);
void DrawArc (LayerTypePtr, ArcTypePtr);
void DrawText (LayerTypePtr, TextTypePtr);
void DrawTextLowLevel (TextTypePtr, int);
void DrawPolygon (LayerTypePtr, PolygonTypePtr);
void DrawPour (LayerTypePtr, PourTypePtr);
void DrawElement (ElementTypePtr);
void DrawElementName (ElementTypePtr);
void DrawElementPackage (ElementTypePtr);
void DrawElementPinsAndPads (ElementTypePtr);
void DrawObject (int, void *, void *);
void EraseVia (PinTypePtr);
void EraseRat (RatTypePtr);
void EraseViaName (PinTypePtr);
void ErasePad (PadTypePtr);
void ErasePadName (PadTypePtr);
void ErasePin (PinTypePtr);
void ErasePinName (PinTypePtr);
void EraseLine (LineTypePtr);
void EraseArc (ArcTypePtr);
void EraseText (LayerTypePtr, TextTypePtr);
void ErasePolygon (PolygonTypePtr);
void ErasePour (PourTypePtr);
void EraseElement (ElementTypePtr);
void EraseElementPinsAndPads (ElementTypePtr);
void EraseElementName (ElementTypePtr);
void EraseObject (int, void *, void *);
void LoadBackgroundImage (char *);

/* TEMPORARY */
void ClearPad (PadTypePtr, bool);
void DrawPinOrViaLowLevel (PinTypePtr, bool);
void DrawPlainPin (PinTypePtr, bool);
void DrawPlainVia (PinTypePtr, bool);
void DrawRegularText (LayerTypePtr, TextTypePtr);
void DrawEMark (ElementTypePtr, LocationType, LocationType, bool);
void DrawHole (PinTypePtr);
void DrawRats (BoxType *);
void DrawSilk (int side, const BoxType *);

/* TEMPORARY */

/*GdkDrawable *draw_get_current_drawable(void);*/

#endif
