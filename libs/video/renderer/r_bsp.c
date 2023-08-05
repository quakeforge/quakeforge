/*
	r_bsp.c

	Common bsp rendering.

	Copyright (C) 1996-1997  Id Software, Inc.

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cvar.h"
#include "QF/set.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "r_internal.h"

void
R_MarkLeavesPVS (visstate_t *visstate, const set_t *pvs)
{
	int visframecount = ++visstate->visframecount;
	auto brush = visstate->brush;
	auto node_visframes = visstate->node_visframes;
	auto leaf_visframes = visstate->leaf_visframes;
	auto face_visframes = visstate->face_visframes;
	for (auto li = set_first (pvs); li; li = set_next (li)) {
		unsigned i = li->element;
		if (set_is_member (pvs, i)) {
			auto leaf = &brush->leafs[i + 1];
			int c;
			if ((c = leaf->nummarksurfaces)) {
				auto mark = brush->marksurfaces + leaf->firstmarksurface;
				do {
					face_visframes[*mark - brush->surfaces] = visframecount;
					mark++;
				} while (--c);
			}
			leaf_visframes[i + 1] = visframecount;
			int         node_id = brush->leaf_parents[leaf - brush->leafs];
			while (node_id >= 0) {
				if (node_visframes[node_id] == visframecount)
					break;
				node_visframes[node_id] = visframecount;
				node_id = brush->node_parents[node_id];
			}
		}
	}
}

void
R_MarkLeaves (visstate_t *visstate, const mleaf_t *viewleaf)
{
	auto brush = visstate->brush;
	set_t        vis = SET_STATIC_INIT (brush->visleafs, alloca);

	if (visstate->viewleaf == viewleaf && !r_novis)
		return;

	visstate->viewleaf = viewleaf;
	if (!viewleaf)
		return;

	if (r_novis) {
		// so vis will be recalculated when novis gets turned off
		visstate->viewleaf = 0;
		// force use of default vis (full visibility)
		viewleaf = brush->leafs;
	}
	Mod_LeafPVS_set (viewleaf, brush, 0xff, &vis);
	R_MarkLeavesPVS (visstate, &vis);
}

/*
  R_TextureAnimation

  Returns the proper texture for a given time and base texture
*/
texture_t  *
R_TextureAnimation (int frame, msurface_t *surf)
{
	texture_t  *base = surf->texinfo->texture;
	int         count, relative;

	if (frame) {
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	relative = (int) (r_data->realtime * 10) % base->anim_total;

	count = 0;
	while (base->anim_min > relative || base->anim_max <= relative) {
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}
