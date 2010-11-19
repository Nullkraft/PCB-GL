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
#include "create.h"
#include "data.h"
#include "error.h"
#include "file.h"
#include "mymem.h"
#include "misc.h"
#include "parse_l.h"
#include "polygon.h"
#include "remove.h"
#include "rtree.h"
#include "strflags.h"
#include "thermal.h"

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
	int		number;
	double		floating;
	char		*string;
	FlagType	flagtype;
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

%token T_SCRIPT

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

%token T_ID
%token T_FIELDTYPE

%token <floating>      FLOAT
%token <floating>      DOUBLE
%token <number>        INT32

%token <string>        STRING
%token <string>        IDFIRSTCHAR
%token <string>        IDRESTCHARS

%%

/* General VRML stuff */

parse				: vrmlScene
				| { printf ("HELLO\n"); }
				| error { YYABORT; }

vrmlScene			: statements
				;

statements			: statement
				| statement statements
				| empty
				;

statement			: nodeStatement
				| protoStatement
				| routeStatement
				;

nodeStatement			: node
				| T_DEF nodeNameId { printf ("Hello world\n");} node
				| T_USE nodeNameId
				;

rootNodeStatement		: node
				| T_DEF nodeNameId node
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

empty				:
				;


/* NODES */

node				: nodeTypeId '{' nodeBody '}'
				| T_SCRIPT '{' scriptBody '}'
				;

nodeBody			: nodeBodyElement
				| nodeBodyElement nodeBody
				| empty
				;

scriptBody			: scriptBodyElement
				| scriptBodyElement scriptBody
				| empty
				;

scriptBodyElement		: nodeBodyElement
				| restrictedInterfaceDeclaration
				| T_EVENTIN fieldType eventInId T_IS eventInId
				| T_EVENTOUT fieldType eventOutId T_IS eventOutId
				| T_FIELD fieldType fieldId T_IS fieldId
				;

nodeBodyElement			: fieldId fieldValue
				| fieldId T_IS fieldId
				| eventInId T_IS eventInId
				| eventOutId T_IS eventOutId
				| routeStatement
				| protoStatement
				;

nodeNameId			: Id
				;

nodeTypeId			: Id
				;

fieldId				: Id
				;

eventInId			: Id
				;

eventOutId			: Id
				;

Id				: IDFIRSTCHAR
				| IDFIRSTCHAR IDRESTCHARS
				;

/* FIELDS */

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

sfboolValue			: T_TRUE
				| T_FALSE
				;

sfcolorValue			: FLOAT FLOAT FLOAT
				;

sffloatValue			: FLOAT
				;

sfimageValue			: image_data
				;

image_data			: INT32
				| image_data INT32
				;

sfint32Value			: INT32
				;

sfnodeValue			: nodeStatement
				| T_NULL
				;

sfrotationValue			: FLOAT FLOAT FLOAT FLOAT
				;

sfstringValue			: string
				;

string				: STRING
				;

sftimeValue			: double
				;

double				: DOUBLE
				;

mftimeValue			: sftimeValue
				| '[' ']'
				| '[' sftimeValues ']'
				;

sftimeValues			: sftimeValue
				| sftimeValue sftimeValues
				;

sfvec2fValue			: FLOAT FLOAT
				;

sfvec3fValue			: FLOAT FLOAT FLOAT
				;

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

mfint32Value			: sfint32Value
				| '[' ']'
				| '[' sfint32Values ']'
				;

sfint32Values			: sfint32Value
				| sfint32Value sfint32Values
				;

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

mfstringValue			: sfstringValue
				| '[' ']'
				| '[' sfstringValues ']'
				;

sfstringValues			: sfstringValue
				| sfstringValue sfstringValues
				;

mfvec2fValue			: sfvec2fValue
				| '[' ']'
				| '[' sfvec2fValues ']'
				;

sfvec2fValues			: sfvec2fValue
				| sfvec2fValue sfvec2fValues
				;

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
	Message("ERROR parsing file (%s)\n"
		"    line number: %i\n"
		"    description: '%s'\n",
		vrml_yyfilename, vrml_yylineno, s);
	return(0);
}

int vrml_yywrap()
{
  return 1;
}
