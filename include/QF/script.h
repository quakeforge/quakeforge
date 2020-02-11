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
#ifndef __QF_script_h
#define __QF_script_h

/** \defgroup script Scripts
	\ingroup utils
	Line oriented script parsing. Multiple scripts being parsed at the same
	time is supported.
*/
///@{

#include "QF/qtypes.h"

typedef struct script_s {
	/// The current (or next when unget is true) token
	struct dstring_s  *token;
	/// True if the last token has been pushed back.
	qboolean    unget;
	/// current position within the script
	const char *p;
	/// name of the file being processed. used only for error reporting
	const char *file;
	/// line number of the file being processed. used only for error reporting
	/// but updated internally.
	int         line;
	/// if set, will be called instead of the internal error handler
	void      (*error)(struct script_s *script, const char *msg);
	/// if set, multi line quoted tokens will be treated as errors
	int         no_quote_lines;
	/// if set, characters in this string will always be lexed as single
	/// character tokens. If not set, defaults to "{}()':". Set to ""
	/// (empty string) to disable. Not set by default.
	const char *single;
} script_t;

/** Return a new script_t object.
	\return A new, blank, script object. Use Script_Start() to initialize.
*/
script_t *Script_New (void);

/** Delete a script_t object.
	\param script The script_t object to be deleted
	Does not free the memory passed to Script_Start().
*/
void Script_Delete (script_t *script);

/** Prepare a script_t object for parsing.
	The caller is responsible for freeing the memory associated with file and
	data when parsing is complete.
	\param script The script_t object being parsed
	\param file Name of the file being parsed. used only for error reporting
	\param data The script to be parsed
*/
void Script_Start (script_t *script, const char *file, const char *data);

/** Check if a new token is available.
	\param script The script_t object being parsed
	\param crossline True to allow passing \n
	\return True if a token is available, false if end of file
	        or end of line (if crossline is false) has been hit
*/
qboolean Script_TokenAvailable (script_t *script, qboolean crossline);

/** Get the next token. Generates an error and exits the program if no token
	is available and crossline is false.
	\param script The script_t object being parsed
	\param crossline True to allow passing \n
	\return True on success, false on failure (no token available)
*/
qboolean Script_GetToken (script_t *script, qboolean crossline);

/** Unget the current token. Only one level of unget is supported.
	\param script The script_t object being parsed
*/
void Script_UngetToken (script_t *script);

/** Return a pointer to the current token.
	\param script The script_t object being parsed
*/
const char *Script_Token (script_t *script) __attribute__((pure));

///@}

#endif//__QF_script_h
