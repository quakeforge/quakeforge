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

const component_t view_components[view_comp_count] = {
	[view_href] = {
		.size = sizeof (hierref_t),
		.name = "view href",
		.destroy = Hierref_DestroyComponent,
	},
};

static const component_t view_type_components[view_type_count] = {
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
	.components = view_type_components,
};

view_t
View_AddToEntity (uint32_t ent, ecs_system_t viewsys, view_t parent, bool own)
{
	uint32_t    href_comp = viewsys.base + view_href;
	hierref_t   ref = {};

	if (parent.reg && parent.id != nullent) {
		hierref_t   pref = View_GetRef (parent);
		ref = Hierarchy_InsertHierarchy (pref, nullhref, viewsys.reg);
	} else {
		ref.id = Hierarchy_New (viewsys.reg, href_comp, &view_type, 1);
	}
	Ent_SetComponent (ent, href_comp, viewsys.reg, &ref);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, viewsys.reg);
	h->ent[ref.index] = ent;
	h->own[ref.index] = own;
	return (view_t) { .reg = viewsys.reg, .id = ent, .comp = href_comp };
}

view_t
View_New (ecs_system_t viewsys, view_t parent)
{
	uint32_t    view = ECS_NewEntity (viewsys.reg);
	return View_AddToEntity (view, viewsys, parent, true);
}

void
View_UpdateHierarchy (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);

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
		modified[0] |= 1;
		abs[0] = pos[0];
		rel[0] = pos[0];
	}
	if (oldlen[0].x != len[0].x || oldlen[0].y != len[0].y) {
		modified[0] |= 2;
		if (onresize[0]) {
			view_t      v = { .reg = view.reg, .id = id[0], .comp = view.comp };
			onresize[0] (v, len[0]);
		}
	}
	for (uint32_t i = 1; i < h->num_objects; i++) {
		uint32_t    par = parent[i];
		if (!(modified[i] & 2) && (modified[par] & 2)
			&& (cont[i].resize_x || cont[i].resize_y)) {
			int         dx = len[par].x - oldlen[par].x;
			int         dy = len[par].y - oldlen[par].y;
			modified[i] |= 2;	// propogate resize modifications
			oldlen[i] = len[i];	// for child resize calculations
			if (cont[i].resize_x) {
				len[i].x += dx;
			}
			if (cont[i].resize_y) {
				len[i].y += dy;
			}
			if (onresize[i]) {
				view_t      v = { .reg = view.reg, .id = id[i],
								  .comp = view.comp };
				onresize[i] (v, len[i]);
			}
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
					//rel is set by the flow functions
					break;
			}
			abs[i].x = abs[par].x + rel[i].x;
			abs[i].y = abs[par].y + rel[i].y;
		}
	}
	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (modified[i] & 2) {
			oldlen[i] = len[i];
		}
		if ((modified[i] & 1) && onmove[i]) {
			view_t      v = { .reg = view.reg, .id = id[i], .comp = view.comp };
			onmove[i] (v, abs[i]);
		}
		modified[i] = 0;
	}
}

void
View_SetParent (view_t view, view_t parent)
{
	hierref_t   dref = nullhref;
	hierref_t   sref = View_GetRef (view);
	if (View_Valid (parent)) {
		dref = View_GetRef (parent);
	}
	Hierarchy_SetParent (dref, sref, view.reg);

	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	byte       *modified = h->components[view_modified];
	modified[ref.index] = 1;
	View_UpdateHierarchy (view);
}

typedef struct flowline_s {
	struct flowline_s *next;
	int         first_child;
	int         child_count;
	int         cursor;
	int         height;	// from baseline
	int         depth;	// from baseline
} flowline_t;

#define NEXT_LINE(line, child_index) \
	do { \
		line->next = alloca (sizeof (flowline_t)); \
		memset (line->next, 0, sizeof (flowline_t)); \
		line = line->next; \
		line->first_child = child_index; \
	} while (0)

