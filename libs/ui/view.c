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

#include "QF/cexpr.h"
#include "QF/mathlib.h"

#define IMPLEMENT_VIEW_Funcs
#include "QF/ui/view.h"

static exprenum_t grav_t_enum;
exprtype_t grav_t_type = {
	.name = "grav_t",
	.size = sizeof (grav_t),
	.data = &grav_t_enum,
	.get_string = cexpr_enum_get_string,
};
static grav_t grav_t_values[] = {
	grav_center,
	grav_north,
	grav_northeast,
	grav_east,
	grav_southeast,
	grav_south,
	grav_southwest,
	grav_west,
	grav_northwest,
};
static exprsym_t grav_t_symbols[] = {
	{ "center",    &grav_t_type, grav_t_values + grav_center    },
	{ "north",     &grav_t_type, grav_t_values + grav_north     },
	{ "northeast", &grav_t_type, grav_t_values + grav_northeast },
	{ "east",      &grav_t_type, grav_t_values + grav_east      },
	{ "southeast", &grav_t_type, grav_t_values + grav_southeast },
	{ "south",     &grav_t_type, grav_t_values + grav_south     },
	{ "southwest", &grav_t_type, grav_t_values + grav_southwest },
	{ "west",      &grav_t_type, grav_t_values + grav_west      },
	{ "northwest", &grav_t_type, grav_t_values + grav_northwest },
	{}
};
static exprtab_t grav_t_symtab = {
	grav_t_symbols,
};
static exprenum_t grav_t_enum = {
	&grav_t_type,
	&grav_t_symtab,
};

static void
view_modified_init (void *_modified)
{
	byte       *modified = _modified;
	*modified = 1;
}

static const component_t view_components[view_type_count] = {
	[view_pos] = {
		.size = sizeof (view_pos_t),
		.name = "pos",
	},
	[view_len] = {
		.size = sizeof (view_pos_t),
		.name = "len",
	},
	[view_abs] = {
		.size = sizeof (view_pos_t),
		.name = "abs",
	},
	[view_rel] = {
		.size = sizeof (view_pos_t),
		.name = "rel",
	},
	[view_oldlen] = {
		.size = sizeof (view_pos_t),
		.name = "oldlen",
	},
	[view_control] = {
		.size = sizeof (viewcont_t),
		.name = "control",
	},
	[view_modified] = {
		.size = sizeof (byte),
		.create = view_modified_init,
		.name = "modified",
	},
	[view_onresize] = {
		.size = sizeof (view_resize_f),
		.name = "onresize",
	},
	[view_onmove] = {
		.size = sizeof (view_move_f),
		.name = "onmove",
	},
};

static const hierarchy_type_t view_type = {
	.num_components = view_type_count,
	.components = view_components,
};

view_t
View_New (ecs_registry_t *reg, view_t parent)
{
	uint32_t    view = ECS_NewEntity (reg);
	hierref_t  *ref = Ent_AddComponent (view, reg->href_comp, reg);

	if (parent.reg && parent.id != nullent) {
		hierref_t  *pref = View_GetRef (parent);
		ref->hierarchy = pref->hierarchy;
		ref->index = Hierarchy_InsertHierarchy (pref->hierarchy, 0,
												pref->index, 0);
	} else {
		ref->hierarchy = Hierarchy_New (reg, &view_type, 1);
		ref->index = 0;
	}
	ref->hierarchy->ent[ref->index] = view;
	return (view_t) { .reg = reg, .id = view, .comp = reg->href_comp };
}

