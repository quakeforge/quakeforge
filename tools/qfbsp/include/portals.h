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

#ifndef qfbsp_portals_h
#define qfbsp_portals_h

/**	\defgroup qfbsp_portals Portal Functions
	\ingroup qfbsp

	A portal is the polygonal interface between two leaf nodes, regardless
	of the contents of the two leaf nodes.

	Decision nodes will not have portals on them, though as part of the
	portal building process, they will temporarily have portals.
*/
///@{

struct node_s;

typedef struct portal_s {
	int         planenum;			///< plane holding this portal
	struct node_s *nodes[2];		///< [0] = front side of plane
	struct portal_s *next[2];		///< [0] = front side of plane
	struct winding_s *winding;		///< this portal's polygon
} portal_t;

extern struct node_s outside_node;	// portals outside the world face this

/**	Allocate a new portal.

	Increases \c c_activeportals by one.

	\return			Pointer to the new portal.
*/
portal_t *AllocPortal (void);

/**	Free a portal.

	Only the first portal will be freed. If the portal is linked to other
	portals, those portals will have to be freed seperately.

	Reduces \c c_activeportals by one.

	\param p		The portal to free.
*/
void FreePortal (portal_t *p);

/**	Builds the exact polyhedrons for the nodes and leafs.

	\param headnode	The root of the world bsp.
*/
void PortalizeWorld (struct node_s *headnode);

/**	Builds the exact polyhedrons for the nodes and leafs.

	Like PortalizeWorld, but stop at detail nodes - Alexander Malmberg.

	\param headnode	The root of the world bsp.
*/
void PortalizeWorldDetail (struct node_s *headnode);	// stop at detail nodes

/**	Free all portals from a node and its decendents.

	\param node		The node from which to remove and free portals.
*/
void FreeAllPortals (struct node_s *node);

/**	Write the map's portals to the portal file.

	\param headnode	The root of the map's bsp.
*/
void WritePortalfile (struct node_s *headnode);

///@}

#endif//qfbsp_portals_h
