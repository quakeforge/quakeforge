/*
	entity.h

	ECS entity management

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/10/07

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

#ifndef __QF_ecs_entity_h
#define __QF_ecs_entity_h

/** \defgroup ecs_entity Entity Component System Entities
	\ingroup ecs
*/
///@{

#define ENT_IDBITS 20
#define nullent (~0u)

#define ENTINLINE GNU89INLINE inline

ENTINLINE int ECS_EntValid (uint32_t id, ecs_registry_t *reg);

ENTINLINE uint32_t Ent_Index (uint32_t id);
ENTINLINE uint32_t Ent_Generation (uint32_t id);
ENTINLINE uint32_t Ent_NextGen (uint32_t id);

ENTINLINE int Ent_HasComponent (uint32_t ent, uint32_t comp,
								 ecs_registry_t *reg);
ENTINLINE void *Ent_GetComponent (uint32_t ent, uint32_t comp,
								   ecs_registry_t *reg);
ENTINLINE void *Ent_SetComponent (uint32_t ent, uint32_t comp,
								   ecs_registry_t *registry, const void *data);

#undef ENTINLINE
#ifndef IMPLEMENT_ECS_ENTITY_Funcs
#define ENTINLINE GNU89INLINE inline
#else
#define ENTINLINE VISIBLE
#endif

ENTINLINE uint32_t
Ent_Index (uint32_t id)
{
	return id & ((1 << ENT_IDBITS) - 1);
}

ENTINLINE uint32_t
Ent_Generation (uint32_t id)
{
	return id & ~((1 << ENT_IDBITS) - 1);
}

ENTINLINE uint32_t
Ent_NextGen(uint32_t id)
{
	return id + (1 << ENT_IDBITS);
}

ENTINLINE int
ECS_EntValid (uint32_t id, ecs_registry_t *reg)
{
	uint32_t    ind = Ent_Index (id);
	return ind < reg->num_entities && reg->entities[ind] == id;
}

ENTINLINE int
Ent_HasComponent (uint32_t ent, uint32_t comp, ecs_registry_t *reg)
{
	ecs_pool_t *pool = &reg->comp_pools[comp];
	uint32_t    ind = pool->sparse[Ent_Index (ent)];
	return ind < pool->count && pool->dense[ind] == ent;
}

ENTINLINE void *
Ent_GetComponent (uint32_t ent, uint32_t comp, ecs_registry_t *reg)
{
	const component_t *component = &reg->components[comp];
	uint32_t    ind = reg->comp_pools[comp].sparse[Ent_Index (ent)];
	byte       *data = reg->comp_pools[comp].data;
	return data + ind * component->size;
}

void *Ent_AddComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry);
void Ent_RemoveComponent (uint32_t ent, uint32_t comp,
						  ecs_registry_t *registry);

ENTINLINE void *
Ent_SetComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry,
				  const void *data)
{
	void       *dst = Ent_AddComponent (ent, comp, registry);
	if (data) {
		return Component_CopyElements (&registry->components[comp],
									   dst, 0, data, 0, 1);
	} else {
		return Component_CreateElements (&registry->components[comp],
										 dst, 0, 1);
	}
}

#undef ENTINLINE

///@}

#endif//__QF_ecs_entity_h
