/*
	node.h

	Node editor link management

	Copyright (C) 2026 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_ui_node_h
#define __QF_ui_node_h

enum {
	node_link_in,	// dst connector entity id
	node_link_out,	// src connector entity id
	node_link_con,	// src, dst connector entity id pair

	node_con_in,	// input link group range id
	node_con_out,	// output link group range id

	node_comp_count
};

typedef struct node_link_s {
	uint32_t    src;	// connector entity id
	uint32_t    dst;	// connector entity id
} node_link_t;

extern const component_t node_components[node_comp_count];

#endif//__QF_ui_node_h
