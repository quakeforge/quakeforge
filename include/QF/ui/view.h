/*
	view.h

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

#ifndef __QF_ui_view_h
#define __QF_ui_view_h

#include "QF/ecs.h"

/** \defgroup console_view Console View Objects
	\ingroup console
*/
///@{

/** Control the positioning of a view within its parent. The directions are
	the standard compass rose (north, east, south, west in clockwise order)
	with north at the top.

	The origin of the view is taken to be the corresponding point on the edge
	of the view (eg, southeast is bottom right), or the view's center for
	center gravity. When the relative coordinates of the view are (0,0), the
	view's origin is placed on the parent view's gravity point using the view's
	gravity (\em not the parent view's gravity). That is, the parent view's
	gravity has no effect on the view's position within the parent view.

	The gravity also affects the direction the view moves as the relative
	coordinates of the view change.

	No checking is done to ensure the view stays within the parent, or that the
	view is smaller than the parent. This is by design. It is up to the drawing
	callbacks to do any necessary clipping.
*/
typedef enum {
	grav_center,	///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_north,		///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_northeast,	///< +ve X left,  +ve Y down, -X right, -ve Y up
	grav_east,		///< +ve X left,  +ve Y down, -X right, -ve Y up
	grav_southeast,	///< +ve X left,  +ve Y up,   -X right, -ve Y down
	grav_south,		///< +ve X right, +ve Y up,   -X left,  -ve Y down
	grav_southwest,	///< +ve X right, +ve Y up,   -X left,  -ve Y down
	grav_west,		///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_northwest,	///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_flow,		///< controlled by view_flow
} grav_t;

extern struct exprtype_s grav_t_type;

typedef struct view_pos_s {
	int         x;
	int         y;
} view_pos_t;

typedef struct viewcont_s {
	grav_t  gravity;			///< The gravity of the view.
	unsigned visible:1;			///< If false, view_draw() skips this view.
	unsigned resize_x:1;		///< If true, view's width follows parent's.
	unsigned resize_y:1;		///< If true, view's height follows parent's.
	unsigned bol_suppress:1;	///< If true, view_flow skips at start of line.
	unsigned flow_size:1;		///< If true, view's size is adjusted to flow.
	unsigned semantic_x:3;		///< layout size control (imui_size_t)
	unsigned semantic_y:3;		///< layout size control (imui_size_t)
	unsigned free_x:1;			///< don't set position automatically
	unsigned free_y:1;			///< don't set position automatically
	unsigned vertical:1;		///< true: layout is vertical, else horizontal
	unsigned active:1;			///< can respond to the mouse
	unsigned is_link:1;			///< has non-root reference component
} viewcont_t;

enum {
	view_href,

	view_comp_count
};

extern const struct component_s view_components[view_comp_count];

// components in the view hierarchy
enum {
	/// Coordinates of view's origin relative to parent's gravity point.
	view_pos,
	/// Size of the view.
	view_len,
	/** Absolute coordinates of the top left (northwest) corner of the view.
		Set interally.
	*/
	view_abs,
	/** Coordinates of the top left (northwest) corner of the view relative to
		the parent view's top left corner. Set internally.
	*/
	view_rel,
	view_oldlen,
	view_control,
	view_modified,
	view_onresize,
	view_onmove,

	view_type_count
};

/** The view object.
*/
typedef struct view_s {
	ecs_registry_t *reg;
	uint32_t    id;
	uint32_t    comp;
} view_t;

#define nullview ((view_t) {})

typedef void (*view_resize_f) (view_t view, view_pos_t len);
typedef void (*view_move_f) (view_t view, view_pos_t abs);

#define VIEWINLINE GNU89INLINE inline

VIEWINLINE view_t View_FromEntity (ecs_system_t viewsys, uint32_t ent);
view_t View_New (ecs_system_t viewsys, view_t parent);
view_t View_AddToEntity (uint32_t ent, ecs_system_t viewsys, view_t parent);
VIEWINLINE void View_Delete (view_t view);
void View_SetParent (view_t view, view_t parent);
void View_UpdateHierarchy (view_t view);

void view_flow_right_down (view_t view, view_pos_t len);
void view_flow_right_up (view_t view, view_pos_t len);
void view_flow_left_down (view_t view, view_pos_t len);
void view_flow_left_up (view_t view, view_pos_t len);
void view_flow_down_right (view_t view, view_pos_t len);
void view_flow_up_right (view_t view, view_pos_t len);
void view_flow_down_left (view_t view, view_pos_t len);
void view_flow_up_left (view_t view, view_pos_t len);

VIEWINLINE hierref_t View_GetRef (view_t view);
VIEWINLINE int View_Valid (view_t view);

VIEWINLINE view_t View_GetRoot (view_t view);
VIEWINLINE view_t View_GetParent (view_t view);
VIEWINLINE uint32_t View_ChildCount (view_t view);
VIEWINLINE view_t View_GetChild (view_t view, uint32_t index);

