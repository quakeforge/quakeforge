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

static __attribute__ ((unused))
const char  rcsid[] = "$Id$";

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "QF/va.h"
#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/cvar.h"
#include "QF/gib_buffer.h"
#include "QF/gib_parse.h"
#include "QF/gib_vars.h"
#include "QF/gib_process.h"
#include "QF/gib_builtin.h"

#include "exp.h"

static int
GIB_Process_Variable (dstring_t * token, unsigned int *i)
{
	hashtab_t  *one = GIB_DATA (cbuf_active)->locals, *two =
		GIB_DATA (cbuf_active)->globals;
	unsigned int n = *i, j, k, start = *i, index = 0, len, len2;
	gib_var_t  *var = 0;
	cvar_t     *cvar;
	char        c;
	const char *str;

	(*i)++;
	if (token->str[*i] == '{') {
		if ((c = GIB_Parse_Match_Brace (token->str, i))) {
			GIB_Error ("Parse", "Could not find match for %c.", c);
			return -1;
		}
		n += 2;
		len = 1;
	} else {
		for (; isalnum ((byte) token->str[*i]) || token->str[*i] == '_';
			 (*i)++);
		if (token->str[*i] == '[') {
			if ((c = GIB_Parse_Match_Index (token->str, i))) {
				GIB_Error ("Parse", "Could not find match for %c.", c);
				return -1;
			} else
				(*i)++;
		}
		n++;
		len = 0;
	}
	c = token->str[*i];
	token->str[*i] = 0;

	for (k = n; token->str[n]; n++)
		if (token->str[n] == '$' || token->str[n] == '#')
			if (GIB_Process_Variable (token, &n))
				return -1;
	index = 0;
	if (n && token->str[n - 1] == ']')
		for (j = n - 1; j; j--)
			if (token->str[j] == '[') {
				index = atoi (token->str + j + 1);
				token->str[j] = 0;
			}
	if ((var = GIB_Var_Get (one, two, token->str + k)) && index < var->size
		&& var->array[index]) {
		if (token->str[start] == '#')
			str = va ("%u", var->size - index);
		else
			str = var->array[index]->str;
	} else if (token->str[start] == '#')
		str = "0";
	else if ((cvar = Cvar_FindVar (token->str + k)))
		str = cvar->string;
	else
		str = "";
	token->str[n] = c;
	len += n - start;
	len2 = strlen (str);
	dstring_replace (token, start, len, str, len2);
	*i = start + len2 - 1;
	return 0;
}

int
GIB_Process_Math (struct dstring_s *token, unsigned int i)
{
	double      value;

	value = EXP_Evaluate (token->str + i);
	if (EXP_ERROR) {
		GIB_Error ("math", "Expression \"%s\" caused an error:\n%s", token->str,
				   EXP_GetErrorMsg ());
		return -1;
	} else {
		token->str[i] = 0;
		token->size = i + 1;
		dasprintf (token, "%.10g", value);
	}
	return 0;
}

int
GIB_Process_Embedded (gib_tree_t * node, cbuf_args_t * args)
{
	unsigned int n, j;
	gib_var_t  *var;
	cvar_t     *cvar;
	gib_tree_t *cur;
	unsigned int index, prev = 0;
	const char *str = node->str;

	for (cur = node->children; cur; cur = cur->next) {
		if (cur->start > prev)
			dstring_appendsubstr (args->argv[args->argc - 1], str + prev,
								  cur->start - prev);
		prev = cur->end;
		if (!cur->str) {
			struct gib_dsarray_s *retvals =
				GIB_DATA (cbuf_active)->stack.values +
				GIB_DATA (cbuf_active)->stack.p - 1;
			if (retvals->size != 1) {
				if (!*args->argv[args->argc - 1]->str)
					args->argc--;
				for (j = 0; j < retvals->size; j++) {
					Cbuf_ArgsAdd (args, retvals->dstrs[j]->str);
					args->argm[args->argc - 1] = node;
				}
				if (str[prev] && retvals->size)	// Still more stuff left?
					Cbuf_ArgsAdd (args, "");
			} else
				dstring_appendstr (args->argv[args->argc - 1],
								   retvals->dstrs[0]->str);
			GIB_Buffer_Pop_Sstack (cbuf_active);
		} else if (cur->flags & TREE_P_EMBED) {
			n = args->argv[args->argc - 1]->size - 1;
			if (cur->delim == '$')
				dstring_appendstr (args->argv[args->argc - 1], "${");
			else
				dstring_appendstr (args->argv[args->argc - 1], "#{");
			dstring_appendstr (args->argv[args->argc - 1], cur->str);
			dstring_appendstr (args->argv[args->argc - 1], "}");
			if (GIB_Process_Variable (args->argv[args->argc - 1], &n))
				return -1;
		} else
			if ((var =
				 GIB_Var_Get_Complex (&GIB_DATA (cbuf_active)->locals,
									  &GIB_DATA (cbuf_active)->globals,
									  (char *) cur->str, &index, false))) {
			if (cur->delim == '$')
				dstring_appendstr (args->argv[args->argc - 1],
								   var->array[index]->str);
			else
				dasprintf (args->argv[args->argc - 1], "%u", var->size - index);
		} else if (cur->delim == '#')
			dstring_appendstr (args->argv[args->argc - 1], "0");
		else if ((cvar = Cvar_FindVar (cur->str)))
			dstring_appendstr (args->argv[args->argc - 1], cvar->string);
	}
	if (str[prev])
		dstring_appendstr (args->argv[args->argc - 1], str + prev);
	return 0;
}

void
GIB_Process_Escapes (char *str)
{
	int         i, j;
	char        c;

	for (i = 0, j = 0; str[i]; j++) {
		if (str[i] == '\\') {
			i++;
			if (isdigit ((byte) str[i]) &&
				isdigit ((byte) str[i + 1]) && isdigit ((byte) str[i + 2])) {
				unsigned int num;

				if ((num =
					 100 * (str[i] - '0') + 10 * (str[i + 1] - '0') +
					 (str[i + 2] - '0')) <= 255) {
					c = (char) num;
					i += 3;
				} else
					c = '\\';
			} else
				switch (str[i++]) {
					case 'n':
						c = '\n';
						break;
					case 't':
						c = '\t';
						break;
					case 'r':
						c = '\r';
						break;
					case '\"':
						c = '"';
						break;
					case '\\':
						c = '\\';
						break;
					default:
						c = '\\';
						i--;
						break;
				}
			str[j] = c;
		} else
			str[j] = str[i++];
	}
	str[j] = 0;
}
