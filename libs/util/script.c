/*
	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	See file, 'COPYING', for details.
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include "QF/dstring.h"
#include "QF/script.h"

static void __attribute__ ((format (printf, 2, 3), noreturn))
script_error (script_t *script, const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	fprintf (stderr, "%s:%d: ", script->file, script->line);
	vfprintf (stderr, fmt, args);
	fprintf (stderr, "\n");
	va_end (args);
	exit (1);
}

VISIBLE script_t *
Script_New (void)
{
	script_t *script = calloc (1, sizeof (script_t));
	script->token = dstring_newstr ();
	return script;
}

VISIBLE void
Script_Delete (script_t *script)
{
	dstring_delete (script->token);
	free (script);
}

VISIBLE void
Script_Start (script_t *script, const char *file, const char *data)
{
	script->line = 1;
	script->file = file;
	script->p = data;
	script->unget = false;
}

VISIBLE qboolean
Script_TokenAvailable (script_t *script, qboolean crossline)
{
	if (script->unget)
		return true;
  skipspace:
	while (isspace ((unsigned char) *script->p)) {
		if (*script->p == '\n') {
			if (!crossline)
				return false;
			script->line++;
		}
		script->p++;
	}
	if (!*script->p)
		return false;
	if (*script->p == 26 || *script->p == 4) {
		// end of file characters
		script->p++;
		goto skipspace;
	}

	if (script->p[0] == '/' && script->p[1] == '/') {
		// comment field
		while (*script->p && *script->p != '\n')
			script->p++;
		if (!*script->p)
			return false;
		if (!crossline)
			return false;
		goto skipspace;
	}
	return true;
}

VISIBLE qboolean
Script_GetToken (script_t *script, qboolean crossline)
{
	const char *token_p;

	if (script->unget) {				// is a token allready waiting?
		script->unget = false;
		return true;
	}

	if (!Script_TokenAvailable (script, crossline)) {
		if (!crossline) {
			if (script->error)
				script->error (script, "line is incomplete");
			else
				script_error (script, "line is incomplete");
		}
		return false;
	}

	// copy token
	if (*script->p == '"') {
		int         start_line = script->line;
		script->p++;
		token_p = script->p;
		while (*script->p != '"') {
			if (!*script->p) {
				script->line = start_line;
				if (script->error)
					script->error (script, "EOF inside quoted token");
				else
					script_error (script, "EOF inside quoted token");
				return false;
			}
			if (*script->p == '\n') {
				if (script->no_quote_lines) {
					if (script->error)
						script->error (script, "EOL inside quoted token");
					else
						script_error (script, "EOL inside quoted token");
				}
				script->line++;
			}
			script->p++;
		}
		dstring_copysubstr (script->token, token_p, script->p - token_p);
		script->p++;
	} else {
		token_p = script->p;
		while (*script->p && !isspace ((unsigned char) *script->p))
			script->p++;
		dstring_copysubstr (script->token, token_p, script->p - token_p);
	}

	return true;
}

VISIBLE void
Script_UngetToken (script_t *script)
{
	script->unget = true;
}
