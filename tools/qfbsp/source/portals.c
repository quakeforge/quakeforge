/*
	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	See file, 'COPYING', for details.
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include "string.h"
#endif

#include "QF/sys.h"

#include "bsp5.h"
#include "options.h"

node_t      outside_node;				// portals outside the world face this


static void
AddPortalToNodes (portal_t *p, node_t *front, node_t *back)
{
	if (p->nodes[0] || p->nodes[1])
		Sys_Error ("AddPortalToNode: allready included");

	p->nodes[0] = front;
	p->next[0] = front->portals;
	front->portals = p;

	p->nodes[1] = back;
	p->next[1] = back->portals;
	back->portals = p;
}

static void
RemovePortalFromNode (portal_t *portal, node_t *l)
{
	portal_t  **pp, *t;

	// remove reference to the current portal
	pp = &l->portals;
	while (1) {
		t = *pp;
		if (!t)
			Sys_Error ("RemovePortalFromNode: portal not in leaf");

		if (t == portal)
			break;

		if (t->nodes[0] == l)
			pp = &t->next[0];
		else if (t->nodes[1] == l)
			pp = &t->next[1];
		else
			Sys_Error ("RemovePortalFromNode: portal not bounding leaf");
	}

	if (portal->nodes[0] == l) {
		*pp = portal->next[0];
		portal->nodes[0] = NULL;
	} else if (portal->nodes[1] == l) {
		*pp = portal->next[1];
		portal->nodes[1] = NULL;
	}
}

/*
	MakeHeadnodePortals

	The created portals will face the global outside_node
*/
static void
MakeHeadnodePortals (node_t *node)
{
	int         side, i, j, n;
	plane_t     bplanes[6], *pl;
	portal_t   *p, *portals[6];
	vec3_t      bounds[2];

	Draw_ClearWindow ();

	// pad with some space so there will never be null volume leafs
	for (i = 0; i < 3; i++) {
		bounds[0][i] = brushset->mins[i] - SIDESPACE;
		bounds[1][i] = brushset->maxs[i] + SIDESPACE;
	}

	outside_node.contents = CONTENTS_SOLID;
	outside_node.portals = NULL;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 2; j++) {
			n = j * 3 + i;

			p = AllocPortal ();
			portals[n] = p;

			pl = &bplanes[n];
			memset (pl, 0, sizeof (*pl));
			if (j) {
				pl->normal[i] = -1;
				pl->dist = -bounds[j][i];
			} else {
				pl->normal[i] = 1;
				pl->dist = bounds[j][i];
			}
			p->planenum = FindPlane (pl, &side);

			p->winding = BaseWindingForPlane (pl);
			if (side)
				AddPortalToNodes (p, &outside_node, node);
			else
				AddPortalToNodes (p, node, &outside_node);
		}

	// clip the basewindings by all the other planes
	for (i = 0; i < 6; i++) {
		for (j = 0; j < 6; j++) {
			if (j == i)
				continue;
			portals[i]->winding =
				ClipWinding (portals[i]->winding, &bplanes[j], true);
		}
	}
}

//============================================================================


static void
PlaneFromWinding (winding_t *w, plane_t *plane)
{
	vec3_t      v1, v2;

	// calc plane
	VectorSubtract (w->points[2], w->points[1], v1);
	VectorSubtract (w->points[0], w->points[1], v2);
	CrossProduct (v2, v1, plane->normal);
	_VectorNormalize (plane->normal);
	plane->dist = DotProduct (w->points[0], plane->normal);
}

static void
CutNodePortals_r (node_t *node)
{
	int         side;
	node_t     *f, *b, *other_node;
	plane_t    *plane, clipplane;
	portal_t   *p, *new_portal, *next_portal;
	winding_t  *w, *frontwinding, *backwinding;

//  CheckLeafPortalConsistancy (node);

	// seperate the portals on node into it's children  
	if (node->contents)
		return;							// at a leaf, no more dividing

	plane = &planes[node->planenum];

	f = node->children[0];
	b = node->children[1];

	// create the new portal by taking the full plane winding for the cutting
	// plane and clipping it by all of the planes from the other portals
	new_portal = AllocPortal ();
	new_portal->planenum = node->planenum;

	w = BaseWindingForPlane (&planes[node->planenum]);
	side = 0;
	for (p = node->portals; p; p = p->next[side]) {
		clipplane = planes[p->planenum];
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node) {
			clipplane.dist = -clipplane.dist;
			VectorSubtract (vec3_origin, clipplane.normal, clipplane.normal);
			side = 1;
		} else
			Sys_Error ("CutNodePortals_r: mislinked portal");

		w = ClipWinding (w, &clipplane, true);
		if (!w) {
			printf ("WARNING: CutNodePortals_r:new portal was clipped away\n");
			break;
		}
	}

	if (w) {
		// if the plane was not clipped on all sides, there was an error
		new_portal->winding = w;
		AddPortalToNodes (new_portal, f, b);
	}

	// partition the portals
	for (p = node->portals; p; p = next_portal) {
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
			side = 1;
		else
			Sys_Error ("CutNodePortals_r: mislinked portal");
		next_portal = p->next[side];

		other_node = p->nodes[!side];
		RemovePortalFromNode (p, p->nodes[0]);
		RemovePortalFromNode (p, p->nodes[1]);

		// cut the portal into two portals, one on each side of the cut plane
		DivideWinding (p->winding, plane, &frontwinding, &backwinding);

		if (!frontwinding) {
			if (side == 0)
				AddPortalToNodes (p, b, other_node);
			else
				AddPortalToNodes (p, other_node, b);
			continue;
		}
		if (!backwinding) {
			if (side == 0)
				AddPortalToNodes (p, f, other_node);
			else
				AddPortalToNodes (p, other_node, f);
			continue;
		}
		// the winding is split
		new_portal = AllocPortal ();
		*new_portal = *p;
		new_portal->winding = backwinding;
		FreeWinding (p->winding);
		p->winding = frontwinding;

		if (side == 0) {
			AddPortalToNodes (p, f, other_node);
			AddPortalToNodes (new_portal, b, other_node);
		} else {
			AddPortalToNodes (p, other_node, f);
			AddPortalToNodes (new_portal, other_node, b);
		}
	}

	DrawLeaf (f, 1);
	DrawLeaf (b, 2);

	CutNodePortals_r (f);
	CutNodePortals_r (b);
}

