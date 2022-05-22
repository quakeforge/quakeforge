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

static mleaf_t *r_oldviewleaf;
static set_t *solid;

void
R_MarkLeaves (mleaf_t *viewleaf, int *node_visframes)
{
	set_t       *vis;
	int			 c;
	mleaf_t     *leaf;
	msurface_t **mark;
	mod_brush_t *brush = &r_refdef.worldmodel->brush;

	if (r_oldviewleaf == viewleaf && !r_novis)
		return;

	r_visframecount++;
	r_oldviewleaf = viewleaf;
	if (!viewleaf)
		return;

	if (r_novis) {
		r_oldviewleaf = 0;	// so vis will be recalcualted when novis gets
							// turned off
		if (!solid) {
			solid = set_new ();
			set_everything (solid);
		}
		vis = solid;
	} else
		vis = Mod_LeafPVS (viewleaf, r_refdef.worldmodel);

	for (unsigned i = 0; i < brush->visleafs; i++) {
		if (set_is_member (vis, i)) {
			leaf = &brush->leafs[i + 1];
			if ((c = leaf->nummarksurfaces)) {
				mark = brush->marksurfaces + leaf->firstmarksurface;
				do {
					(*mark)->visframe = r_visframecount;
					mark++;
				} while (--c);
			}
			leaf->visframe = r_visframecount;
			int         node_id = brush->leaf_parents[leaf - brush->leafs];
			while (node_id >= 0) {
				if (node_visframes[node_id] == r_visframecount)
					break;
				node_visframes[node_id] = r_visframecount;
				node_id = brush->node_parents[node_id];
			}
		}
	}
}

/*
  R_TextureAnimation

  Returns the proper texture for a given time and base texture
*/
texture_t  *
R_TextureAnimation (const entity_t *entity, msurface_t *surf)
{
	texture_t  *base = surf->texinfo->texture;
	int         count, relative;

	if (entity->animation.frame) {
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
