#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/simd/types.h"
#include "QF/mathlib.h"
#include "QF/ecs.h"

enum test_components {
	test_position,
	test_scale,
	test_rotation,

	test_num_components
};

static uint32_t comp_base;
#define t_position (comp_base + test_position)
#define t_scale (comp_base + test_scale)
#define t_rotation (comp_base + test_rotation)

static void
create_position (void *data)
{
	vec4f_t    *pos = data;
	*pos = (vec4f_t) { 0, 0, 0, 1 };
}

static void
create_scale (void *data)
{
	vec_t     *scale = data;
	VectorSet (1, 1, 1, scale);
}

static void
create_rotation (void *data)
{
	vec4f_t    *rot = data;
	*rot = (vec4f_t) { 0, 0, 0, 1 };
}

static const component_t test_components[] = {
	[test_position] = {
		.size = sizeof (vec4f_t),
		.create = create_position,
		.name = "position",
	},
	[test_scale] = {
		.size = sizeof (vec3_t),
		.create = create_scale,
		.name = "scale",
	},
	[test_rotation] = {
		.size = sizeof (vec4f_t),
		.create = create_rotation,
		.name = "rotation",
	},
};

static int
check_ent_components (const uint32_t *ents, uint32_t count, uint32_t comp,
					  ecs_registry_t *reg)
{
	ecs_pool_t *pool = &reg->comp_pools[comp];
	const component_t *component = &reg->components.a[comp];
	if (pool->count != count) {
		printf ("%s pool has wrong object count: %d %d\n", component->name,
				pool->count, count);
		return 0;
	}
	uint32_t    num_entities = 0;
	for (uint32_t i = 0; i < reg->max_entities; i++) {
		if (pool->sparse[i] == nullent) {
			continue;
		}
		if (pool->sparse[i] < pool->count) {
			num_entities++;
			continue;
		}
		printf ("invalid index in sparse array of %s: %d %d\n", component->name,
				pool->sparse[i], pool->count);
		return 0;
	}
	if (num_entities != count) {
		printf ("%s sparse has wrong entity count: %d %d\n", component->name,
				num_entities, count);
		return 0;
	}

	for (uint32_t i = 0; i < count; i++) {
		if (pool->sparse[Ent_Index (ents[i])] == nullent) {
			printf ("ent not in %s sparse: %x\n", component->name, ents[i]);
			return 0;
		}
		if (pool->dense[pool->sparse[Ent_Index (ents[i])]] != ents[i]) {
			printf ("indexed %s dense does have ent: %x %x\n", component->name,
					pool->dense[pool->sparse[Ent_Index (ents[i])]], ents[i]);
			return 0;
		}
	}
	return 1;
}

int
main (void)
{
	ecs_registry_t *reg = ECS_NewRegistry ("components");
	comp_base = ECS_RegisterComponents (reg, test_components,
										test_num_components);
	ECS_CreateComponentPools (reg);

	uint32_t    enta = ECS_NewEntity (reg);
	uint32_t    entb = ECS_NewEntity (reg);
	uint32_t    entc = ECS_NewEntity (reg);
	Ent_AddComponent (enta, t_position, reg);
	Ent_AddComponent (entb, t_position, reg);
	Ent_AddComponent (entc, t_position, reg);
	Ent_AddComponent (enta, t_rotation, reg);
	Ent_AddComponent (entb, t_rotation, reg);
	Ent_AddComponent (enta, t_scale, reg);

	if (!check_ent_components ((uint32_t[]){enta, entb, entc}, 3,
							   t_position, reg)) {
		return 1;
	}
	if (!check_ent_components ((uint32_t[]){enta, entb}, 2,
							   t_rotation, reg)) {
		return 1;
	}
	if (!check_ent_components ((uint32_t[]){enta}, 1, t_scale, reg)) {
		return 1;
	}

	ECS_DelRegistry (reg);
	return 0;
}