void
View_UpdateHierarchy (view_t view)
{
	__auto_type ref = View_GetRef (view);
	hierarchy_t *h = ref->hierarchy;

	byte       *modified = h->components[view_modified];
	view_pos_t *pos = h->components[view_pos];
	view_pos_t *len = h->components[view_len];
	view_pos_t *abs = h->components[view_abs];
	view_pos_t *rel = h->components[view_rel];
	view_pos_t *oldlen = h->components[view_oldlen];
	viewcont_t *cont = h->components[view_control];
	view_resize_f *onresize = h->components[view_onresize];
	view_resize_f *onmove = h->components[view_onmove];
	uint32_t   *parent = h->parentIndex;
	uint32_t   *id = h->ent;

	if (abs[0].x != pos[0].x || abs[0].y != pos[0].y) {
		modified[0] = 1;
		abs[0] = pos[0];
		rel[0] = pos[0];
	}
	for (uint32_t i = 1; i < h->num_objects; i++) {
		uint32_t    par = parent[i];
		if (!(modified[i] & 2) && (modified[par] & 2)
			&& (cont[i].resize_x || cont[i].resize_y)) {
			int         dx = len[par].x - oldlen[par].x;
			int         dy = len[par].y - oldlen[par].y;
			if (cont[i].resize_x) {
				len[i].x += dx;
			}
			if (cont[i].resize_y) {
				len[i].y += dy;
			}
			modified[i] |= 2;	// propogate resize modifications
		}
		if (modified[i] || modified[par]) {
			modified[i] |= 1;	// propogate motion modifications
			switch (cont[i].gravity) {
				case grav_center:
					rel[i].x = pos[i].x + (len[par].x - len[i].x) / 2;
					rel[i].y = pos[i].y + (len[par].y - len[i].y) / 2;
					break;
				case grav_north:
					rel[i].x = pos[i].x + (len[par].x - len[i].x) / 2;
					rel[i].y = pos[i].y;
					break;
				case grav_northeast:
					rel[i].x = len[par].x - pos[i].x - len[i].x;
					rel[i].y = pos[i].y;
					break;
				case grav_east:
					rel[i].x = len[par].x - pos[i].x - len[i].x;
					rel[i].y = pos[i].y + (len[par].y - len[i].y) / 2;
					break;
				case grav_southeast:
					rel[i].x = len[par].x - pos[i].x - len[i].x;
					rel[i].y = len[par].y - pos[i].y - len[i].y;
					break;
				case grav_south:
					rel[i].x = pos[i].x + (len[par].x - len[i].x) / 2;
					rel[i].y = len[par].y - pos[i].y - len[i].y;
					break;
				case grav_southwest:
					rel[i].x = pos[i].x;
					rel[i].y = len[par].y - pos[i].y - len[i].y;
					break;
				case grav_west:
					rel[i].x = pos[i].x;
					rel[i].y = pos[i].y + (len[par].y - len[i].y) / 2;
					break;
				case grav_northwest:
					rel[i].x = pos[i].x;
					rel[i].y = pos[i].y;
					break;
				case grav_flow:
					break;
			}
			abs[i].x = abs[par].x + rel[i].x;
			abs[i].y = abs[par].y + rel[i].y;
		}
	}
	for (uint32_t i = 0; i < h->num_objects; i++) {
		if ((modified[i] & 2) && onresize[i]) {
			view_t      v = { .reg = view.reg, .id = id[i], .comp = view.comp };
			onresize[i] (v, len[i]);
		}
		if (modified[i] & 2) {
			oldlen[i] = len[i];
		}
		if ((modified[i] & 1) && onmove[i]) {
			view_t      v = { .reg = view.reg, .id = id[i], .comp = view.comp };
			onresize[i] (v, abs[i]);
		}
		modified[i] = 0;
	}
}

