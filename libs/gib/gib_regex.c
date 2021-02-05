/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "regex.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "gib_regex.h"
#include "QF/qtypes.h"

hashtab_t  *gib_regexs;
static char errstr[1024];

static const char *
GIB_Regex_Get_Key (const void *ele, void *ptr)
{
	return ((gib_regex_t *) ele)->regex;
}

static void
GIB_Regex_Free (void *ele, void *ptr)
{
	regfree (&(((gib_regex_t *) ele)->comp));
	free (((gib_regex_t *) ele)->regex);
	free (ele);
}

void
GIB_Regex_Init (void)
{
	gib_regexs = Hash_NewTable (512, GIB_Regex_Get_Key, GIB_Regex_Free, 0, 0);
}

regex_t *
GIB_Regex_Compile (const char *regex, int cflags)
{
	static unsigned int num_regexs = 0;
	gib_regex_t *reg;
	int         res;

	// Already compiled ?
	if ((reg = Hash_Find (gib_regexs, regex))) {
		// Did cflags change?
		if (cflags != reg->cflags) {
			// Try to recompile
			reg->cflags = cflags;
			if ((res = regcomp (&(reg->comp), regex, cflags))) {
				// Blew up, remove from hash
				regerror (res, &(reg->comp), errstr, sizeof (errstr));
				regfree (&(reg->comp));
				free (reg->regex);
				free (Hash_Del (gib_regexs, regex));
				num_regexs--;
				return 0;
			} else
				return &(reg->comp);
		} else
			return &(reg->comp);
	} else {
		reg = calloc (1, sizeof (gib_regex_t));
		if ((res = regcomp (&(reg->comp), regex, cflags))) {
			regerror (res, &(reg->comp), errstr, sizeof (errstr));
			regfree (&(reg->comp));
			free (reg);
			return 0;
		} else {
			reg->cflags = cflags;
			reg->regex = strdup (regex);
			if (++num_regexs > 128) {
				Hash_FlushTable (gib_regexs);
				num_regexs = 0;
			}
			Hash_Add (gib_regexs, reg);
			return &(reg->comp);
		}
	}
}

const char *
GIB_Regex_Error (void)
{
	return errstr;
}

int
GIB_Regex_Translate_Options (const char *opstr)
{
	int         options = 0;

	if (strchr (opstr, 'i'))
		options |= REG_ICASE;
	if (strchr (opstr, 'n'))
		options |= REG_NEWLINE;
	return options;
}

int
GIB_Regex_Translate_Runtime_Options (const char *opstr)
{
	int options = 0;

	if (strchr (opstr, '<'))
		options |= REG_NOTBOL;
	if (strchr (opstr, '>'))
		options |= REG_NOTEOL;
	return options;
}

unsigned int
GIB_Regex_Apply_Match (regmatch_t match[10], dstring_t * dstr, unsigned int ofs,
					   const char *replace)
{
	int         i, start, len, sub, rlen = strlen (replace);
	char       *matched;

	start = match[0].rm_so + ofs;
	len = match[0].rm_eo - match[0].rm_so;

	// Save matched pattern space
	matched = calloc (len + 1, sizeof (char));
	memcpy (matched, dstr->str + start, match[0].rm_eo - match[0].rm_so);

	dstring_replace (dstr, start, len, replace, rlen);
	for (i = start; i < start + rlen; i++) {
		if (dstr->str[i] == '\\') {
			if (dstr->str[i + 1] == '&') {
				dstring_snip (dstr, i, 1);
				rlen--;
				continue;
			}
			if (isdigit ((byte) dstr->str[i + 1])) {
				if (i && dstr->str[i - 1] == '\\') {	// Escaped, not a true
														// back reference
					dstring_snip (dstr, i, 1);
					rlen--;
					continue;
				}
				sub = dstr->str[i + 1] - '0';
				if (match[sub].rm_so != -1) {
					dstring_replace (dstr, i, 2, matched + match[sub].rm_so,
									 match[sub].rm_eo - match[sub].rm_so);
					rlen += match[sub].rm_eo - match[sub].rm_so - 2;
				} else {
					dstring_snip (dstr, i, 2);
					rlen -= 2;
				}
			}
		} else if (dstr->str[i] == '&') {
			dstring_replace (dstr, i, 1, matched, len);
			rlen += strlen (matched) - 1;
		}
	}
	free (matched);
	return rlen + match[0].rm_so;
}
