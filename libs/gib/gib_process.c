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
#include "QF/gib_process.h"
#include "QF/gib_vars.h"

#include "exp.h"

static int
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
	if ((p = strchr (index->str+pos, ':'))) {
		if (*(p+1) == ']')
			v2 = -1;
		else {
			v2 = atoi (p+1);
			if (v2 < 0)
				v2--;
		}
	} else
		v2 = v1;
	dstring_snip (index, pos, i - pos + 1);
	*i1 = v1;
	*i2 = v2;
	return 0;
}

static unsigned int
GIB_Process_Variable (struct dstring_s *dstr, unsigned int pos, qboolean tolerant)
{
	cvar_t *cvar;
	const char *str;
	char *p, c;
	
	for (p = dstr->str+pos+1; tolerant ? *p : isalnum ((byte)*p) || *p == '_'; p++);
	c = *p;
	*p = 0;
	if ((str = GIB_Var_Get_Local (cbuf_active, dstr->str+pos+1)))	
		; // yay for us
	else if ((str = GIB_Var_Get_Global (dstr->str+pos+1)))
		; // yay again
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
GIB_Process_Math (struct dstring_s *token)
{
	double value;
	
	value = EXP_Evaluate (token->str);
	if (EXP_ERROR) {
		Cbuf_Error ("math", "Expression \"%s\" caused an error:\n%s", token->str, EXP_GetErrorMsg());
		return -1;
	} else {
		dstring_clearstr (token);
		dsprintf (token, "%.10g", value);
	}
	return 0;
}

static int
GIB_Process_Embedded (struct dstring_s *token)
{
	cbuf_t *sub;
	int i, n, m, i1, i2, ofs = 0;
	char c = 0, *p;
	dstring_t *var = dstring_newstr ();
	qboolean index;
	
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
		if (token->str[i] == '`' || (token->str[i] == '$' && token->str[i+1] == '(')) {
			n = i;
			switch (token->str[i]) {
				case '`':
					if ((c = GIB_Parse_Match_Backtick (token->str, &i))) {
						Cbuf_Error ("parse", "Could not find matching %c", c);
						return -1;
					}
					break;
				case '$':
					i++;
					ofs = 1;
					if ((c = GIB_Parse_Match_Paren (token->str, &i))) {
						Cbuf_Error ("parse", "Could not find matching %c", c);
						return -1;
					}
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
				dstring_insert (sub->buf, 0, token->str+n+ofs+1, i-n-ofs-1);
				if (cbuf_active->down)
					Cbuf_DeleteStack (cbuf_active->down);
				cbuf_active->down = sub;
				sub->up = cbuf_active;
				cbuf_active->state = CBUF_STATE_STACK;
				GIB_DATA(cbuf_active)->ret.waiting = true;
				GIB_DATA(cbuf_active)->ret.token_pos = n;
				return -1;
			}
		} else if (token->str[i] == '$') {
			index = false;
			if (token->str[i+1] == '{') {
				n = i+1;
				if ((c = GIB_Parse_Match_Brace (token->str, &n))) {
					Cbuf_Error ("parse", "Could not find match for %c", c);
					goto ERROR;
				}
				if (token->str[n+1] == '[')  {
					// Cut index out and put it with the variable
					m = n+1;
					index = true;
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
				  token->str[i+n] == ':'; n++) {
					if (token->str[i+n] == '[') {
						while (token->str[i+n] && token->str[i+n] != ']')
							n++;
						if (!token->str[i+n]) {
							Cbuf_Error ("parse", "Could not find match for [");
							c = '[';
							goto ERROR;
						}
					}
				}
				dstring_insert (var, 0, token->str+i, n); // extract it
			}
			for (m = 1; var->str[m]; m++) {
				if (var->str[m] == '$')
					m += GIB_Process_Variable (var, m, false) - 1;
			}
			i1 = -1;
			if (var->str[strlen(var->str)-1] == ']' && (p = strrchr (var->str, '['))) {
				index = true;
				if (GIB_Process_Index(var, p-var->str, &i1, &i2)) {
					c = '[';
					goto ERROR;
				}
			}
			GIB_Process_Variable (var, 0, true);
			if (index) {
				if (i1 < 0) {
					i1 += strlen(var->str);
					if (i1 < 0)
						i1 = 0;
				} else if (i1 >= strlen (var->str))
					i1 = strlen(var->str)-1;
				if (i2 < 0) {
					i2 += strlen(var->str);
					if (i2 < 0)
						i2 = 0;
				} else if (i2 >= strlen (var->str))
					i2 = strlen(var->str)-1;
				if (i2 < i1)
					dstring_clearstr (var);
				else {	
					if (i2 < strlen(var->str)-1) // Snip everthing after index 2
						dstring_snip (var, i2+1, strlen(var->str)-i2-1);
					if (i1 > 0) // Snip everything before index 1
						dstring_snip (var, 0, i1);
				}
			}
			dstring_replace (token, i, n, var->str, strlen(var->str));
			i += strlen (var->str) - 1;
			dstring_clearstr (var);
		}
	}
	return 0;
	dstring_delete (var);
ERROR:
	dstring_delete (var);
	return c ? -1 : 0;
}

static void
GIB_Process_Escapes (dstring_t *token)
{
	int i;
	for (i = 0; token->str[i]; i++) {
		if (token->str[i] == '\\') {
			if (strlen(token->str+i+1) > 2 &&
				isdigit ((byte) token->str[i+1]) &&
			    isdigit ((byte) token->str[i+2]) &&
			    isdigit ((byte) token->str[i+3])) {
					unsigned int num;
					num = 100 * (token->str[i+1] - '0') + 10 * (token->str[i+2] - '0') + (token->str[i+3] - '0');
					if (num > 255)
						dstring_snip (token, i, 4);
					else {
						dstring_snip (token, i, 3);
						token->str[i] = (char) num;
					}
			} else switch (token->str[i+1]) {
				case 'n':
					token->str[i+1] = '\n';
					goto snip;
				case 't':
					token->str[i+1] = '\t';
					goto snip;
				case 'r':
					token->str[i+1] = '\r';
					goto snip;
				case '\"':
				case '\\':
					goto snip;
				default:
					break;
				snip:
					dstring_snip (token, i, 1);
			}
		}
	}
}

int
GIB_Process_Token (dstring_t *token, char delim)
{
	if (delim != '{' && delim != '\"')
		if (GIB_Process_Embedded (token))
			return -1;
	if (delim == '(')
		if (GIB_Process_Math (token))
			return -1;
	if (delim == '\"')
		GIB_Process_Escapes (token);
	return 0;
}
