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

#include "r_internal.h"

mvertex_t  *r_pcurrentvertbase;
mleaf_t    *r_viewleaf;
static mleaf_t *r_oldviewleaf;
static set_t *solid;

void
R_MarkLeaves (void)
{
	set_t       *vis;
	int			 c;
	mleaf_t     *leaf;
	mnode_t     *node;
	msurface_t **mark;
	mod_brush_t *brush = &r_worldentity.renderer.model->brush;

	if (r_oldviewleaf == r_viewleaf && !r_novis->int_val)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;
	if (!r_viewleaf)
		return;

	if (r_novis->int_val) {
		r_oldviewleaf = 0;	// so vis will be recalcualted when novis gets
							// turned off
		if (!solid) {
			solid = set_new ();
			set_everything (solid);
		}
		vis = solid;
	} else
		vis = Mod_LeafPVS (r_viewleaf, r_worldentity.renderer.model);

	for (unsigned i = 0; i < brush->numleafs; i++) {
		if (set_is_member (vis, i)) {
			leaf = &brush->leafs[i + 1];
			if ((c = leaf->nummarksurfaces)) {
				mark = leaf->firstmarksurface;
				do {
					(*mark)->visframe = r_visframecount;
					mark++;
				} while (--c);
			}
			leaf->visframe = r_visframecount;
			node = brush->leaf_parents[leaf - brush->leafs];
			while (node) {
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = brush->node_parents[node - brush->nodes];
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

	relative = (int) (vr_data.realtime * 10) % base->anim_total;

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
