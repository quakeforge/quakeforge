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
#include "QF/hash.h"

#include "qfcc.h"

#include "class.h"
#include "method.h"
#include "type.h"

static hashtab_t *class_hash;
static hashtab_t *protocol_hash;

static const char *
class_get_key (void *class, void *unused)
{
	return ((class_t *) class)->name;
}

static const char *
protocol_get_key (void *protocol, void *unused)
{
	return ((protocol_t *) protocol)->name;
}

class_t *
get_class (const char *name, int create)
{
	class_t    *c;

	if (!class_hash)
		class_hash = Hash_NewTable (1021, class_get_key, 0, 0);
	if (name) {
		c = Hash_Find (class_hash, name);
		if (c || !create)
			return c;
	}

	c = calloc (sizeof (class_t), 1);
	c->name = name;
	Hash_Add (class_hash, c);
	return c;
}

void
class_add_methods (class_t *class, methodlist_t *methods)
{
	if (!methods)
		return;
	if (!class->methods)
		class->methods = new_methodlist ();
	*class->methods->tail = methods->head;
	class->methods->tail = methods->tail;
	free (methods);
}

void
class_add_protocol_methods (class_t *class, expr_t *protocols)
{
	expr_t     *e;
	protocol_t *p;

	if (!protocol_hash)
		protocol_hash = Hash_NewTable (1021, protocol_get_key, 0, 0);
	if (!class->methods)
		class->methods = new_methodlist ();

	for (e = protocols; e; e = e->next) {
		if (!(p = get_protocol (e->e.string_val, 0))) {
			error (e, "undefined protocol `%s'", e->e.string_val);
			continue;
		}
		copy_methods (class->methods, p->methods);
	}
}

def_t *
class_def (class_t *class)
{
	return PR_GetDef (&type_Class, class->name, 0, &numpr_globals);
}

protocol_t *
get_protocol (const char *name, int create)
{
	protocol_t    *p;

	if (name) {
		p = Hash_Find (protocol_hash, name);
		if (p || !create)
			return p;
	}

	p = calloc (sizeof (protocol_t), 1);
	p->name = name;
	Hash_Add (protocol_hash, p);
	return p;
}

void
protocol_add_methods (protocol_t *protocol, methodlist_t *methods)
{
	if (!methods)
		return;
	if (!protocol->methods)
		protocol->methods = new_methodlist ();
	*protocol->methods->tail = methods->head;
	protocol->methods->tail = methods->tail;
	free (methods);
}

void
protocol_add_protocol_methods (protocol_t *protocol, expr_t *protocols)
{
	expr_t     *e;
	protocol_t *p;

	if (!protocol->methods)
		protocol->methods = new_methodlist ();

	for (e = protocols; e; e = e->next) {
		if (!(p = get_protocol (e->e.string_val, 0))) {
			error (e, "undefined protocol `%s'", e->e.string_val);
			continue;
		}
		copy_methods (protocol->methods, p->methods);
	}
}

def_t *
protocol_def (protocol_t *protocol)
{
	return PR_GetDef (&type_Protocol, protocol->name, 0, &numpr_globals);
}
