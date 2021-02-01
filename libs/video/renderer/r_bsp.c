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
#include "QF/sys.h"

#include "r_internal.h"

mvertex_t  *r_pcurrentvertbase;
mleaf_t    *r_viewleaf;
static mleaf_t *r_oldviewleaf;

void
R_MarkLeaves (void)
{
	byte         solid[8192];
	byte        *vis;
	int			 c;
	unsigned int i;
	mleaf_t     *leaf;
	mnode_t     *node;
	msurface_t **mark;

	if (r_oldviewleaf == r_viewleaf && !r_novis->int_val)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;
	if (!r_viewleaf)
		return;

	if (r_novis->int_val) {
		r_oldviewleaf = 0;	// so vis will be recalcualted when novis gets
							// turned off
		vis = solid;
		memset (solid, 0xff, (r_worldentity.model->brush.numleafs + 7) >> 3);
	} else
		vis = Mod_LeafPVS (r_viewleaf, r_worldentity.model);

	for (i = 0; (int) i < r_worldentity.model->brush.numleafs; i++) {
		if (vis[i >> 3] & (1 << (i & 7))) {
			leaf = &r_worldentity.model->brush.leafs[i + 1];
			if ((c = leaf->nummarksurfaces)) {
				mark = leaf->firstmarksurface;
				do {
					(*mark)->visframe = r_visframecount;
					mark++;
				} while (--c);
			}
			node = (mnode_t *) leaf;
			do {
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

/*
  R_TextureAnimation

  Returns the proper texture for a given time and base texture
*/
texture_t  *
R_TextureAnimation (msurface_t *surf)
{
	texture_t  *base = surf->texinfo->texture;
	int         count, relative;

	if (currententity->frame) {
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
