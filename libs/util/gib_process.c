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

static const char rcsid[] =
        "$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/cvar.h"
#include "QF/gib_buffer.h"
#include "QF/gib_parse.h"
#include "QF/gib_vars.h"

#include "exp.h"

int GIB_Process_Variables_All (struct dstring_s *token);

int
GIB_Process_Index (dstring_t *index, unsigned int pos, int *i1, int *i2)
{
	int i, v1, v2;
	char *p;
	
	
	for (i = pos; index->str[i] != ']'; i++)
		if (!index->str[i]) {
			Cbuf_Error ("parse", "Could not find matching [");
			return -1;
		}
	v1 = atoi (index->str+pos+1);
	if (v1 < 0) {
		Cbuf_Error ("index", "Negative index found in sub-string expression");
		return -1;
	}
	if ((p = strchr (index->str+pos, ':'))) {
		v2 = atoi (p+1);
		if (v2 < 0) {
			Cbuf_Error ("index", "Negative index found in sub-string expression");
			return -1;
		}
	} else
		v2 = v1;
	dstring_snip (index, pos, i - pos + 1);
	if (v2 < v1) {
		v1 ^= v2;
		v2 ^= v1;
		v1 ^= v2;
	}
	*i1 = v1;
	*i2 = v2;
	return 0;
}

unsigned int
GIB_Process_Variable (struct dstring_s *dstr, unsigned int pos, qboolean tolerant)
{
	cvar_t *cvar;
	gib_var_t *gvar;
	const char *str;
	char *p, c;
	
	for (p = dstr->str+pos+1; tolerant ? *p : isalnum ((byte)*p) || *p == '_'; p++);
	c = *p;
	*p = 0;
	if ((str = GIB_Var_Get (cbuf_active, dstr->str+pos+1)))	
		; // yay for us
	else if ((gvar = GIB_Var_Get_R (gib_globals, dstr->str+pos+1)))
		str = gvar->value->str;
	else if ((cvar = Cvar_FindVar (dstr->str+pos+1)))
		str = cvar->string;
	else
		str = 0;
	*p = c;
	if (str)
		dstring_replace (dstr, pos, p - dstr->str - pos, str, strlen(str));
	else
		dstring_snip (dstr, pos, p - dstr->str - pos);
	return str ? strlen (str) : 0;
}

int
GIB_Process_Variables_All (struct dstring_s *token)
{
	int i, n, m;
	dstring_t *var = dstring_newstr ();
	char c = 0;
	char *p;
	int i1, i2;
	
	for (i = 0; token->str[i]; i++) {
		if (token->str[i] == '$') {
			if (token->str[i+1] == '{') {
				n = i+1;
				if ((c = GIB_Parse_Match_Brace (token->str, &n))) {
					Cbuf_Error ("parse", "Could not find match for %c", c);
					goto ERROR;
				}
				if (token->str[n+1] == '[')  {
					// Cut index out and put it with the variable
					m = n+1;
					if ((c = GIB_Parse_Match_Index (token->str, &m))) {
						Cbuf_Error ("parse", "Could not find match for %c", c);
						goto ERROR;
					}
					dstring_insert (var, 0, token->str+n+1, m-n);
					dstring_snip (token, n+1, m-n);
				}
				n -= i;
				dstring_insert (var, 0, token->str+i+2, n-2);
				dstring_insertstr (var, 0, "$");
				n++;
			} else {
				for (n = 1; isalnum((byte)token->str[i+n]) ||
							token->str[i+n] == '$' ||
							token->str[i+n] == '_' ||
							token->str[i+n] == '[' ||
							token->str[i+n] == ']' ||
							token->str[i+n] == ':'; n++); // find end of var
				dstring_insert (var, 0, token->str+i, n); // extract it
			}
			for (m = 1; var->str[m]; m++) {
				if (var->str[m] == '$')
					m += GIB_Process_Variable (var, m, false) - 1;
			}
			i1 = -1;
			if (var->str[strlen(var->str)-1] == ']' && (p = strrchr (var->str, '[')))
				if (GIB_Process_Index(var, p-var->str, &i1, &i2)) {
					c = '[';
					goto ERROR;
				}
			GIB_Process_Variable (var, 0, true);
			if (i1 >= 0) {
				if (i1 >= strlen (var->str))
					i1 = strlen(var->str)-1;
				if (i2 >= strlen (var->str))
					i2 = strlen(var->str)-1;
				if (i2 < strlen(var->str)-1) // Snip everthing after index 2
				dstring_snip (var, i2+1, strlen(var->str)-i2-1);
				if (i1 > 0) // Snip everything before index 1
				dstring_snip (var, 0, i1);
			}
			dstring_replace (token, i, n, var->str, strlen(var->str));
			i += strlen (var->str) - 1;
			dstring_clearstr (var);
		}
	}
ERROR:
	dstring_delete (var);
	return c ? -1 : 0;
}

