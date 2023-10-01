/*
	leafmesh.c

	Generate a mesh for a leaf node.

	Copyright (C) 2023  Bill Currie <bill@taniwha.org>

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
// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "qfalloca.h"
#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"
#include "vid_vulkan.h"

typedef struct {
	vec3_t      vel;		// velocity part of Plücker coords
	vec3_t      mom;		// moment part of Plücker coords
	uint32_t    verts[2];	// ~0u if not a valid vert (0 or 1 plane intersects)
} lm_edge_t;

typedef struct {
	vec4f_t    p;			// 1-vector plane (ax + by + cz + d = 0)
} lm_plane_t;

typedef struct {
	vec4f_t    p;			// trivector point
} lm_vert_t;

void *leafmesh (const mod_brush_t *brush, uint32_t leafnum, memhunk_t *hunk);

void *
leafmesh (const mod_brush_t *brush, uint32_t leafnum, memhunk_t *hunk)
{
	if (leafnum > brush->modleafs || leafnum < 1) {
		// invalid leafnum (0 is the infinite solid leaf and doesn't actually
		// exist (also, generally not convex))
		return 0;
	}
	if (!brush->leaf_parents[leafnum]) {
		// with only one parent node, there's no way to generate a mesh for
		// the leaf
		return 0;
	}
	uint32_t num_planes = 0;
	for (int32_t p = brush->leaf_parents[leafnum]; p != -1;
		 p = brush->leaf_parents[p]) {
		num_planes++;
	};
	lm_plane_t *planes = Hunk_RawAlloc (hunk, sizeof (lm_plane_t[num_planes]));
	for (int32_t p = brush->leaf_parents[leafnum], i = 0; p != -1;
		 p = brush->leaf_parents[p], i++) {
		planes[i].p = brush->nodes[p].plane;
	};
}
