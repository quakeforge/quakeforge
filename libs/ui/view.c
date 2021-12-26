/*
	view.c

	console view object

	Copyright (C) 2003 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2003/5/5

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/ui/view.h"

static void
setgeometry (view_t *view)
{
	int         i;
	view_t     *par = view->parent;

	if (!par) {
		view->xabs = view->xrel = view->xpos;
		view->yabs = view->yrel = view->ypos;
		if (view->setgeometry)
			view->setgeometry (view);
		for (i = 0; i < view->num_children; i++)
			setgeometry (view->children[i]);
		return;
	}

	switch (view->gravity) {
		case grav_center:
			view->xrel = view->xpos + (par->xlen - view->xlen) / 2;
			view->yrel = view->ypos + (par->ylen - view->ylen) / 2;
			break;
		case grav_north:
			view->xrel = view->xpos + (par->xlen - view->xlen) / 2;
			view->yrel = view->ypos;
			break;
		case grav_northeast:
			view->xrel = par->xlen - view->xpos - view->xlen;
			view->yrel = view->ypos;
			break;
		case grav_east:
			view->xrel = par->xlen - view->xpos - view->xlen;
			view->yrel = view->ypos + (par->ylen - view->ylen) / 2;
			break;
		case grav_southeast:
			view->xrel = par->xlen - view->xpos - view->xlen;
			view->yrel = par->ylen - view->ypos - view->ylen;
			break;
		case grav_south:
			view->xrel = view->xpos + (par->xlen - view->xlen) / 2;
			view->yrel = par->ylen - view->ypos - view->ylen;
			break;
		case grav_southwest:
			view->xrel = view->xpos;
			view->yrel = par->ylen - view->ypos - view->ylen;
			break;
		case grav_west:
			view->xrel = view->xpos;
			view->yrel = view->ypos + (par->ylen - view->ylen) / 2;
			break;
		case grav_northwest:
			view->xrel = view->xpos;
			view->yrel = view->ypos;
			break;
	}
	view->xabs = par->xabs + view->xrel;
	view->yabs = par->yabs + view->yrel;
	if (view->setgeometry)
		view->setgeometry (view);
	for (i = 0; i < view->num_children; i++)
		setgeometry (view->children[i]);
}

VISIBLE view_t *
view_new (int xp, int yp, int xl, int yl, grav_t grav)
{
	view_t     *view = calloc (1, sizeof (view_t));
	view->xpos = xp;
	view->ypos = yp;
	view->xlen = xl;
	view->ylen = yl;
	view->gravity = grav;
	view->visible = 1;
	view->draw = view_draw;
	setgeometry (view);
	return view;
}

VISIBLE void
view_insert (view_t *par, view_t *view, int pos)
{
	view->parent = par;
	if (pos < 0)
		pos = par->num_children + 1 + pos;
	if (pos < 0)
		pos = 0;
	if (pos > par->num_children)
		pos = par->num_children;
	if (par->num_children == par->max_children) {
		par->max_children += 8;
		par->children = realloc (par->children,
								 par->max_children * sizeof (view_t *));
		memset (par->children + par->num_children, 0,
				(par->max_children - par->num_children) * sizeof (view_t *));
	}
	memmove (par->children + pos + 1, par->children + pos,
			 (par->num_children - pos) * sizeof (view_t *));
	par->num_children++;
	par->children[pos] = view;
	setgeometry (view);
}

VISIBLE void
view_add (view_t *par, view_t *view)
{
	view_insert (par, view, -1);
}

VISIBLE void
view_remove (view_t *par, view_t *view)
{
	int        i;

	for (i = 0; i < par->num_children; i++) {
		if (par->children[i] == view) {
			memmove (par->children + i, par->children + i + 1,
					 (par->num_children - i - 1) * sizeof (view_t *));
			par->children [--par->num_children] = 0;
			break;
		}
	}
}

VISIBLE void
view_delete (view_t *view)
{
	if (view->parent)
		view_remove (view->parent, view);
	while (view->num_children)
		view_delete (view->children[0]);
	free (view);
}

VISIBLE void
view_draw (view_t *view)
{
	int         i;

	for (i = 0; i < view->num_children; i++) {
		view_t     *v = view->children[i];
		if (v->visible && v->draw)
			v->draw (v);
	}
}

static void
_resize (view_t *view, int xl, int yl)
{
	int         i, xd, yd;

	xd = xl - view->xlen;
	yd = yl - view->ylen;
	view->xlen = xl;
	view->ylen = yl;
	for (i = 0; i < view->num_children; i++) {
		view_t     *v = view->children[i];

		if (v->resize_x && v->resize_y) {
			_resize (v, v->xlen + xd, v->ylen + yd);
		} else if (v->resize_x) {
			_resize (v, v->xlen + xd, v->ylen);
		} else if (v->resize_y) {
			_resize (v, v->xlen, v->ylen + yd);
		}
	}
}

VISIBLE void
view_resize (view_t *view, int xl, int yl)
{
	_resize (view, xl, yl);
	setgeometry (view);
}

VISIBLE void
view_move (view_t *view, int xp, int yp)
{
	view->xpos = xp;
	view->ypos = yp;
	setgeometry (view);
}

VISIBLE void
view_setgeometry (view_t *view, int xp, int yp, int xl, int yl)
{
	view->xpos = xp;
	view->ypos = yp;
	_resize (view, xl, yl);
	setgeometry (view);
}

VISIBLE void
view_setgravity (view_t *view, grav_t grav)
{
	view->gravity = grav;
	setgeometry (view);
}
