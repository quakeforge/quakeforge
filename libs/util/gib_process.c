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

#include <QF/dstring.h>
#include <QF/cvar.h>

void
GIB_Process_Variable (struct dstring_s *token)
{
	int i;
	cvar_t *var;
	
	for (i = 0; token->str[i] == '$'; i++);
	i--;
	for (; i >= 0; i--) {
		var = Cvar_FindVar (token->str+i+1);
		if (!var)
			return;
		token->str[i] = 0;
		dstring_appendstr (token, var->string);
	}
}
