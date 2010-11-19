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

#include "global.h"

#ifdef HAVE_LIBDMALLOC
# include <dmalloc.h> /* see http://dmalloc.com */
#endif

#define YYDEBUG 1

int vrml_yyerror(const char *s);
extern int vrml_yylex();
extern int vrml_yylineno;
extern char *vrml_yyfilename;

%}

%name-prefix="vrml_yy"

%union									/* define YYSTACK type */
{
	int		int32;
	double		floating;
	char		*string;
}

%token T_DEF
%token T_EXTERNPROTO
%token T_FALSE
%token T_IS
%token T_NULL
%token T_PROTO
%token T_ROUTE
%token T_TO
%token T_TRUE
%token T_USE
%token T_EVENTIN
%token T_EVENTOUT
%token T_EXPOSEDFIELD
%token T_FIELD

%token MFColor
%token MFFloat
%token MFInt32
%token MFNode
%token MFRotation
%token MFString
%token MFTime
%token MFVec2f
%token MFVec3f
%token SFBool
%token SFColor
%token SFFloat
%token SFImage
%token SFInt32
%token SFNode
%token SFRotation
%token SFString
%token SFTime
%token SFVec2f
%token SFVec3f

%token T_VRMLHEADER
%token T_SCRIPT
%token T_ID
%token T_FIELDTYPE


/* Fields for any grouping node */
%token T_children

/* Transform node and its fields */
%token T_TRANSFORM
%token T_center
%token T_rotation
%token T_scale
%token T_scale_orientation
%token T_translation
%token T_bbox_center
%token T_bbox_size

/* Shape node and its fields */
%token T_SHAPE
%token T_appearance
%token T_geometry

/* Appearance node and its fields */
%token T_APPEARANCE
%token T_material
%token T_texture
%token T_texture_transform

/* Material node and its fields */
%token T_MATERIAL
%token T_ambient_intensity
%token T_diffuse_color
%token T_emissive_color
%token T_shininess
%token T_specular_color
%token T_transparency

/* IndexedFaceSet node and its fields */
%token T_INDEXED_FACE_SET
%token T_color
%token T_coord
%token T_normal
%token T_tex_coord
%token T_ccw
%token T_color_index
%token T_color_per_vertex
%token T_convex
%token T_coord_index
%token T_crease_angle
%token T_normal_index
%token T_normal_per_vertex
%token T_solid
%token T_tex_coord_index

/* Coordinate node and its field */
%token T_COORDINATE
%token T_point

/* Normal node and its field */
%token T_NORMAL
%token T_vector


%token <floating>      FLOATING
%token <number>        INT32

%token <string>        STRING
//%token <string>        IDFIRSTCHAR
//%token <string>        IDRESTCHARS

%%

/* General VRML stuff */

parse				: vrmlHeader
				  vrmlScene
				| error { YYABORT; }
				;

vrmlHeader			: T_VRMLHEADER { printf ("Got header\n"); }
				  vrmlHeaderComment
				;

vrmlHeaderComment		: sfstringValues
				| empty
				;

vrmlScene			: statements
				;

statements			: statement
				| statement statements
				| empty
				;

statement			: nodeStatement
//				| protoStatement
//				| routeStatement
				;

nodeStatement			: node
				| T_DEF nodeNameId node
				| T_USE nodeNameId
				;

/*
rootNodeStatement		: node
				| T_DEF nodeNameId node { printf ("Got a root Node Statement\n"); }
				;

protoStatement			: proto
				| externproto
				;

protoStatements			: protoStatement
				| protoStatement protoStatements
				| empty
				;

proto				: T_PROTO nodeTypeId '[' interfaceDeclarations ']' '{' protoBody '}'
				;

protoBody			: protoStatements rootNodeStatement statements
				;

interfaceDeclarations		: interfaceDeclaration
				| interfaceDeclaration interfaceDeclarations
				| empty
				;

restrictedInterfaceDeclaration	: T_EVENTIN fieldType eventInId
				| T_EVENTOUT fieldType eventOutId
				| T_FIELD fieldType fieldId fieldValue
				;

interfaceDeclaration		: restrictedInterfaceDeclaration
				| T_EXPOSEDFIELD fieldType fieldId fieldValue
				;

externproto			: T_EXTERNPROTO nodeTypeId '[' externInterfaceDeclarations ']' URLList
				;

externInterfaceDeclarations	: externInterfaceDeclaration
				| externInterfaceDeclaration externInterfaceDeclarations
				| empty
				;

externInterfaceDeclaration	: T_EVENTIN fieldType eventInId
				| T_EVENTOUT fieldType eventOutId
				| T_FIELD fieldType fieldId
				| T_EXPOSEDFIELD fieldType fieldId
				;

routeStatement			: T_ROUTE nodeNameId '.' eventOutId T_TO nodeNameId '.' eventInId
				;

URLList				: mfstringValue
				;
*/

