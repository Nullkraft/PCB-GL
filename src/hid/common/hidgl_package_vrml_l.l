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


%}

IDFIRSTCHAR		[^0-9 "#'+,-.\[\\\]{}]
IDRESTCHARS		[^ "#',.\[\\\]{}]
FLOAT			([+/-]?((([0-9]+(\.)?)|([0-9]*\.[0-9]+))([eE][+\-]?[0-9]+)?))
DOUBLE			([+/-]?((([0-9]+(\.)?)|([0-9]*\.[0-9]+))([eE][+\-]?[0-9]+)?))
INT32			([+\-]?(([0-9]+)|(0[xX][0-9a-fA-F]+)))
STRING			".*" ... double-quotes must be \", backslashes must be \\...

DECIMAL                 -?[1-9][0-9]*|0
STRINGCHAR              ([^"\n\r\\]|\\.)

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

{DOUBLE}		{
					vrml_yylval.floating = strtod (vrml_yytext, NULL);
					return DOUBLE;
			}


\"{STRINGCHAR}*\"	{
						char	*p1, *p2;

							/* return NULL on empty string */
						if (vrml_yyleng == 2)
						{
							vrml_yylval.string = NULL;
							return(STRING);
						}

							/* allocate memory and copy string;
							 * stringlength is counted and copied without
							 * leading and trailing '"'
							 */
						vrml_yyleng -= 2;
						vrml_yylval.string = MyCalloc(vrml_yyleng+1, sizeof(char), "LEX");
						p1 = (char *) (vrml_yytext +1);
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
						return(STRING);
					}
#.*					{}
[ \t,]+				{}
[\n]				{
#ifndef FLEX_SCANNER
						vrml_yylineno++;
#endif
					}
[\r]				{}
.					{ return(*vrml_yytext); }

%%

extern int vrml_yydebug;

/* ---------------------------------------------------------------------------
 * sets up the preprocessor command
 */
static int Parse(char *filename)
{
  int retval;
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

  /* We need to save the data temporarily because lex-yacc are able
   * to break the application if the input file has an illegal format.
   * It's not necessary if the system supports the call of functions
   * on termination.
   */

  vrml_yydebug = 1;
  printf ("Entering parser\n");
#if !defined(HAS_ATEXIT) && !defined(HAS_ON_EXIT)
  if (PCB)
    SaveTMPData();
  retval = vrml_yyparse();
  RemoveTMPData();
#else
  retval = vrml_yyparse();
#endif
  printf ("Leaving parser\n");

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
