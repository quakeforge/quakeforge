
/*
 *  Linux Frame Buffer Device Configuration
 *
 *  © Copyright 1995-1998 by Geert Uytterhoeven
 *		       (Geert.Uytterhoeven@cs.kuleuven.ac.be)
 *
 *  --------------------------------------------------------------------------
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of the Linux
 *  distribution for more details.
 */


%{

#include <string.h>
#include <stdlib.h>

#define Die Sys_Error

#include "fbset.h"
#include "fbset_modes_y.h"

#ifndef YY_PROTO
# define YY_PROTO(x) x
#else
# define YY_FLEX_REALLOC_HACK
#endif
int yyget_lineno  (void);
FILE *yyget_in  (void);
FILE *yyget_out  (void);
int yyget_leng  (void);
char *yyget_text  (void);
void yyset_lineno (int  line_number);
void yyset_in (FILE *  in_str);
void yyset_out (FILE *  out_str);
int yyget_debug  (void);
void yyset_debug (int  bdebug);
int yylex_destroy (void);

#define YY_NO_UNPUT
#define YY_DECL int yylex YY_PROTO(( void ))
YY_DECL;

struct keyword {
	const char *name;
	int token;
	int value;
};

static struct keyword keywords[] = {
	{ "mode", MODE, 0 },
	{ "geometry", GEOMETRY, 0 },
	{ "timings", TIMINGS, 0 },
	{ "hsync", HSYNC, 0 },
	{ "vsync", VSYNC, 0 },
	{ "csync", CSYNC, 0 },
	{ "gsync", GSYNC, 0 },
	{ "extsync", EXTSYNC, 0 },
	{ "bcast", BCAST, 0 },
	{ "laced", LACED, 0 },
	{ "double", DOUBLE, 0 },
	{ "rgba", RGBA, 0 },
	{ "nonstd", NONSTD, 0 },
	{ "accel", ACCEL, 0 },
	{ "grayscale", GRAYSCALE, 0 },
	{ "endmode", ENDMODE, 0 },
	{ "low", POLARITY, LOW },
	{ "high", POLARITY, HIGH },
	{ "false", BOOLEAN, FALSE },
	{ "true", BOOLEAN, TRUE },
	{ "", -1, 0 }
};

int line = 1;


void yyerror(const char *s);
void yyerror(const char *s)
{
	Die("%s:%d: %s", Opt_modedb, line, s);
}


static int yywrap(void)
{
	return 1;
}


static int FindToken(const char *s)
{
	int i;

	for (i = 0; keywords[i].token > 0; i++)
	if (!strcasecmp(s, keywords[i].name)) {
		yylval.int_val = keywords[i].value;
		return keywords[i].token;
	}
	Die("%s:%d: Unknown keyword `%s'", Opt_modedb, line, s);
}


static const char *CopyString(const char *s)
{
	int len;
	char *s2;

	len = strlen(s)-2;
	if (!(s2 = malloc(len+1)))
	Die("No memory");
	strncpy(s2, s+1, len);
	s2[len] = '\0';
	return s2;
}


%}

keyword	[a-zA-Z][a-zA-Z0-9]*
number	[0-9]*
string	\"[^\"\n]*\"
comment	\#([^\n]*)
space	[ \t]+
junk	.

%%

{keyword}   {
		return FindToken(yytext);
		}

{number}    {
		yylval.int_val = strtoul(yytext, NULL, 0);
		return NUMBER;
		}

{string}    {
		yylval.string = CopyString(yytext);
		return STRING;
		}

{comment}$  break;

{space}	    break;

\n	    {
		line++;
		break;
		}

{junk}	    {
		Die("%s:%d: Invalid token `%s'", Opt_modedb, line, yytext);
		}

%%

#ifdef YY_FLEX_REALLOC_HACK
#else
static __attribute__ ((used)) void (*yyunput_hack)(int, char*) = yyunput;
static __attribute__ ((used)) int (*input_hack)(void) = input;
#endif
