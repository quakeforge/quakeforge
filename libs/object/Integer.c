/*
	Integer.c

	Class for integer values

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

#include "QF/classes/Integer.h"
#include "QF/va.h"

static String *
Integer_ToString_f (Object *self)
{
	return new (String, va("%i", INTEGER(self)->value));
}

static int
Integer_IntValue_f (Number *self)
{
	return INTEGER(self)->value;
}

static double
Integer_DoubleValue_f (Number *self)
{
	return (double) INTEGER(self)->value;
}

static Object *
Integer_Init_f (Object *self, int value)
{
	superInit(Integer, self);
	INTEGER(self)->value = value;
	NUMBER(self)->intValue = Integer_IntValue_f;
	NUMBER(self)->doubleValue = Integer_DoubleValue_f;
	self->toString = Integer_ToString_f;
	return self;
}

static void
Integer_Deinit_f (Object *self)
{
}

classInitFunc(Integer)
{
	classObj(Integer) = new(Class, "Integer", sizeof(Integer), classObj(Number), Integer_Init_f, Integer_Deinit_f, false);
}