static void
flow_right (view_t view, void (*set_rows) (view_t, flowline_t *))
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *pos = h->components[view_pos];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t    vind = ref.index;

	flowline_t  flowline = { .first_child = h->childIndex[ref.index] };
	flowline_t *line = &flowline;
	for (uint32_t i = 0; i < h->childCount[vind]; i++) {
		uint32_t    child = h->childIndex[vind] + i;
		if (line->cursor && line->cursor + len[child].x > len[vind].x) {
			NEXT_LINE(line, child);
		}
		pos[child].x = line->cursor;
		if (pos[child].x || !cont[child].bol_suppress) {
			line->cursor += len[child].x;
		}
		line->height = max (len[child].y - pos[child].y, line->height);
		line->depth = max (pos[child].y, line->depth);
		line->child_count++;
	}
	set_rows (view, &flowline);
}

static void
flow_left (view_t view, void (*set_rows) (view_t, flowline_t *))
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *pos = h->components[view_pos];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t    vind = ref.index;

	flowline_t  flowline = { .first_child = h->childIndex[ref.index] };
	flowline_t *line = &flowline;
	line->cursor = len[vind].x;
	for (uint32_t i = 0; i < h->childCount[vind]; i++) {
		uint32_t    child = h->childIndex[ref.index] + i;
		if (line->cursor < len[vind].x && line->cursor - len[child].x < 0) {
			NEXT_LINE(line, child);
			line->cursor = len[vind].x;
		}
		if (pos[child].x < len[vind].x || !cont[child].bol_suppress) {
			line->cursor -= len[child].x;
		}
		pos[child].x = line->cursor;
		line->height = max (len[child].y - pos[child].y, line->height);
		line->depth = max (pos[child].y, line->depth);
		line->child_count++;
	}
	set_rows (view, &flowline);
}

static void
flow_down (view_t view, void (*set_rows) (view_t, flowline_t *))
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *pos = h->components[view_pos];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t    vind = ref.index;

	flowline_t  flowline = { .first_child = h->childIndex[ref.index] };
	flowline_t *line = &flowline;
	for (uint32_t i = 0; i < h->childCount[vind]; i++) {
		uint32_t    child = h->childIndex[vind] + i;
		if (line->cursor && line->cursor + len[child].y > len[vind].y) {
			NEXT_LINE(line, child);
		}
		pos[child].y = line->cursor;
		if (pos[child].y || !cont[child].bol_suppress) {
			line->cursor += len[child].y;
		}
		line->height = max (len[child].x - pos[child].x, line->height);
		line->depth = max (pos[child].x, line->depth);
		line->child_count++;
	}
	set_rows (view, &flowline);
}

static void
flow_up (view_t view, void (*set_rows) (view_t, flowline_t *))
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *pos = h->components[view_pos];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t    vind = ref.index;

	flowline_t  flowline = { .first_child = h->childIndex[ref.index] };
	flowline_t *line = &flowline;
	line->cursor = len[vind].y;
	for (uint32_t i = 0; i < h->childCount[vind]; i++) {
		uint32_t    child = h->childIndex[ref.index] + i;
		if (line->cursor < len[vind].y && line->cursor - len[child].y < 0) {
			NEXT_LINE(line, child);
			line->cursor = len[vind].y;
		}
		if (pos[child].y < len[vind].y || !cont[child].bol_suppress) {
			line->cursor -= len[child].y;
		}
		pos[child].y = line->cursor;
		line->height = max (len[child].x - pos[child].x, line->height);
		line->depth = max (pos[child].x, line->depth);
		line->child_count++;
	}
	set_rows (view, &flowline);
}

static void
flow_view_vertical (view_pos_t *pos, view_pos_t *len,
					uint32_t vind, uint32_t pind)
{
	pos[vind].y = len[pind].y;
	len[pind].y += len[vind].y;
}

static void
flow_view_horizontal (view_pos_t *pos, view_pos_t *len,
					  uint32_t vind, uint32_t pind)
{
	pos[vind].x = len[pind].x;
	len[pind].x += len[vind].x;
}

static void
flow_view_height (view_pos_t *len, flowline_t *flowlines)
{
	len->y = 0;
	for (flowline_t *line = flowlines; line; line = line->next) {
		len->y += line->height + line->depth;
	}
}

static void
flow_view_width (view_pos_t *len, flowline_t *flowlines)
{
	len->x = 0;
	for (flowline_t *line = flowlines; line; line = line->next) {
		len->x += line->height + line->depth;
	}
}

