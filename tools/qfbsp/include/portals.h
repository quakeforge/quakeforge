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

	$Id$
*/

#ifndef qfbsp_portals_h
#define qfbsp_portals_h

struct node_s;

typedef struct portal_s {
	int         planenum;
	struct node_s *nodes[2];		// [0] = front side of planenum
	struct portal_s *next[2];
	struct winding_s *winding;
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

void PortalizeWorld (struct node_s *headnode);
void PortalizeWorldDetail (struct node_s *headnode);	// stop at detail nodes
void WritePortalfile (struct node_s *headnode);
void FreeAllPortals (struct node_s *node);

#endif//qfbsp_portals_h