empty				:
				;


/* NODES */

/*
node				: nodeTypeId '{' nodeBody '}'
				| T_SCRIPT '{' scriptBody '}'
				;
*/

node				: T_TRANSFORM        '{' Transform_nodeBody      '}' { printf ("Got a transform node\n");}
				| T_SHAPE            '{' Shape_nodeBody          '}' { printf ("Got a shape node\n");}
				| T_APPEARANCE       '{' Appearance_nodeBody     '}' { printf ("Got an appearance node\n");}
				| T_MATERIAL         '{' Material_nodeBody       '}' { printf ("Got a material node\n");}
				| T_INDEXED_FACE_SET '{' IndexedFaceSet_nodeBody '}' { printf ("Got an indexed face set node\n");}
				| T_COORDINATE       '{' Coordinate_nodeBody     '}' { printf ("Got a coordinate node\n");}
				| T_NORMAL           '{' Normal_nodeBody         '}' { printf ("Got a normal node\n");}
				| T_SCRIPT           '{' scriptBody '}'
				;

/* NORMAL NODE ------------------------------------------------------------- */
Transform_nodeBody		: Transform_nodeBodyElements
				| empty;

Transform_nodeBodyElements	: Transform_nodeBodyElement
				| Transform_nodeBodyElement Transform_nodeBodyElements;

Transform_nodeBodyElement	: T_center sfvec3fValue
				| T_children '[' statements ']'
				| T_rotation sfvec3fValue
				| T_scale sfvec3fValue
				| T_scale_orientation sfrotationValue
				| T_translation sfvec3fValue
				| T_bbox_center sfvec3fValue
				| T_bbox_size sfvec3fValue
				;

/* SHAPE NODE -------------------------------------------------------------- */
Shape_nodeBody			: Shape_nodeBodyElements
				| empty;

Shape_nodeBodyElements		: Shape_nodeBodyElement
				| Shape_nodeBodyElement Shape_nodeBodyElements;

Shape_nodeBodyElement		: T_appearance statements
				| T_geometry statements
				;

/* APPEARANCE NODE --------------------------------------------------------- */
Appearance_nodeBody		: Appearance_nodeBodyElements
				| empty;

Appearance_nodeBodyElements	: Appearance_nodeBodyElement
				| Appearance_nodeBodyElement Appearance_nodeBodyElements;

Appearance_nodeBodyElement	: T_material statements
				| T_texture statements
				| T_texture_transform statements
				;

/* MATERIAL NODE ----------------------------------------------------------- */
Material_nodeBody		: Material_nodeBodyElements
				| empty;

Material_nodeBodyElements	: Material_nodeBodyElement
				| Material_nodeBodyElement Material_nodeBodyElements;

Material_nodeBodyElement	: T_ambient_intensity sffloatValue
				| T_diffuse_color sfcolorValue
				| T_emissive_color sfcolorValue
				| T_shininess sffloatValue
				| T_specular_color sfcolorValue
				| T_transparency sffloatValue
				;

/* INDEXED_FACE_SET NODE --------------------------------------------------- */
IndexedFaceSet_nodeBody		: IndexedFaceSet_nodeBodyElements
				| empty;

IndexedFaceSet_nodeBodyElements	: IndexedFaceSet_nodeBodyElement
				| IndexedFaceSet_nodeBodyElement IndexedFaceSet_nodeBodyElements;

IndexedFaceSet_nodeBodyElement	: T_color statement
				| T_coord statement
				| T_normal statement
				| T_tex_coord statement
				| T_ccw sfboolValue
				| T_color_index mfint32Value
				| T_color_per_vertex sfboolValue
				| T_convex sfboolValue
				| T_coord_index mfint32Value
				| T_crease_angle sffloatValue
				| T_normal_index mfint32Value
				| T_normal_per_vertex sfboolValue
				| T_solid sfboolValue
				| T_tex_coord_index mfint32Value
				;

/* COORDINATE NODE --------------------------------------------------------- */
Coordinate_nodeBody		: Coordinate_nodeBodyElements
				| empty;

Coordinate_nodeBodyElements	: Coordinate_nodeBodyElement
				| Coordinate_nodeBodyElement Coordinate_nodeBodyElements;

Coordinate_nodeBodyElement	: T_point mfvec3fValue
				;

/* NORMAL NODE ------------------------------------------------------------- */
Normal_nodeBody			: Normal_nodeBodyElements
				| empty;

Normal_nodeBodyElements		: Normal_nodeBodyElement
				| Normal_nodeBodyElement Normal_nodeBodyElements;

Normal_nodeBodyElement		: T_vector mfvec3fValue
				;

/* GENERIC NODE ------------------------------------------------------------ */
/*
nodeBody			: nodeBodyElement
				| nodeBodyElement nodeBody
				| empty
				;
*/

