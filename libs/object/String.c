/*
	String.c

	Class for containing immutable strings

	Copyright (C) 2003 Brian Koropoff

	Author: Brian Koropoff
	Date: December 03, 2003

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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/classes/String.h"

static String *
String_ToString_f (Object *self)
{
	return STRING(self);
}

static Object *
String_Init_f (Object *self, const char *value)
{
	superInit(String, self);
	self->toString = String_ToString_f;
	STRING(self)->str = strdup (value);
	return self;
}

static void
String_Deinit_f (Object *self)
{
	free((void *)STRING(self)->str);
}

classInitFunc(String)
{
	classObj(String) = new(Class, "String", sizeof(String), classObj(Object), String_Init_f, String_Deinit_f, false);
}
