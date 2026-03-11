/*
	node.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "QF/ecs.h"
#include "QF/ecs/component.h"
#include "QF/ui/node.h"

static uint32_t
node_link_in_rangeid (ecs_registry_t *reg, uint32_t ent, uint32_t comp)
{
	uint32_t c_link_con = comp + (node_link_con - node_link_in);
	node_link_t *con = Ent_GetComponent (ent, c_link_con, reg);
	uint32_t c_con_in = comp + (node_con_in - node_link_in);
	return *(uint32_t *) Ent_GetComponent (con->dst, c_con_in, reg);
}

static uint32_t
node_link_out_rangeid (ecs_registry_t *reg, uint32_t ent, uint32_t comp)
{
	uint32_t c_link_con = comp + (node_link_con - node_link_out);
	node_link_t *con = Ent_GetComponent (ent, c_link_con, reg);
	uint32_t c_con_out = comp + (node_con_out - node_link_out);
	return *(uint32_t *) Ent_GetComponent (con->src, c_con_out, reg);
}

const component_t node_components[node_comp_count] = {
	[node_link_in] = {
		.size = sizeof (uint32_t),
		.name = "link in",
		.rangeid = node_link_in_rangeid,
	},
	[node_link_out] = {
		.size = sizeof (uint32_t),
		.name = "link out",
		.rangeid = node_link_out_rangeid,
	},
	[node_link_con] = {
		.size = sizeof (node_link_t),
		.name = "link con",
	},
	[node_con_in] = {
		.size = sizeof (uint32_t),
		.name = "con in",
	},
	[node_con_out] = {
		.size = sizeof (uint32_t),
		.name = "con out",
	},
};
