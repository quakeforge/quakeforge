/*
	soundphs.c

	PHS (possible hearable set) for ambient brushes

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2002 Colin Thompson

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

#include <getopt.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "QF/bspfile.h"
#include "QF/cmd.h"
#include "QF/mathlib.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "tools/qfvis/include/vis.h"

/*
	Some textures (sky, water, slime, lava) are considered ambient sound
	emiters.

	Find an aproximate distance to the nearest emiter of each class for each
	leaf.
*/

static void
SurfaceBBox (dface_t *s, vec3_t mins, vec3_t maxs)
{
	int		vi, e, i, j;
	float  *v;

	mins[0] = mins[1] = mins[2] = 999999;
	maxs[0] = maxs[1] = maxs[2] = -99999;

	for (i = 0; i < s->numedges; i++) {
		e = bsp->surfedges[s->firstedge + i];
		if (e >= 0)
			vi = bsp->edges[e].v[0];
		else
			vi = bsp->edges[-e].v[1];
		v = bsp->vertexes[vi].point;

		for (j = 0; j < 3; j++) {
			if (v[j] < mins[j])
				mins[j] = v[j];
			if (v[j] > maxs[j])
				maxs[j] = v[j];
		}
	}
}

void
CalcAmbientSounds (void)
{
	byte		*vis;
	unsigned     i, j, k;
	int			 ambient_type, ofs, l;
	float		 maxd, vol, d;
	float		 dists[NUM_AMBIENTS];
	dface_t		*surf;
	dleaf_t		*leaf, *hit;
	vec3_t		 mins, maxs;
	texinfo_t	*info;
	miptex_t	*miptex;

	for (i = 0; i < numrealleafs - 1; i++) {
		leaf = &bsp->leafs[i + 1];

		// clear ambients
		for (j = 0; j < NUM_AMBIENTS; j++)
			dists[j] = 1020;

		vis = &uncompressed[leafcluster[i] * bitbytes_l];

		for (j = 0; j < numrealleafs - 1; j++) {
			if (!(vis[j >> 3] & (1 << (j & 7))))
				continue;

			if (!bsp->texdata)
				continue;

			// check this leaf for sound textures
			hit = &bsp->leafs[j + 1];

			for (k = 0; k < hit->nummarksurfaces; k++) {
				surf = &bsp->faces[bsp->marksurfaces[hit->firstmarksurface + k]];
				info = &bsp->texinfo[surf->texinfo];
				ofs = ((dmiptexlump_t *) bsp->texdata)->dataofs[info->miptex];
				miptex = (miptex_t *) (&bsp->texdata[ofs]);

				if (!strncasecmp (miptex->name, "*water", 6))
					ambient_type = AMBIENT_WATER;
				else if (!strncasecmp (miptex->name, "sky", 3))
					ambient_type = AMBIENT_SKY;
				else if (!strncasecmp (miptex->name, "*slime", 6))
					ambient_type = AMBIENT_WATER;
				else if (!strncasecmp (miptex->name, "*lava", 6))
					ambient_type = AMBIENT_LAVA;
				else if (!strncasecmp (miptex->name, "*04water", 8))
					ambient_type = AMBIENT_WATER;
				else
					continue;

				// find distance from source leaf to polygon
				SurfaceBBox (surf, mins, maxs);
				maxd = 0;
				for (l = 0; l < 3; l++) {
					if (mins[l] > leaf->maxs[l])
						d = mins[l] - leaf->maxs[l];
					else if (maxs[l] < leaf->mins[l])
						d = leaf->mins[l] - mins[l];
					else
						d = 0;
					if (d > maxd)
						maxd = d;
				}

				maxd = 0.25;
				if (maxd < dists[ambient_type])
					dists[ambient_type] = maxd;
			}
		}

		for (j = 0; j < NUM_AMBIENTS; j++) {
			if (dists[j] < 100)
				vol = 1.0;
			else {
				vol = 1.0 - dists[2] * 0.002;
				if (vol < 0)
					vol = 0;
			}
			leaf->ambient_level[j] = vol * 255;
		}
	}
}
