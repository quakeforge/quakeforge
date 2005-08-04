/*
	Number.c

	Abstract class for numeric values

	Copyright (C) 2003 Brian Koropoff

	Author: Brian Koropoff
	Date: December 06, 2003

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

#include <stdlib.h>

#include "QF/classes/Number.h"

static Object *
Number_Init_f (Object *self)
{
	superInit(String, self);
	return self;
}

static void
Number_Deinit_f (Object *self)
{
}

classInitFunc(Number)
{
	classObj(Number) = new(Class, "Number", sizeof(Number), classObj(Object), Number_Init_f, Number_Deinit_f, true);
}
