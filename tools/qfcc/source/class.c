/*
	function.c

	QC function support code

	Copyright (C) 2001 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/5/7

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

#include "QF/dstring.h"

#include "qfcc.h"

#include "class.h"
#include "type.h"

class_t *
new_class (const char *name, class_t *base)
{
	class_t    *c = malloc (sizeof (class_t));

	c->name = name;
	c->base = base;
	c->methods.head = 0;
	c->methods.tail = &c->methods.head;
	return c;
}

protocol_t *
new_protocol (const char *name)
{
	protocol_t    *p = malloc (sizeof (protocol_t));

	p->name = name;
	p->methods.head = 0;
	p->methods.tail = &p->methods.head;
	return p;
}

category_t *
new_category (const char *name, class_t *klass)
{
	category_t    *c = malloc (sizeof (category_t));

	c->name = name;
	c->klass = klass;
	c->methods.head = 0;
	c->methods.tail = &c->methods.head;
	return c;
}

class_t *
find_class (const char *name)
{
	return 0;
}

protocol_t *
find_protocol (const char *name)
{
	return 0;
}