VIEWINLINE void View_SetPos (view_t view, int x, int y);
VIEWINLINE view_pos_t View_GetPos (view_t view);
VIEWINLINE view_pos_t View_GetAbs (view_t view);
VIEWINLINE view_pos_t View_GetRel (view_t view);
VIEWINLINE void View_SetLen (view_t view, int x, int y);
VIEWINLINE view_pos_t View_GetLen (view_t view);
VIEWINLINE viewcont_t *View_Control (view_t view);
VIEWINLINE void View_SetGravity (view_t view, grav_t grav);
VIEWINLINE grav_t View_GetGravity (view_t view);
VIEWINLINE void View_SetVisible (view_t view, int visible);
VIEWINLINE int View_GetVisible (view_t view);
VIEWINLINE void View_SetResize (view_t view, int resize_x, int resize_y);
VIEWINLINE void View_SetOnResize (view_t view, view_resize_f onresize);
VIEWINLINE void View_SetOnMove (view_t view, view_move_f onmove);

#undef VIEWINLINE
#ifndef IMPLEMENT_VIEW_Funcs
#define VIEWINLINE GNU89INLINE inline
#else
#define VIEWINLINE VISIBLE
#endif

VIEWINLINE
view_t
View_FromEntity (ecs_system_t viewsys, uint32_t ent)
{
	return (view_t) {
		.id = ent,
		.reg = viewsys.reg,
		.comp = viewsys.base + view_href,
	};
}

VIEWINLINE
hierref_t
View_GetRef (view_t view)
{
	return *(hierref_t *) Ent_GetComponent (view.id, view.comp, view.reg);
}

VIEWINLINE
int
View_Valid (view_t view)
{
	return view.reg && ECS_EntValid (view.id, view.reg);
}

VIEWINLINE
void
View_Delete (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	Hierarchy_RemoveHierarchy (h, ref.index, 1);
	if (!h->num_objects) {
		Hierarchy_Delete (ref.id, view.reg);
	}
}

VIEWINLINE
view_t
View_GetRoot (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	return (view_t) {
		.reg = view.reg,
		.id = h->ent[0],
		.comp = view.comp,
	};
}

VIEWINLINE
view_t
View_GetParent (view_t view)
{
	auto ref = View_GetRef (view);
	if (ref.index == 0) {
		return nullview;
	}
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	return (view_t) {
		.reg = view.reg,
		.id = h->ent[h->parentIndex[ref.index]],
		.comp = view.comp,
	};
}

VIEWINLINE
uint32_t
View_ChildCount (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	return h->childCount[ref.index];
}

VIEWINLINE
view_t
View_GetChild (view_t view, uint32_t childIndex)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	if (childIndex >= h->childCount[ref.index]) {
		return nullview;
	}
	return (view_t) {
		.reg = view.reg,
		.id = h->ent[h->childIndex[ref.index] + childIndex],
		.comp = view.comp,
	};
}


VIEWINLINE
void
View_SetPos (view_t view, int x, int y)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *pos = h->components[view_pos];
	byte       *modified = h->components[view_modified];
	pos[ref.index] = (view_pos_t) { x, y };
	modified[ref.index] |= 1;
}

VIEWINLINE
view_pos_t
View_GetPos (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *pos = h->components[view_pos];
	return pos[ref.index];
}

VIEWINLINE
view_pos_t
View_GetAbs (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *abs = h->components[view_abs];
	return abs[ref.index];
}

VIEWINLINE
view_pos_t
View_GetRel (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *rel = h->components[view_rel];
	return rel[ref.index];
}

VIEWINLINE
void
View_SetLen (view_t view, int x, int y)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *len = h->components[view_len];
	view_pos_t *oldlen = h->components[view_oldlen];
	byte       *modified = h->components[view_modified];
	if (!(modified[ref.index] & 2)) {
		oldlen[ref.index] = len[ref.index];
	}
	len[ref.index] = (view_pos_t) { x, y };
	modified[ref.index] |= 2;
}

VIEWINLINE
view_pos_t
View_GetLen (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_pos_t *len = h->components[view_len];
	return len[ref.index];
}

VIEWINLINE
viewcont_t *
View_Control (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	viewcont_t *cont = h->components[view_control];
	return &cont[ref.index];
}

VIEWINLINE
void
View_SetGravity (view_t view, grav_t grav)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	viewcont_t *cont = h->components[view_control];
	byte       *modified = h->components[view_modified];
	cont[ref.index].gravity = grav;
	modified[ref.index] |= 1;
}

VIEWINLINE
grav_t
View_GetGravity (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	viewcont_t *cont = h->components[view_control];
	return cont[ref.index].gravity;
}

VIEWINLINE
void
View_SetVisible (view_t view, int visible)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	viewcont_t *cont = h->components[view_control];
	cont[ref.index].visible = !!visible;
}

VIEWINLINE
int
View_GetVisible (view_t view)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	viewcont_t *cont = h->components[view_control];
	return cont[ref.index].visible;
}

VIEWINLINE
void
View_SetResize (view_t view, int resize_x, int resize_y)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	viewcont_t *cont = h->components[view_control];
	cont[ref.index].resize_x = resize_x;
	cont[ref.index].resize_y = resize_y;
}

VIEWINLINE
void
View_SetOnResize (view_t view, view_resize_f onresize)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_resize_f *resize = h->components[view_onresize];
	resize[ref.index] = onresize;
}

VIEWINLINE
void
View_SetOnMove (view_t view, view_move_f onmove)
{
	auto ref = View_GetRef (view);
	hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, view.reg);
	view_move_f *move = h->components[view_onmove];
	move[ref.index] = onmove;
}

///@}

#endif//__QF_ui_view_h
