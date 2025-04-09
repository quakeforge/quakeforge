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
	// skip over the dreaded BOM if it's present
	if (strncmp (script->p, "\xef\xbb\xbf", 3) == 0) {
		script->p += 3;
	}
	script->unget = false;
	script->error = 0;
}

VISIBLE bool
Script_TokenAvailable (script_t *script, bool crossline)
{
	if (script->error) {
		return false;
	}

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

VISIBLE bool
Script_GetToken (script_t *script, bool crossline)
{
	const char *token_p;

	if (script->error) {
		return false;
	}

	if (script->unget) {				// is a token allready waiting?
		script->unget = false;
		return true;
	}

	if (!Script_TokenAvailable (script, crossline)) {
		if (!crossline) {
			script->error = "line is incomplete";
		}
		return false;
	}

	// copy token
	if (!script->no_quotes && *script->p == '"') {
		int         start_line = script->line;
		script->p++;
		token_p = script->p;
		while (*script->p != '"') {
			if (!*script->p) {
				script->line = start_line;
				script->error = "EOF inside quoted token";
				return false;
			}
			if (*script->p == '\n') {
				if (script->no_quote_lines) {
					script->error = "EOL inside quoted token";
					return false;
				}
				script->line++;
			}
			script->p++;
		}
		dstring_copysubstr (script->token, token_p, script->p - token_p);
		script->p++;
	} else {
		const char *single = "{}()':";

		if (script->single)
			single = script->single;
		token_p = script->p;
		if (strchr (single, *script->p)) {
			script->p++;
		} else {
			while (*script->p && !isspace ((unsigned char) *script->p)
				   && !strchr (single, *script->p))
				script->p++;
		}
		dstring_copysubstr (script->token, token_p, script->p - token_p);
	}

	return true;
}

VISIBLE bool
Script_GetLine (script_t *script)
{
	const char *token_p = script->p;
	while (*script->p) {
		if (script->p[0] == '\n') {
			script->line++;
			script->p++;
			break;
		}
		if (script->p[0] == '/' && script->p[1] == '/') {
			break;
		}
		script->p++;
	}
	if (script->unget) {
		dstring_appendsubstr (script->token, token_p, script->p - token_p);
	} else {
		dstring_copysubstr (script->token, token_p, script->p - token_p);
	}
	return *script->p;
}

VISIBLE void
Script_UngetToken (script_t *script)
{
	if (!script->error) {
		script->unget = true;
	}
}

VISIBLE const char *
Script_Token (script_t *script)
{
	if (script->error) {
		return 0;
	}
	return script->token->str;
}
