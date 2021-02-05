/*
	vis.c

	PVS support to speed lighting calcs

	Copyright (C) 2003 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2003/9/8

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#if defined(_WIN32) && defined(HAVE_MALLOC_H)
# include <malloc.h>
#endif

#include "qfalloca.h"

#include "QF/bspfile.h"
#include "QF/mathlib.h"

#include "tools/qflight/include/entities.h"
#include "tools/qflight/include/light.h"
#include "tools/qflight/include/options.h"
#include "tools/qflight/include/threads.h"

static struct {
	int         empty;
	int         solid;
	int         water;
	int         slime;
	int         lava;
	int         sky;
	int         misc;

	int         lights;
	int         cast;
} counts;

static lightchain_t *free_lightchains;
lightchain_t **surfacelightchain;
vec3_t *surfaceorgs;
entity_t **alllights;
int num_alllights;
entity_t **novislights;
int num_novislights;

static __attribute__((pure)) dleaf_t *
Light_PointInLeaf (vec3_t point)
{
	int         num = 0, side;
	dnode_t    *node;
	dplane_t   *plane;

	while (num >= 0) {
		node = bsp->nodes + num;
		plane = bsp->planes + node->planenum;
		side = DotProduct (point, plane->normal) < plane->dist;
		num = node->children[side];
	}
	return bsp->leafs + (-1 - num);
}

static void
DecompressVis (byte *in, byte *out, int size)
{
	byte *end = out + size;
	int         n;

	while (out < end) {
		n = *in++;
		if (n) {
			*out++ = n;
		} else {
			n = *in++;
			while (n-- && out < end)
				*out++ = 0;
		}
	}
}

static lightchain_t *
get_lightchain (void)
{
	lightchain_t *lc;
	int         i;

	if (!free_lightchains) {
		lc = free_lightchains = malloc (1024 * sizeof (lightchain_t));
		for (i = 0; i < 1023; i++, lc++)
			lc->next = lc + 1;
		lc->next = 0;
	}
	lc = free_lightchains;
	free_lightchains = lc->next;
	return lc;
}

static inline void
mark_face (int surfnum, byte *surfacehit, entity_t *entity)
{
	lightchain_t *lc;

	if (surfacehit[surfnum >> 3] & (1 << (surfnum & 7)))
		return;
	surfacehit[surfnum >> 3] |= (1 << (surfnum & 7));
	counts.cast++;
	lc = get_lightchain ();
	lc->next = surfacelightchain[surfnum];
	lc->light = entity;
	surfacelightchain[surfnum] = lc;
}

void
VisEntity (int ent_index)
{
	entity_t   *entity = entities + ent_index;
	dleaf_t    *leaf;
	int         ignorevis = false;
	int         i;
	unsigned    j;
	uint32_t   *mark;
	byte       *vis, *surfacehit;
	int         vis_size;

	if (!entity->light)
		return;

	counts.lights++;

	leaf = Light_PointInLeaf (entity->origin);

	switch (leaf->contents) {
		case CONTENTS_EMPTY:
			counts.empty++;
			break;
		case CONTENTS_SOLID:
			counts.solid++;
			ignorevis = true;
			break;
		case CONTENTS_WATER:
			counts.water++;
			break;
		case CONTENTS_SLIME:
			counts.slime++;
			break;
		case CONTENTS_LAVA:
			counts.lava++;
			break;
		case CONTENTS_SKY:
			counts.sky++;
			ignorevis = true;
			break;
		default:
			counts.misc++;
			break;
	}
	if (leaf->visofs == -1 || ignorevis || options.novis) {
		counts.cast += bsp->numfaces;
		novislights[num_novislights++] = entity;
	} else {
		vis_size = (bsp->numleafs + 7) / 8;
		vis = alloca (vis_size + (bsp->numfaces + 7) / 8);
		surfacehit = vis + vis_size;
		memset (surfacehit, 0, (bsp->numfaces + 7) / 8);

		DecompressVis (bsp->visdata + leaf->visofs, vis, vis_size);
		for (i = 0, leaf = bsp->leafs + 1; i < bsp->models[0].visleafs;
			 i++, leaf++) {
			if (!leaf->nummarksurfaces)
				continue;
			if (vis[i >> 3] & (1 << (i & 7))) {
				for (j = 0, mark = bsp->marksurfaces + leaf->firstmarksurface;
					 j < leaf->nummarksurfaces; j++, mark++) {
					mark_face (*mark, surfacehit, entity);
				}
			}
		}
		for (i = 1; i < bsp->nummodels; i++) {
			for (j = 0; (int) j < bsp->models[i].numfaces; j++) {
				//FIXME vis
				mark_face (bsp->models[i].firstface + j, surfacehit, entity);
			}
		}
	}
}

void
VisStats (void)
{
	int         i, count;

	printf ("%4i lights\n", counts.lights);
	printf ("%4i air\n", counts.empty);
	printf ("%4i solid\n", counts.solid);
	printf ("%4i water\n", counts.water);
	printf ("%4i slime\n", counts.slime);
	printf ("%4i lava\n", counts.lava);
	printf ("%4i sky\n", counts.sky);
	printf ("%4i unknown\n", counts.misc);

	for (i = count = 0; i < bsp->numfaces; i++)
		if (surfacelightchain[i])
			count++;
	printf ("%i faces, %i (%i%%) may receive light\n", bsp->numfaces,
			count, count * 100 / bsp->numfaces);
	if (counts.solid || counts.sky)
		printf ("warning: %i lights of %i lights (%i%%) were found in sky\n"
				"or solid and will not be accelerated using vis, move them\n"
				"out of the solid or sky to accelerate compiling\n",
				 counts.solid + counts.sky, counts.lights,
				(counts.solid + counts.sky) * 100 / counts.lights);
	printf ("%i lights will be cast onto %i surfaces, %i casts will "
			"be performed\n", counts.lights, bsp->numfaces, counts.cast);
}