int
GIB_Process_Math (struct dstring_s *token)
{
	double value;
	
	value = EXP_Evaluate (token->str);
	if (EXP_ERROR) {
		// FIXME:  Give real error descriptions
		Cbuf_Error ("math", "Expression \"%s\" caused error %i", token->str, EXP_ERROR);
		return -1;
	} else {
		dstring_clearstr (token);
		dsprintf (token, "%.10g", value);
	}
	return 0;
}

int
GIB_Process_Embedded (struct dstring_s *token)
{
	cbuf_t *sub;
	int i, n;
	char c;
	
	if (GIB_DATA(cbuf_active)->ret.waiting)  {
		if (!GIB_DATA(cbuf_active)->ret.available) {
			GIB_DATA(cbuf_active)->ret.waiting = false;
			Cbuf_Error ("return", "Embedded command did not return a value.");
			return -1;
		}
		i = GIB_DATA(cbuf_active)->ret.token_pos; // Jump to the right place
	} else
		i = 0;
	
	for (; token->str[i]; i++) {
		if (token->str[i] == '`') {
			n = i;
			if ((c = GIB_Parse_Match_Backtick (token->str, &i))) {
				Cbuf_Error ("parse", "Could not find matching %c", c);
				return -1;
			}
			if (GIB_DATA(cbuf_active)->ret.available) {
				dstring_replace (token, n, i-n+1, GIB_DATA(cbuf_active)->ret.retval->str,
				   strlen(GIB_DATA(cbuf_active)->ret.retval->str));
				i = n + strlen(GIB_DATA(cbuf_active)->ret.retval->str) - 1;
				GIB_DATA(cbuf_active)->ret.waiting = false;
				GIB_DATA(cbuf_active)->ret.available = false;
			} else {
				sub = Cbuf_New (&gib_interp);
				GIB_DATA(sub)->type = GIB_BUFFER_PROXY;
				GIB_DATA(sub)->locals = GIB_DATA(cbuf_active)->locals;
				dstring_insert (sub->buf, 0, token->str+n+1, i-n-1);
				if (cbuf_active->down)
					Cbuf_DeleteStack (cbuf_active->down);
				cbuf_active->down = sub;
				sub->up = cbuf_active;
				cbuf_active->state = CBUF_STATE_STACK;
				GIB_DATA(cbuf_active)->ret.waiting = true;
				GIB_DATA(cbuf_active)->ret.token_pos = n;
				return -1;
			}
		}
	}
	return 0;
}

int
GIB_Process_Token (struct dstring_s *token, char delim)
{
	if (delim != '{' && delim != '\"') {
		if (GIB_Process_Embedded (token))
			return -1;
		if (GIB_Process_Variables_All (token))
			return -1;
	}
			
	if (delim == '(')
		if (GIB_Process_Math (token))
			return -1;
	return 0;
}
