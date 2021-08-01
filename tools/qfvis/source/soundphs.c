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

#include "tools/qfvis/include/options.h"
#include "tools/qfvis/include/vis.h"

/*
	Some textures (sky, water, slime, lava) are considered ambient sound
	emiters.

	Find an aproximate distance to the nearest emiter of each class for each
	leaf.
*/

static byte *surf_ambients;

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

static void
init_surf_ambients (void)
{
	surf_ambients = malloc (bsp->numfaces);
	if (!bsp->texdata) {
		memset (surf_ambients, -1, bsp->numfaces);
		return;
	}

	dmiptexlump_t *miptex = (dmiptexlump_t *) bsp->texdata;

	for (int i = 0; i < bsp->numfaces; i++) {
		dface_t    *surf = &bsp->faces[i];
		texinfo_t  *info = &bsp->texinfo[surf->texinfo];
		int         ofs = miptex->dataofs[info->miptex];
		miptex_t   *miptex = (miptex_t *) &bsp->texdata[ofs];

		if (!strncasecmp (miptex->name, "*water", 6))
			surf_ambients[i] = AMBIENT_WATER;
		else if (!strncasecmp (miptex->name, "sky", 3))
			surf_ambients[i] = AMBIENT_SKY;
		else if (!strncasecmp (miptex->name, "*slime", 6))
			surf_ambients[i] = AMBIENT_WATER;
		else if (!strncasecmp (miptex->name, "*lava", 6))
			surf_ambients[i] = AMBIENT_LAVA;
		else if (!strncasecmp (miptex->name, "*04water", 8))
			surf_ambients[i] = AMBIENT_WATER;
		else
			surf_ambients[i] = -1;
	}
}

static void
LeafAmbients (dleaf_t *leaf, set_t *vis)
{
	float		 dists[NUM_AMBIENTS];

	// clear ambients
	for (int j = 0; j < NUM_AMBIENTS; j++) {
		dists[j] = 1020;
	}

	for (unsigned j = 0; j < numrealleafs - 1; j++) {
		if (!set_is_member (vis, j)) {
			continue;
		}

		if (!bsp->texdata)
			continue;

		// check this leaf for sound textures
		dleaf_t    *hit = &bsp->leafs[j + 1];

		for (uint32_t k = 0; k < hit->nummarksurfaces; k++) {
			unsigned    surf_index = bsp->marksurfaces[hit->firstmarksurface + k];
			dface_t    *surf = &bsp->faces[surf_index];

			byte        ambient_type;
			if ((ambient_type = surf_ambients[surf_index]) == 0xff) {
				continue;
			}

			// find distance from source leaf to polygon
			vec3_t      mins, maxs;
			float       maxd = 0;
			SurfaceBBox (surf, mins, maxs);
			for (int l = 0; l < 3; l++) {
				float       d = 0;
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

	for (int j = 0; j < NUM_AMBIENTS; j++) {
		float vol = 0;

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

// vis is 1-based, but bsp leafs are 0-based
static unsigned work_leaf = 1;

static int
ambient_progress (void)
{
	return (work_leaf - 1) * 100 / (numrealleafs - 1);
}

static unsigned
next_leaf (void)
{
	unsigned    leaf = ~0;
	WRLOCK (global_lock);
	if (work_leaf < numrealleafs) {
		leaf = work_leaf++;
	}
	UNLOCK (global_lock);
	return leaf;
}

static void *
AmbientThread (void *d)
{
	int         thread = (intptr_t) d;
	set_t       vis = {};

	vis.size = SET_SIZE (numrealleafs - 1);

	while (1) {
		unsigned    leaf_num = next_leaf ();

		if (working)
			working[thread] = leaf_num;
		if (leaf_num == ~0u) {
			break;
		}

		unsigned    cluster_num = leafcluster[leaf_num];
		vis.map = (set_bits_t *) &uncompressed[cluster_num * bitbytes_l];
		LeafAmbients (&bsp->leafs[leaf_num], &vis);
	}
	return 0;
}

void
CalcAmbientSounds (void)
{
	init_surf_ambients ();
	if (options.verbosity >= 0) {
		printf ("Ambients: ");
	}
	RunThreads (AmbientThread, ambient_progress);
}
