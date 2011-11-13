%{
/*
 *                            COPYRIGHT
 *
 *  PCB, interactive printed circuit board design
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#if defined(_POSIX_SOURCE) || defined(_HPUX_SOURCE)
#include <unistd.h>
#endif

#include "global.h"

#ifdef HAVE_LIBDMALLOC
# include <dmalloc.h> /* see http://dmalloc.com */
#endif


#include "global.h"
#include "crosshair.h"
#include "data.h"
#include "error.h"
#include "file.h"
#include "mymem.h"
#include "misc.h"
#include "strflags.h"
#include "hidgl_package_vrml_y.h"

/* ---------------------------------------------------------------------------
 * some shared parser identifiers
 */
#ifdef FLEX_SCANNER
int vrml_yylineno;  /* linenumber */
#define yyunput ATTRIBUTE_UNUSED yyunput
#endif

char *vrml_yyfilename; /* in this file */

/* ---------------------------------------------------------------------------
 * an external prototype
 */
extern int	vrml_yyparse(void);

/*STRING			".*" ... double-quotes must be \", backslashes must be \\... */
/*IDFIRSTCHAR		[^0-9 "#'+,-.\[\\\]{}] */
/*IDRESTCHARS		[^ "#',.\[\\\]{}] */

/*STRINGCHAR              ([^"\n\r\\]|\\.) */
/* \"{STRINGCHAR}*\"	{printf ("DEAD RULE?\n");} */
/* {STRINGCHAR}+		{ */

%}

INT32			([+\-]?(([0-9]+)|(0[xX][0-9a-fA-F]+)))
FLOATING		([+/-]?((([0-9]+(\.)?)|([0-9]*\.[0-9]+))([eE][+\-]?[0-9]+)?))

DECIMAL                 -?[1-9][0-9]*|0

STRINGCHAR              ([^, "\n\r\\{}\[\]]|\\.)

%option prefix="vrml_yy"
%option outfile="lex.yy.c"
%option yylineno

%%

DEF		{ return T_DEF; }
EXTERNPROTO	{ return T_EXTERNPROTO; }
FALSE		{ return T_FALSE; }
IS		{ return T_IS; }
NULL		{ return T_NULL; }
PROTO		{ return T_PROTO; }
ROUTE		{ return T_ROUTE; }
TO		{ return T_TO; }
TRUE		{ return T_TRUE; }
USE		{ return T_USE; }
eventIn		{ return T_EVENTIN; }
eventOut	{ return T_EVENTOUT; }
exposedField	{ return T_EXPOSEDFIELD; }
field		{ return T_FIELD; }

Script		{ return T_SCRIPT; }

"#VRML V2.0 utf8"	{ return T_VRMLHEADER; }

%{ /* Fields for any grouping node */ %}
children		{ return T_children; }

%{ /* Transform node and its fields */ %}
Transform		{ return T_TRANSFORM; }
center			{ return T_center; }
rotation		{ return T_rotation; }
scale			{ return T_scale; }
scaleOrientation	{ return T_scale_orientation; }
translation		{ return T_translation; }
bboxCenter		{ return T_bbox_center; }
bboxSize		{ return T_bbox_size; }

%{ /* Shape node and its fields */ %}
Shape			{ return T_SHAPE; }
appearance		{ return T_appearance; }
geometry		{ return T_geometry; }

%{ /* Appearance node and its fields */ %}
Appearance		{ return T_APPEARANCE; }
material		{ return T_material; }
texture			{ return T_texture; }
textureTransform	{ return T_texture_transform; }

%{ /* Material node and its fields */ %}
Material		{ return T_MATERIAL; }
ambientIntensity	{ return T_ambient_intensity; }
diffuseColor		{ return T_diffuse_color; }
emissiveColor		{ return T_emissive_color; }
shininess		{ return T_shininess; }
specularColor		{ return T_specular_color; }
transparency		{ return T_transparency; }

%{ /* IndexedFaceSet node and its fields */ %}
IndexedFaceSet		{ return T_INDEXED_FACE_SET; }
color			{ return T_color; }
coord			{ return T_coord; }
normal			{ return T_normal; }
texCoord		{ return T_tex_coord; }
ccw			{ return T_ccw; }
colorIndex		{ return T_color_index; }
colorPerVertex		{ return T_color_per_vertex; }
convex			{ return T_convex; }
coordIndex		{ return T_coord_index; }
creaseAngle		{ return T_crease_angle; }
normalIndex		{ return T_normal_index; }
normalPerVertex		{ return T_normal_per_vertex; }
solid			{ return T_solid; }
texCoordIndex		{ return T_tex_coord_index; }

%{ /* Coordinate node and its field */ %}
Coordinate		{ return T_COORDINATE; }
point			{ return T_point; }

%{ /* Normal node and its field */ %}
Normal			{ return T_NORMAL; }
vector			{ return T_vector; }


{INT32}		{
					vrml_yylval.int32 = atoi(vrml_yytext);
					return INT32;
			}

{FLOATING}		{
					vrml_yylval.floating = strtod (vrml_yytext, NULL);
					return FLOATING;
			}

#.*					{}

{STRINGCHAR}*	{

						char	*p1, *p2;

							/* allocate memory and copy string;
							 * stringlength is counted and copied without
							 * leading and trailing '"'
							 */
						vrml_yylval.string = calloc (vrml_yyleng+1, sizeof(char));
						p1 = (char *) (vrml_yytext);
						p2 = vrml_yylval.string;
						while(vrml_yyleng--)
						{
								/* check for special character */
							if (*p1 == '\\')
							{
								vrml_yyleng--;
								p1++;

							}
							*p2++ = *p1++;
						}
						*p2 = '\0';
//						printf ("Returning a string token %s\n", vrml_yylval.string);
						return(STRING);
					}
[ \t,]+				{}
[\n]				{
#ifndef FLEX_SCANNER
						vrml_yylineno++;
#endif
					}
[\r]				{}
.					{ return(*vrml_yytext); }

%%

/* ---------------------------------------------------------------------------
 * sets up the preprocessor command
 */
static int Parse(char *filename)
{
  int retval;
  extern int vrml_yydebug;

#ifdef FLEX_SCANNER
  static	bool	firsttime = true;
#endif

  yyin = fopen (filename, "r");
  if (!yyin)
    return 1;

#ifdef FLEX_SCANNER
  /* reset parser if not called the first time */
  if (!firsttime)
    vrml_yyrestart(yyin);
  firsttime = false;
#endif

  /* init linenumber and filename for vrml_yyerror() */
  vrml_yylineno = 1;
  vrml_yyfilename = filename;
//  vrml_yydebug = 1;

  /* We need to save the data temporarily because lex-yacc are able
   * to break the application if the input file has an illegal format.
   * It's not necessary if the system supports the call of functions
   * on termination.
   */

#if !defined(HAS_ATEXIT) && !defined(HAS_ON_EXIT)
  if (PCB)
    SaveTMPData();
  retval = vrml_yyparse();
  RemoveTMPData();
#else
  retval = vrml_yyparse();
#endif

  /* clean up parse buffer */
  vrml_yy_delete_buffer (YY_CURRENT_BUFFER);

  return (fclose (vrml_yyin) ? 1 : retval);
}

/* ---------------------------------------------------------------------------
 * initializes LEX and calls parser for a single element file
 */
int hidgl_parse_vrml (char *filename)
{
  return (Parse (filename));
}
