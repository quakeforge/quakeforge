/*
	Double.c

	Class for double values

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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#include <stdlib.h>

#include "QF/classes/Double.h"
#include "QF/va.h"

static String *
Double_ToString_f (Object *self)
{
	return new(String, va("%f", DOUBLE(self)->value));
}

static int
Double_IntValue_f (Number *self)
{
	return DOUBLE(self)->value;
}

static double
Double_DoubleValue_f (Number *self)
{
	return (int) DOUBLE(self)->value;
}

static Object *
Double_Init_f (Object *self, double value)
{
	superInit(Double, self);
	DOUBLE(self)->value = value;
	NUMBER(self)->intValue = Double_IntValue_f;
	NUMBER(self)->doubleValue = Double_DoubleValue_f;
	self->toString = Double_ToString_f;
	return self;
}

static void
Double_Deinit_f (Object *self)
{
}

classInitFunc(Double)
{
	classObj(Double) = new(Class, "Double", sizeof(Double), classObj(Number), Double_Init_f, Double_Deinit_f, false);
}
