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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#include <stdlib.h>

#include "QF/sys.h"

#include "tools/qfbsp/include/brush.h"
#include "tools/qfbsp/include/bsp5.h"
#include "tools/qfbsp/include/draw.h"
#include "tools/qfbsp/include/options.h"
#include "tools/qfbsp/include/portals.h"

/**	\addtogroup qfbsp_portals
*/
//@{

int         c_activeportals, c_peakportals;

node_t      outside_node;				// portals outside the world face this

portal_t *
AllocPortal (void)
{
	portal_t   *p;

	c_activeportals++;
	if (c_activeportals > c_peakportals)
		c_peakportals = c_activeportals;

	p = malloc (sizeof (portal_t));
	memset (p, 0, sizeof (portal_t));

	return p;
}

void
FreePortal (portal_t *p)
{
	c_activeportals--;
	free (p);
}

/**	Link the portal into the nodes on either side of the portal.

	\param p		The portal to link.
	\param front	The node on the front side of the portal.
	\param back		The node on the back side of the portal.
*/
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

/**	Remove the portal from a node.

	The portal most be linked into the node and bounding the node.

	\param portal	The portal to remove.
	\param l		The leaf node from which to remove the portal.
*/
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

/**	Calculate the bounding box of the node based on its portals.

	\param node		The node of which to calculate the bounding box.
*/
static void
CalcNodeBounds (node_t *node)
{
	int         i, j;
	portal_t   *p;
	winding_t  *w;
	int         side;

	for (i=0 ; i<3 ; i++) {
		node->mins[i] = BOGUS_RANGE;
		node->maxs[i] = -BOGUS_RANGE;
	}

	for (p = node->portals ; p ; p = p->next[side]) {
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
			side = 1;
		else
			Sys_Error ("CalcNodeBounds: mislinked portal");

		w = p->winding;
		for (i = 0; i < w->numpoints; i++) {
			for (j=0 ; j<3 ; j++) {
				if (w->points[i][j] < node->mins[j])
					node->mins[j] = w->points[i][j];
				if (w->points[i][j] > node->maxs[j])
					node->maxs[j] = w->points[i][j];
			}
		}
	}
}

/**	Make portals for the head node, initializing outside_node.

	The created portals will face the global outside_node.

	\param node		The head node.
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

	// create a brush based on the enlarged bounding box.
	// The brush has all sides pointing in.
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 2; j++) {
			n = j * 3 + i;

			p = AllocPortal ();
			portals[n] = p;

			pl = &bplanes[n];
			memset (pl, 0, sizeof (*pl));
			pl->normal[i] = 1;
			pl->dist = bounds[j][i];
			if (j)
				PlaneFlip (pl, pl);
			p->planenum = FindPlane (pl, &side);

			p->winding = BaseWindingForPlane (pl);
			if (side)
				AddPortalToNodes (p, &outside_node, node);
			else
				AddPortalToNodes (p, node, &outside_node);
		}
	}

	// clip the basewindings by all the other planes
	for (i = 0; i < 6; i++) {
		for (j = 0; j < 6; j++) {
			if (j == i)
				continue;
			portals[i]->winding = ClipWinding (portals[i]->winding,
											   &bplanes[j], true);
		}
	}
}

/**	Calculate the plane holding the winding.

	Uses the first three points of the winding.

	\param w		The plane for which to calculate the plane.
	\param plane	The plane to set.
*/
static void
PlaneFromWinding (const winding_t *w, plane_t *plane)
{
	vec3_t      v1, v2;

	// calc plane
	VectorSubtract (w->points[2], w->points[1], v1);
	VectorSubtract (w->points[0], w->points[1], v2);
	CrossProduct (v2, v1, plane->normal);
	_VectorNormalize (plane->normal);
	plane->dist = DotProduct (w->points[0], plane->normal);
}

static int cutnode_detail;