static void
set_rows_down (view_t view, flowline_t *flowlines)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *pos = h->components[view_pos];
	view_pos_t *rel = h->components[view_rel];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t    vind = ref.index;

	if (cont[vind].flow_size) {
		flow_view_height (&len[ref.index], flowlines);
		if (cont[vind].flow_parent) {
			flow_view_vertical (pos, len, vind, h->parentIndex[vind]);
		}
	}

	int         cursor = 0;
	for (flowline_t *line = flowlines; line; line = line->next) {
		cursor += line->height;
		for (int i = 0; i < line->child_count; i++) {
			uint32_t    child = line->first_child + i;

			rel[child].x = pos[child].x;
			rel[child].y = cursor + pos[child].y - len[child].y;
		}
		cursor += line->depth;
	}
}

static void
set_rows_up (view_t view, flowline_t *flowlines)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *pos = h->components[view_pos];
	view_pos_t *rel = h->components[view_rel];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t    vind = ref.index;

	if (cont[vind].flow_size) {
		flow_view_height (&len[ref.index], flowlines);
		if (cont[vind].flow_parent) {
			flow_view_vertical (pos, len, vind, h->parentIndex[vind]);
		}
	}

	int         cursor = len[vind].y;
	for (flowline_t *line = flowlines; line; line = line->next) {
		cursor -= line->depth;
		for (int i = 0; i < line->child_count; i++) {
			uint32_t    child = line->first_child + i;

			rel[child].x = pos[child].x;
			rel[child].y = cursor + pos[child].y - len[child].y;
		}
		cursor -= line->height;
	}
}

static void
set_columns_right (view_t view, flowline_t *flowlines)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *pos = h->components[view_pos];
	view_pos_t *rel = h->components[view_rel];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t    vind = ref.index;

	if (cont[vind].flow_size) {
		flow_view_width (&len[ref.index], flowlines);
		if (cont[vind].flow_parent) {
			flow_view_horizontal (pos, len, vind, h->parentIndex[vind]);
		}
	}

	int         cursor = 0;
	for (flowline_t *line = flowlines; line; line = line->next) {
		cursor += line->depth;
		for (int i = 0; i < line->child_count; i++) {
			uint32_t    child = line->first_child + i;

			rel[child].x = cursor + pos[child].x;
			rel[child].y = pos[child].y;
		}
		cursor += line->height;
	}
}

static void
set_columns_left (view_t view, flowline_t *flowlines)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *pos = h->components[view_pos];
	view_pos_t *rel = h->components[view_rel];
	view_pos_t *len = h->components[view_len];
	viewcont_t *cont = h->components[view_control];
	uint32_t    vind = ref.index;

	if (cont[vind].flow_size) {
		flow_view_width (&len[ref.index], flowlines);
		if (cont[vind].flow_parent) {
			flow_view_horizontal (pos, len, vind, h->parentIndex[vind]);
		}
	}

	int         cursor = len[vind].x;
	for (flowline_t *line = flowlines; line; line = line->next) {
		cursor -= line->height;
		for (int i = 0; i < line->child_count; i++) {
			uint32_t    child = line->first_child + i;

			rel[child].x = cursor + pos[child].x;
			rel[child].y = pos[child].y;
		}
		cursor -= line->depth;
	}
}

VISIBLE void
view_flow_right_down (view_t view, view_pos_t len)
{
	flow_right (view, set_rows_down);
}

VISIBLE void
view_flow_right_up (view_t view, view_pos_t len)
{
	flow_right (view, set_rows_up);
}

VISIBLE void
view_flow_left_down (view_t view, view_pos_t len)
{
	flow_left (view, set_rows_down);
}

VISIBLE void
view_flow_left_up (view_t view, view_pos_t len)
{
	flow_left (view, set_rows_up);
}

VISIBLE void
view_flow_down_right (view_t view, view_pos_t len)
{
	flow_down (view, set_columns_right);
}

VISIBLE void
view_flow_up_right (view_t view, view_pos_t len)
{
	flow_up (view, set_columns_right);
}

VISIBLE void
view_flow_down_left (view_t view, view_pos_t len)
{
	flow_down (view, set_columns_left);
}

VISIBLE void
view_flow_up_left (view_t view, view_pos_t len)
{
	flow_up (view, set_columns_left);
}
