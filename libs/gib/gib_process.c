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

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "QF/va.h"
#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/cvar.h"
#include "QF/gib.h"

#include "exp.h"
#include "gib_buffer.h"
#include "gib_vars.h"
#include "gib_process.h"

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
		} else if (cur->flags & TREE_A_EMBED) {
			n = args->argv[args->argc - 1]->size - 1;
			dstring_appendstr (args->argv[args->argc - 1], cur->str);
			var = GIB_Var_Get_Very_Complex (&GIB_DATA (cbuf_active)->locals,
									&GIB_DATA (cbuf_active)->globals,
									args->argv[args->argc - 1], n, &index, false);
			cvar = Cvar_FindVar (args->argv[args->argc - 1]->str);
			args->argv[args->argc - 1]->size = n+1;
			args->argv[args->argc - 1]->str[n] = 0;
			if (var) {
				if (cur->delim == '#')
					dasprintf (args->argv[args->argc - 1], "%u", var->size);
				else
					dstring_appendstr (args->argv[args->argc - 1], var->array[index].value->str);
			} else if (cur->delim == '#')
				dstring_appendstr (args->argv[args->argc - 1], "0");
			else if (cvar)
				dstring_appendstr (args->argv[args->argc - 1],
								   Cvar_VarString (cvar));
		} else if ((var = GIB_Var_Get_Complex (&GIB_DATA (cbuf_active)->locals,
									  &GIB_DATA (cbuf_active)->globals,
									  (char *) cur->str, &index, false))) {
			if (cur->delim == '$')
				dstring_appendstr (args->argv[args->argc - 1],
								   var->array[index].value->str);
			else
				dasprintf (args->argv[args->argc - 1], "%u", var->size - index);
		} else if (cur->delim == '#')
			dstring_appendstr (args->argv[args->argc - 1], "0");
		else if ((cvar = Cvar_FindVar (cur->str)))
			dstring_appendstr (args->argv[args->argc - 1],
							   Cvar_VarString (cvar));
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
