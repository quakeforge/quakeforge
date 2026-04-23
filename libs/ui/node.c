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

uint32_t
Node_AddLink (node_system_t node_sys, node_link_t link)
{
	auto reg = node_sys.reg;
	uint32_t base = node_sys.base;
	uint32_t link_ent = ECS_NewEntity (reg);
	Ent_SetComponent (link_ent, base + node_link_con, reg, &link);
	Ent_SetComponent (link_ent, base + node_link_out, reg, &link.src);
	Ent_SetComponent (link_ent, base + node_link_in,  reg, &link.dst);
	return link_ent;
}

uint32_t
Node_AddOutput (node_system_t node_sys, uint32_t ent)
{
	auto reg = node_sys.reg;
	uint32_t base = node_sys.base;
	uint32_t grp_id = ECS_NewSubpoolRange (reg, base + node_link_out);
	Ent_SetComponent (ent, base + node_con_out, reg, &grp_id);
	return grp_id;
}

uint32_t
Node_AddInput (node_system_t node_sys, uint32_t ent)
{
	auto reg = node_sys.reg;
	uint32_t base = node_sys.base;
	uint32_t grp_id = ECS_NewSubpoolRange (reg, base + node_link_in);
	Ent_SetComponent (ent, base + node_con_in, reg, &grp_id);
	return grp_id;
}

static void
node_getlinks (node_system_t node_sys, uint32_t ent,
			   uint32_t *num_links, node_link_t *links,
			   uint32_t con_comp, uint32_t link_comp)
{
	auto reg = node_sys.reg;
	uint32_t base = node_sys.base;
	uint32_t *grp_id = Ent_GetComponent (ent, base + con_comp, reg);
	uint32_t ind = Ent_Index (*grp_id);
	auto pool = &reg->comp_pools[base + link_comp];
	auto subpool = &reg->subpools[base + link_comp];
	if (subpool->rangeids[ind] != *grp_id) {
		*num_links = 0;
		return;
	}
	uint32_t sind = subpool->sorted[ind];
	uint32_t end = subpool->ranges[sind];
	uint32_t start = sind ? subpool->ranges[sind - 1] : 0;
	*num_links = end - start;
	if (links) {
		for (uint32_t i = start; i < end; i++) {
			uint32_t link_ent = pool->dense[i];
			auto c = Ent_GetComponent (link_ent, base + node_link_con, reg);
			links[i - start] = *(node_link_t *) c;
		}
	}
}

void
Node_GetOutputLinks (node_system_t node_sys, uint32_t ent,
					 uint32_t *num_links, node_link_t *links)
{
	node_getlinks (node_sys, ent, num_links, links,
				   node_con_out, node_link_out);
}

void
Node_GetInputLinks (node_system_t node_sys, uint32_t ent,
					uint32_t *num_links, node_link_t *links)
{
	node_getlinks (node_sys, ent, num_links, links, node_con_in, node_link_in);
}