scriptBody			: // scriptBodyElement
//				| scriptBodyElement scriptBody
/*				| */ empty
				;

/*
scriptBodyElement		: nodeBodyElement
				| restrictedInterfaceDeclaration
				| T_EVENTIN fieldType eventInId T_IS eventInId
				| T_EVENTOUT fieldType eventOutId T_IS eventOutId
				| T_FIELD fieldType fieldId T_IS fieldId
				;

nodeBodyElement			: //fieldId fieldValue
//				| fieldId T_IS fieldId
//				| eventInId T_IS eventInId
//				| eventOutId T_IS eventOutId
//				| routeStatement
//				| protoStatement
				;
*/

nodeNameId			: Id
				;

/*
nodeTypeId			: Id
				;

fieldId				: Id
				;

eventInId			: Id
				;

eventOutId			: Id
				;
*/

/*
Id				: IDFIRSTCHAR
				| IDFIRSTCHAR IDRESTCHARS
				;
*/

Id				: STRING
				;

/* FIELDS */

/*
fieldType			: MFColor
				| MFFloat
				| MFInt32
				| MFNode
				| MFRotation
				| MFString
				| MFTime
				| MFVec2f
				| MFVec3f
				| SFBool
				| SFColor
				| SFFloat
				| SFImage
				| SFInt32
				| SFNode
				| SFRotation
				| SFString
				| SFTime
				| SFVec2f
				| SFVec3f
				;

fieldValue			:
				| sfboolValue
				| sfcolorValue
				| sffloatValue
				| sfimageValue
				| sfint32Value
				| sfnodeValue
				| sfrotationValue
				| sfstringValue
				| sftimeValue
				| sfvec2fValue
				| sfvec3fValue
				| mfcolorValue
				| mffloatValue
				| mfint32Value
				| mfnodeValue
				| mfrotationValue
				| mfstringValue
				| mftimeValue
				| mfvec2fValue
				| mfvec3fValue
				;
*/

sfboolValue			: T_TRUE
				| T_FALSE
				;

sfcolorValue			: FLOATING FLOATING FLOATING
				;

sffloatValue			: FLOATING
				;

/*
sfimageValue			: image_data
				;

image_data			: INT32
				| image_data INT32
				;
*/

sfint32Value			: INT32
				;

/*
sfnodeValue			: nodeStatement
				| T_NULL
				;
*/

sfrotationValue			: FLOATING FLOATING FLOATING FLOATING
				;

sfstringValue			: STRING
				;

/*
sftimeValue			: DOUBLE
				;

mftimeValue			: sftimeValue
				| '[' ']'
				| '[' sftimeValues ']'
				;

sftimeValues			: sftimeValue
				| sftimeValue sftimeValues
				;

sfvec2fValue			: FLOATING FLOATING
				;
*/

sfvec3fValue			: FLOATING FLOATING FLOATING
				;

/*
mfcolorValue			: sfcolorValue
				| '[' ']'
				| '[' sfcolorValues ']'
				;

sfcolorValues			: sfcolorValue
				| sfcolorValue sfcolorValues
				;

mffloatValue			: sffloatValue
				| '[' ']'
				| '[' sffloatValues ']'
				;

sffloatValues			: sffloatValue
				| sffloatValue sffloatValues
				;
*/

mfint32Value			: sfint32Value
				| '[' ']'
				| '[' sfint32Values ']'
				;

sfint32Values			: sfint32Value
				| sfint32Value sfint32Values
				;

/*
mfnodeValue			: nodeStatement
				| '[' ']'
				| '[' nodeStatements ']'
				;

nodeStatements			: nodeStatement
				| nodeStatement nodeStatements
				;

mfrotationValue			: sfrotationValue
				| '[' ']'
				| '[' sfrotationValues ']'
				;

sfrotationValues		: sfrotationValue
				| sfrotationValue sfrotationValues
				;
*/

/*
mfstringValue			: sfstringValue
				| '[' ']'
				| '[' sfstringValues ']'
				;
*/

sfstringValues			: sfstringValue
				| sfstringValue sfstringValues
				;

/*
mfvec2fValue			: sfvec2fValue
				| '[' ']'
				| '[' sfvec2fValues ']'
				;

sfvec2fValues			: sfvec2fValue
				| sfvec2fValue sfvec2fValues
				;
*/

mfvec3fValue			: sfvec3fValue
				| '[' ']'
				| '[' sfvec3fValues ']'
				;

sfvec3fValues			: sfvec3fValue
				| sfvec3fValue sfvec3fValues
				;


%%

/* ---------------------------------------------------------------------------
 * error routine called by parser library
 */
int vrml_yyerror(s)
const char *s;
{
	printf("ERROR parsing file (%s)\n"
		"    line number: %i\n"
		"    description: '%s'\n",
		vrml_yyfilename, vrml_yylineno, s);
	return(0);
}

int vrml_yywrap()
{
  return 1;
}
