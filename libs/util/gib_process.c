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

#include <string.h>
#include <ctype.h>

#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/cvar.h"

#include "exp.h"

void
GIB_Process_Variable (struct dstring_s *dstr)
{
	int i;
	cvar_t *var;
	
	for (i = 0; dstr->str[i] == '$'; i++);
	i--;
	for (; i >= 0; i--) {
		var = Cvar_FindVar (dstr->str+i+1);
		if (!var)
			return;
		dstr->str[i] = 0;
		dstring_appendstr (dstr, var->string);
	}
}

void
GIB_Process_Variables_All (struct dstring_s *token)
{
	int i, n;
	dstring_t *var = dstring_newstr ();
	
	for (i = 0; token->str[i]; i++) {
		if (token->str[i] == '$') {
			for (n = 1; token->str[i+n] == '$'; n++); // get past $s
			for (; isalnum(token->str[i+n]); n++); // find end of var
			dstring_insert (var, 0, token->str+i, n); // extract it
			GIB_Process_Variable (var);
			dstring_replace (token, i, n, var->str, strlen(var->str));
			i += strlen (var->str) - 1;
			dstring_clearstr (var);
		}
	}
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
