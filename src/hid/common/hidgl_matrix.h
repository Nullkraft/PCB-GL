/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
 *  Copyright (C) 2010 PCB Contributors (See ChangeLog for details).
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
 */

#ifndef __HIDGL_MATRIX_INCLUDED__
#define __HIDGL_MATRIX_INCLUDED__

/* Some convenience routines for simple matrix operations. For anything
 * more complicated, we should probably pull in a linear algebra library.
 */

float determinant_2x2 (float m[2][2]);
void invert_2x2 (float m[2][2], float out[2][2]);

float determinant_4x4 (float m[4][4]);
void invert_4x4 (float m[4][4], float out[4][4]);

#endif /* __HIDGL_MATRIX_INCLUDED__  */