void
View_SetParent (view_t view, view_t parent)
{
	if (parent.reg && parent.id != nullent) {
		__auto_type ref = View_GetRef (view);
		__auto_type vref = *ref;
		__auto_type pref = View_GetRef (parent);
		ref->index = Hierarchy_InsertHierarchy (pref->hierarchy,
												vref.hierarchy,
												pref->index, vref.index);
		ref->hierarchy = pref->hierarchy;
		Hierarchy_RemoveHierarchy (vref.hierarchy, vref.index);
		if (!vref.hierarchy->num_objects) {
			Hierarchy_Delete (vref.hierarchy);
		}
	} else {
		__auto_type ref = View_GetRef (view);
		__auto_type vref = *ref;
		if (!vref.index) {
			return;
		}
		ref->hierarchy = Hierarchy_New (view.reg, &view_type, 0);
		Hierarchy_InsertHierarchy (ref->hierarchy, vref.hierarchy, nullent,
								   vref.index);
		Hierarchy_RemoveHierarchy (vref.hierarchy, vref.index);
		if (!vref.hierarchy->num_objects) {
			Hierarchy_Delete (vref.hierarchy);
		}
	}
	__auto_type ref = View_GetRef (view);
	hierarchy_t *h = ref->hierarchy;
	byte       *modified = h->components[view_modified];
	modified[ref->index] = 1;
	View_UpdateHierarchy (view);
}
#if 0
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
		case grav_flow:
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
view_new_data (int xp, int yp, int xl, int yl, grav_t grav, void *data)
{
	view_t     *view = calloc (1, sizeof (view_t));
	view->xpos = xp;
	view->ypos = yp;
	view->xlen = xl;
	view->ylen = yl;
	view->gravity = grav;
	view->visible = 1;
	view->draw = view_draw;
	view->data = data;
	setgeometry (view);
	return view;
}

