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

#include "vis.h"
#include "options.h"

static int  clustersee;
static byte portalsee[MAX_PORTALS];

/*
	This is a rough first-order aproximation that is used to trivially reject
	some of the final calculations.
*/
static void
SimpleFlood (portal_t *srcportal, int clusternum)
{
    int			i;
    cluster_t  *cluster;
    portal_t   *portal;

	if (set_is_member (srcportal->mightsee, clusternum))
		return;
	set_add (srcportal->mightsee, clusternum);
    clustersee++;

    cluster = &clusters[clusternum];

    for (i = 0; i < cluster->numportals; i++) {
		portal = cluster->portals[i];
		if (!portalsee[portal - portals])
			continue;
		SimpleFlood (srcportal, portal->cluster);
    }
}

void
BasePortalVis (void)
{
    int			i, j, k;
	float		d;
    portal_t   *tp, *portal;
    winding_t  *winding;

    for (i = 0, portal = portals; i < numportals * 2; i++, portal++) {
		portal->mightsee = set_new_size (portalclusters);

		memset (portalsee, 0, numportals * 2);

		for (j = 0, tp = portals; j < numportals * 2; j++, tp++) {
			if (j == i)
				continue;

			winding = tp->winding;
			for (k = 0; k < winding->numpoints; k++) {
				d = DotProduct (winding->points[k],
								portal->plane.normal) - portal->plane.dist;
				if (d > ON_EPSILON)
					break;
			}
			if (k == winding->numpoints)
				continue;	// no points on front

			winding = portal->winding;
			for (k = 0; k < winding->numpoints; k++) {
				d = DotProduct (winding->points[k],
								tp->plane.normal) - tp->plane.dist;
				if (d < -ON_EPSILON)
					break;
			}
			if (k == winding->numpoints)
				continue;	// no points on front

			portalsee[j] = 1;
		}

		clustersee = 0;
		SimpleFlood (portal, portal->cluster);
		portal->nummightsee = clustersee;
    }
}