/**	Separate the node's portals into its children.

	\param node		The current node.
*/
static void
CutNodePortals_r (node_t *node)
{
	int         side;
	node_t     *f, *b, *other_node;
	plane_t    *plane, clipplane;
	portal_t   *p, *new_portal, *next_portal;
	winding_t  *w, *frontwinding, *backwinding;

//  CheckLeafPortalConsistancy (node);

	CalcNodeBounds (node);

	if (node->contents) {
		/// Leaf nodes contain the final portals.
		return;
	}

	if (node->detail && cutnode_detail) {
		/// Detail nodes are fake leaf nodes.
		return;
	}

	plane = &planes[node->planenum];

	f = node->children[0];
	b = node->children[1];

	/// Create a new portal by taking the full plane winding for the node's
	/// cutting plane and clipping it by all of the planes from the other
	/// portals on the node.
	w = BaseWindingForPlane (plane);
	for (p = node->portals; p; p = p->next[side]) {
		clipplane = planes[p->planenum];	// copy the plane
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node) {
			PlaneFlip (&clipplane, &clipplane);
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
		/// Add the new portal to the node's children.
		new_portal = AllocPortal ();
		new_portal->planenum = node->planenum;

		new_portal->winding = w;
		AddPortalToNodes (new_portal, f, b);
	}

	/// Partition the node's portals by the node's plane, adding each portal's
	/// fragments to the node's children.
	for (p = node->portals; p; p = next_portal) {
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
			side = 1;
		else
			Sys_Error ("CutNodePortals_r: mislinked portal");

		next_portal = p->next[side];
		other_node = p->nodes[!side];

		/// Remove each portal from the node. When finished, the node will
		/// have no portals on it.
		RemovePortalFromNode (p, node);
		/// The fragments will be added back to the other node.
		RemovePortalFromNode (p, other_node);

		/// Cut the portal in two, one on each side of the cut plane.
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

void
PortalizeWorld (node_t *headnode)
{
	qprintf ("----- portalize ----\n");

	MakeHeadnodePortals (headnode);
	cutnode_detail = 0;
	CutNodePortals_r (headnode);
}

void
PortalizeWorldDetail (node_t *headnode)
{
	qprintf ("----- portalize ----\n");

	MakeHeadnodePortals (headnode);
	cutnode_detail = 1;
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

#define	PORTALFILE	"PRT1-AM"

FILE       *pf;
int         num_visleafs;				// leafs the player can be in
int         num_visportals;
int         num_realleafs;

/**	Check if a node has the specified contents.

	\param n		The node to check.
	\param cont		The contents for which to check.
	\return			1 if the node has the specified contents, otherwise 0.
*/
static __attribute__((pure)) int
HasContents (const node_t *n, int cont)
{
	if (n->contents == cont)
		return 1;
	if (n->contents)
		return 0;
	if (HasContents (n->children[0], cont))
		return 1;
	return HasContents (n->children[1], cont);
}

/**	Check if two nodes have the same non-solid contents somewhere within them.

	\param n1		The first node to check.
	\param n2		The second node to check.
*/
static __attribute__((pure)) int
ShareContents (const node_t *n1, const node_t *n2)
{
	if (n1->contents) {
		if (n1->contents == CONTENTS_SOLID)
			return 0;
		else
			return HasContents (n2, n1->contents);
	}

	if (ShareContents (n1->children[0], n2))
		return 1;
	return ShareContents (n1->children[1], n2);
}

/**	Check if two nodes have the same non-solid, non-sky contents.

	\note Affected by watervis.

	\param n1		The first node to check.
	\param n2		The second node to check.
*/
static __attribute__((pure)) int
SameContents (const node_t *n1, const node_t *n2)
{
	if (n1->contents == CONTENTS_SOLID || n2->contents == CONTENTS_SOLID)
		return 0;
	if (n1->contents == CONTENTS_SKY || n2->contents == CONTENTS_SKY)
		return 0;
	if (options.watervis)	//FIXME be more picky?
		return 1;
	if (n1->detail && n2->detail)
		return ShareContents (n1, n2);
	if (n1->detail)
		return HasContents (n1, n2->contents);
	if (n2->detail)
		return HasContents (n2, n1->contents);
	return n1->contents == n2->contents;
}

/**	Recurse through the world bsp, writing the portals for each leaf node to
	the portal file.

	\param node		The current node of the bsp. Call with the root node.
*/
static void
WritePortalFile_r (const node_t *node)
{
	int         i;
	const plane_t *pl;
	plane_t     plane2;
	const portal_t *p;
	const winding_t *w;

	if (!node->contents && !node->detail) {
		WritePortalFile_r (node->children[0]);
		WritePortalFile_r (node->children[1]);
		return;
	}

	if (node->contents == CONTENTS_SOLID)
		return;

	for (p = node->portals; p;) {
		w = p->winding;
		if (w && p->nodes[0] == node
			&& SameContents (p->nodes[0], p->nodes[1])) {
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
			for (i = 0; i < w->numpoints - 1; i++) {
				fprintf (pf, "(%g %g %g) ",
						 w->points[i][0], w->points[i][1], w->points[i][2]);
			}
			fprintf (pf, "(%g %g %g)\n",
					 w->points[i][0], w->points[i][1], w->points[i][2]);
		}

		if (p->nodes[0] == node)
			p = p->next[0];
		else
			p = p->next[1];
	}
}

/**	Write the vis leaf number to the portal file.

	\param n		The current node of the bsp. Call with the root node.
*/
static void
WritePortalLeafs_r (const node_t *n)
{
	if (!n->contents) {
		WritePortalLeafs_r (n->children[0]);
		WritePortalLeafs_r (n->children[1]);
	} else {
		if (n->visleafnum != -1)
			fprintf (pf, "%i\n", n->visleafnum);
	}
}

/**	Set the vis leaf number of the leafs in a detail cluster.

	\param n		The current node. Call with the detail node.
	\param num		The vis leaf number.
*/
static void
SetCluster_r (node_t *n, int num)
{
	if (n->contents == CONTENTS_SOLID) {
		// solid block, viewpoint never inside
		n->visleafnum = -1;
		return;
	}

	n->visleafnum = num;
	if (!n->contents) {
		SetCluster_r (n->children[0], num);
		SetCluster_r (n->children[1], num);
	} else
		num_realleafs++;
}

/**	Set the vis leaf number of the leafs in a bsp tree.

	\param node		The current node. Call with the root node.
*/
static void
NumberLeafs_r (node_t *node)
{
	portal_t   *p;

	if (!node->contents && !node->detail) {
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
			// write out from only the first leaf
			if (SameContents(p->nodes[0], p->nodes[1]))
				num_visportals++;
			p = p->next[0];
		} else
			p = p->next[1];
	}

	if (node->detail) {
		SetCluster_r (node->children[0], node->visleafnum);
		SetCluster_r (node->children[1], node->visleafnum);
	} else {
		num_realleafs++;
	}
}

void
WritePortalfile (node_t *headnode)
{
	// set the visleafnum field in every leaf and count the total number of
	// portals
	num_visleafs = 0;
	num_visportals = 0;
	num_realleafs = 0;
	NumberLeafs_r (headnode);

	// write the file
	printf ("writing %s\n", options.portfile);
	pf = fopen (options.portfile, "w");
	if (!pf)
		Sys_Error ("Error opening %s", options.portfile);

	fprintf (pf, "%s\n", PORTALFILE);
	fprintf (pf, "%i\n", num_visleafs);
	fprintf (pf, "%i\n", num_visportals);
	fprintf (pf, "%i\n", num_realleafs);

	WritePortalFile_r (headnode);

	WritePortalLeafs_r (headnode);

	fclose (pf);
}

//@}