VISIBLE view_t *
view_new (int xp, int yp, int xl, int yl, grav_t grav)
{
	return view_new_data (xp, yp, xl, yl, grav, 0);
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
	free (view->children);
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

typedef struct flowline_s {
	struct flowline_s *next;
	int         first_child;
	int         child_count;
	int         cursor;
	int         height;
} flowline_t;

#define NEXT_LINE(line, child_index) \
	do { \
		line->next = alloca (sizeof (flowline_t)); \
		memset (line->next, 0, sizeof (flowline_t)); \
		line = line->next; \
		line->first_child = child_index; \
	} while (0)

static void
flow_right (view_t *view, void (*set_rows) (view_t *, flowline_t *))
{
	flowline_t  flowline = {};
	flowline_t *line = &flowline;
	for (int i = 0; i < view->num_children; i++) {
		view_t     *child = view->children[i];
		if (line->cursor && line->cursor + child->xlen > view->xlen) {
			NEXT_LINE(line, i);
		}
		child->xpos = line->cursor;
		if (child->xpos || !child->bol_suppress) {
			line->cursor += child->xlen;
		}
		line->height = max (child->ylen, line->height);
		line->child_count++;
	}
	set_rows (view, &flowline);
}

static void
flow_left (view_t *view, void (*set_rows) (view_t *, flowline_t *))
{
	flowline_t  flowline = {};
	flowline_t *line = &flowline;
	line->cursor = view->xlen;
	for (int i = 0; i < view->num_children; i++) {
		view_t     *child = view->children[i];
		if (line->cursor < view->xlen && line->cursor - child->xlen < 0) {
			NEXT_LINE(line, i);
			line->cursor = view->xlen;
		}
		if (child->xpos < view->xlen || !child->bol_suppress) {
			line->cursor -= child->xlen;
		}
		child->xpos = line->cursor;
		line->height = max (child->ylen, line->height);
		line->child_count++;
	}
	set_rows (view, &flowline);
}

static void
flow_down (view_t *view, void (*set_rows) (view_t *, flowline_t *))
{
	flowline_t  flowline = {};
	flowline_t *line = &flowline;
	for (int i = 0; i < view->num_children; i++) {
		view_t     *child = view->children[i];
		if (line->cursor && line->cursor + child->ylen > view->ylen) {
			NEXT_LINE(line, i);
		}
		child->ypos = line->cursor;
		if (child->ypos || !child->bol_suppress) {
			line->cursor += child->ylen;
		}
		line->height = max (child->xlen, line->height);
		line->child_count++;
	}
	set_rows (view, &flowline);
}

static void
flow_up (view_t *view, void (*set_rows) (view_t *, flowline_t *))
{
	flowline_t  flowline = {};
	flowline_t *line = &flowline;
	line->cursor = view->ylen;
	for (int i = 0; i < view->num_children; i++) {
		view_t     *child = view->children[i];
		if (line->cursor < view->ylen && line->cursor - child->ylen < 0) {
			NEXT_LINE(line, i);
			line->cursor = view->ylen;
		}
		if (child->ypos < view->ylen || !child->bol_suppress) {
			line->cursor -= child->ylen;
		}
		child->ypos = line->cursor;
		line->height = max (child->xlen, line->height);
		line->child_count++;
	}
	set_rows (view, &flowline);
}

static void
flow_view_height (view_t *view, flowline_t *flowlines)
{
	if (view->flow_size) {
		view->ylen = 0;
		for (flowline_t *line = flowlines; line; line = line->next) {
			view->ylen += line->height;
		}
	}
}

static void
flow_view_width (view_t *view, flowline_t *flowlines)
{
	if (view->flow_size) {
		view->xlen = 0;
		for (flowline_t *line = flowlines; line; line = line->next) {
			view->xlen += line->height;
		}
	}
}

static void
set_rows_down (view_t *view, flowline_t *flowlines)
{
	flow_view_height (view, flowlines);

	int         cursor = 0;
	for (flowline_t *line = flowlines; line; line = line->next) {
		cursor += line->height;
		for (int i = 0; i < line->child_count; i++) {
			view_t     *child = view->children[line->first_child + i];

			child->xrel = child->xpos;
			child->yrel = cursor + child->ypos - child->ylen;
		}
	}
}

static void
set_rows_up (view_t *view, flowline_t *flowlines)
{
	flow_view_height (view, flowlines);

	int         cursor = view->ylen;
	for (flowline_t *line = flowlines; line; line = line->next) {
		for (int i = 0; i < line->child_count; i++) {
			view_t     *child = view->children[line->first_child + i];

			child->xrel = child->xpos;
			child->yrel = cursor + child->ypos - child->ylen;
		}
		cursor -= line->height;
	}
}

static void
set_columns_right (view_t *view, flowline_t *flowlines)
{
	flow_view_width (view, flowlines);

	int         cursor = 0;
	for (flowline_t *line = flowlines; line; line = line->next) {
		for (int i = 0; i < line->child_count; i++) {
			view_t     *child = view->children[line->first_child + i];

			child->xrel = cursor + child->xpos;
			child->yrel = child->ypos;
		}
		cursor += line->height;
	}
}

static void
set_columns_left (view_t *view, flowline_t *flowlines)
{
	flow_view_width (view, flowlines);

	int         cursor = view->xlen;
	for (flowline_t *line = flowlines; line; line = line->next) {
		cursor -= line->height;
		for (int i = 0; i < line->child_count; i++) {
			view_t     *child = view->children[line->first_child + i];

			child->xrel = cursor + child->xpos;
			child->yrel = child->ypos;
		}
	}
}

VISIBLE void
view_flow_right_down (view_t *view)
{
	flow_right (view, set_rows_down);
}

VISIBLE void
view_flow_right_up (view_t *view)
{
	flow_right (view, set_rows_up);
}

VISIBLE void
view_flow_left_down (view_t *view)
{
	flow_left (view, set_rows_down);
}

VISIBLE void
view_flow_left_up (view_t *view)
{
	flow_left (view, set_rows_up);
}

VISIBLE void
view_flow_down_right (view_t *view)
{
	flow_down (view, set_columns_right);
}

VISIBLE void
view_flow_up_right (view_t *view)
{
	flow_up (view, set_columns_right);
}

VISIBLE void
view_flow_down_left (view_t *view)
{
	flow_down (view, set_columns_left);
}

VISIBLE void
view_flow_up_left (view_t *view)
{
	flow_up (view, set_columns_left);
}
#endif
