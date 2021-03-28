/*
	base-vis.c

	PVS PHS generator tool

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
#include "tools/qfvis/include/options.h"

/*
	This is a rough first-order aproximation that is used to trivially reject
	some of the final calculations.
*/
static void
SimpleFlood (basethread_t *thread, portal_t *srcportal, int clusternum)
{
    int			i;
    cluster_t  *cluster;
    portal_t   *portal;

	if (set_is_member (srcportal->mightsee, clusternum))
		return;
	set_add (srcportal->mightsee, clusternum);
    thread->clustersee++;

    cluster = &clusters[clusternum];

    for (i = 0; i < cluster->numportals; i++) {
		portal = cluster->portals[i];
		if (!set_is_member (thread->portalsee, portal - portals))
			continue;
		SimpleFlood (thread, srcportal, portal->cluster);
    }
}

static inline int
test_sphere (const vspheref_t *sphere, vec4f_t plane)
{
	const vec4f_t zero = {};
	float       r = sphere->radius;
	vec4f_t     eps = { r, r, r, r };
	vec4f_t     d = _mm_addsub_ps (zero, dotf (sphere->center, plane));
	vec4i_t     c = (d - eps) >= 0;

	c = (vec4i_t) _mm_hsub_epi32 ((__m128i) c, (__m128i) c);
	return c[0];
}

void
PortalBase (basethread_t *thread, portal_t *portal)
{
	unsigned    i, j, k;
	vec4f_t     d;
	portal_t   *tp;
	winding_t  *winding;
	int         tp_side, portal_side;

	i = portal - portals;

	for (j = 0, tp = portals; j < numportals * 2; j++, tp++) {
		if (j == i)
			continue;

		// If the target portal is behind the portals's plane, then it
		// can't possibly be seen by the portal.
		//
		// If the portal is in front of the target's plane, then the target
		// is of no interest as it is facing counter to the flow of
		// visibility.

		// First check using the bounding spheres of the two portals.
		tp_side = test_sphere (&tp->sphere, portal->plane);
		if (tp_side < 0) {
			// The test portal definitely is entirely behind the portal's
			// plane.
			thread->spherecull++;
			continue;	// entirely behind
		}
		portal_side = test_sphere (&portal->sphere, tp->plane);
		if (portal_side > 0) {
			// The portal definitely is entirely in front of the test
			// portal's plane.
			thread->spherecull++;
			continue;	// entirely in front
		}

		if (tp_side == 0) {
			// The test portal's sphere touches the portal's plane, so
			// do a more refined check.
			winding = tp->winding;
			for (k = 0; k < winding->numpoints; k++) {
				d = dotf (winding->points[k], portal->plane);
				if (d[0] > ON_EPSILON)
					break;
			}
			if (k == winding->numpoints) {
				thread->windingcull++;
				continue;	// no points on front
			}
		}

		if (portal_side == 0) {
			// The portal's sphere touches the test portal's plane, so
			// do a more refined check.
			winding = portal->winding;
			for (k = 0; k < winding->numpoints; k++) {
				d = dotf (winding->points[k], tp->plane);
				if (d[0] < -ON_EPSILON)
					break;
			}
			if (k == winding->numpoints) {
				thread->windingcull++;
				continue;	// no points on front
			}
		}

		set_add (thread->portalsee, j);
	}

	SimpleFlood (thread, portal, portal->cluster);
	portal->nummightsee = thread->clustersee;
}
