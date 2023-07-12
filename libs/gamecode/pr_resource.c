/*
	pr_resource.c

	progs resource management

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/1/29

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

#include <stdlib.h>

#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/sys.h"

struct pr_resource_s {
	const char *name;
	pr_resource_t *next;
	void *data;
	void (*clear)(progs_t *pr, void *data);
	void (*destroy)(progs_t *pr, void *data);
};

static const char *
resource_get_key (const void *r, void *unused)
{
	return ((pr_resource_t *)r)->name;
}

VISIBLE void
PR_Resources_Init (progs_t *pr)
{
	pr->resource_hash = Hash_NewTable (1021, resource_get_key, 0, 0,
									   pr->hashctx);
	pr->resources = 0;
}

void
PR_Resources_Clear (progs_t *pr)
{
	pr_resource_t *res = pr->resources;
	while (res) {
		res->clear (pr, res->data);
		res = res->next;
	}
}

VISIBLE void
PR_Resources_Shutdown (progs_t *pr)
{
	// Clear resources first in case there are any cross-dependencies
	PR_Resources_Clear (pr);

	pr_resource_t *res = pr->resources;
	while (res) {
		pr_resource_t *t = res->next;
		res->destroy (pr, res->data);
		free (res);
		res = t;
	}
	pr->resources = 0;

	Hash_DelTable (pr->resource_hash);
	pr->resource_hash = 0;
}

VISIBLE void
PR_Resources_Register (progs_t *pr, const char *name, void *data,
					   void (*clear)(progs_t *, void *),
					   void (*destroy)(progs_t *, void *))
{
	pr_resource_t *res = malloc (sizeof (pr_resource_t));
	if (!res)
		Sys_Error ("PR_Resources_Register: out of memory");
	res->name = name;
	res->data = data;
	res->clear = clear;
	res->destroy = destroy;
	res->next = pr->resources;
	pr->resources = res;
	Hash_Add (pr->resource_hash, res);
}

VISIBLE void *
PR_Resources_Find (progs_t *pr, const char *name)
{
	pr_resource_t *res = Hash_Find (pr->resource_hash, name);
	if (!res)
		return 0;
	return res->data;
}