/*
	PortalizeWorld

	Builds the exact polyhedrons for the nodes and leafs
*/
void
PortalizeWorld (node_t *headnode)
{
	qprintf ("----- portalize ----\n");

	MakeHeadnodePortals (headnode);
	CutNodePortals_r (headnode);
}

void
FreeAllPortals (node_t *node)
{
	portal_t   *p, *nextp;

	if (!node->contents) {
		FreeAllPortals (node->children[0]);
		FreeAllPortals (node->children[1]);
	}

	for (p = node->portals; p; p = nextp) {
		if (p->nodes[0] == node)
			nextp = p->next[0];
		else
			nextp = p->next[1];
		RemovePortalFromNode (p, p->nodes[0]);
		RemovePortalFromNode (p, p->nodes[1]);
		FreeWinding (p->winding);
		FreePortal (p);
	}
}

// PORTAL FILE GENERATION

#define	PORTALFILE	"PRT1"

FILE       *pf;
int         num_visleafs;				// leafs the player can be in
int         num_visportals;

static void
WriteFloat (FILE *f, vec_t v)
{
	if (fabs (v - RINT (v)) < 0.001)
		fprintf (f, "%i ", (int) RINT (v));
	else
		fprintf (f, "%f ", v);
}

static void
WritePortalFile_r (node_t *node)
{
	int         i;
	plane_t    *pl, plane2;
	portal_t   *p;
	winding_t  *w;

	if (!node->contents) {
		WritePortalFile_r (node->children[0]);
		WritePortalFile_r (node->children[1]);
		return;
	}

	if (node->contents == CONTENTS_SOLID)
		return;

	for (p = node->portals; p;) {
		w = p->winding;
		if (w && p->nodes[0] == node
			&& p->nodes[0]->contents == p->nodes[1]->contents) {
			// write out to the file

			// sometimes planes get turned around when they are very near the
			// changeover point between different axis.  interpret the plane
			// the same way vis will, and flip the side orders if needed
			pl = &planes[p->planenum];
			PlaneFromWinding (w, &plane2);
			if (DotProduct (pl->normal, plane2.normal) < 0.99) { // backwards..
				fprintf (pf, "%i %i %i ", w->numpoints,
						 p->nodes[1]->visleafnum, p->nodes[0]->visleafnum);
			} else
				fprintf (pf, "%i %i %i ", w->numpoints,
						 p->nodes[0]->visleafnum, p->nodes[1]->visleafnum);
			for (i = 0; i < w->numpoints; i++) {
				fprintf (pf, "(");
				WriteFloat (pf, w->points[i][0]);
				WriteFloat (pf, w->points[i][1]);
				WriteFloat (pf, w->points[i][2]);
				fprintf (pf, ") ");
			}
			fprintf (pf, "\n");
		}

		if (p->nodes[0] == node)
			p = p->next[0];
		else
			p = p->next[1];
	}

}

static void
NumberLeafs_r (node_t *node)
{
	portal_t   *p;

	if (!node->contents) {
		// decision node
		node->visleafnum = -99;
		NumberLeafs_r (node->children[0]);
		NumberLeafs_r (node->children[1]);
		return;
	}

	Draw_ClearWindow ();
	DrawLeaf (node, 1);

	if (node->contents == CONTENTS_SOLID) {
		// solid block, viewpoint never inside
		node->visleafnum = -1;
		return;
	}

	node->visleafnum = num_visleafs++;

	for (p = node->portals; p;) {
		if (p->nodes[0] == node) {
			// only write out from first leaf
			if (p->nodes[0]->contents == p->nodes[1]->contents)
				num_visportals++;
			p = p->next[0];
		} else
			p = p->next[1];
	}
}

void
WritePortalfile (node_t *headnode)
{
	// set the visleafnum field in every leaf and count the total number of
	// portals
	num_visleafs = 0;
	num_visportals = 0;
	NumberLeafs_r (headnode);

	// write the file
	printf ("writing %s\n", options.portfile);
	pf = fopen (options.portfile, "w");
	if (!pf)
		Sys_Error ("Error opening %s", options.portfile);

	fprintf (pf, "%s\n", PORTALFILE);
	fprintf (pf, "%i\n", num_visleafs);
	fprintf (pf, "%i\n", num_visportals);

	WritePortalFile_r (headnode);

	fclose (pf);
}
