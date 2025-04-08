#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/ecs.h"

static int
test_new_del (void)
{
	ecs_registry_t *reg = ECS_NewRegistry ("new del");
	ECS_CreateComponentPools (reg);

	if (!reg) {
		printf ("could not create registry\n");
		return 0;
	}
	if (reg->entities.max_ids && !reg->entities.ids) {
		printf ("Non-zero max_ids with null ids pointer\n");
		return 0;
	}
	if (!reg->entities.max_ids && reg->entities.ids) {
		printf ("Zero max_ids with non-null ids pointer\n");
		return 0;
	}
	if (reg->entities.num_ids > reg->entities.max_ids) {
		printf ("num_ids > max_ids\n");
		return 0;
	}
	if (reg->entities.num_ids) {
		printf ("Fresh registry has entities\n");
		return 0;
	}
	if (reg->entities.available) {
		printf ("Fresh registry has recycled entities\n");
		return 0;
	}
	ECS_DelRegistry (reg);
	return 1;
}

#define NUM_ENTS 5

static int
test_entities (void)
{
	ecs_registry_t *reg = ECS_NewRegistry ("entities");
	ECS_CreateComponentPools (reg);

	uint32_t    entities[NUM_ENTS];

	for (int i = 0; i < NUM_ENTS; i++) {
		entities[i] = ECS_NewEntity (reg);
	}
	if (!reg->entities.max_ids || !reg->entities.ids) {
		printf ("Zero max_ids or non-null entities pointer\n");
		return 0;
	}
	if (reg->entities.available) {
		printf ("unexpected recycled entities\n");
		return 0;
	}
	if (reg->entities.num_ids != NUM_ENTS) {
		printf ("wrong number of entities in registry: %d vs %d\n", NUM_ENTS,
				reg->entities.num_ids);
		return 0;
	}
	for (int i = 0; i < NUM_ENTS; i++) {
		if (Ent_Index (entities[i]) > reg->entities.num_ids) {
			printf ("bad entity returned: %d > %d\n", Ent_Index (entities[i]),
					reg->entities.num_ids);
			return 0;
		}
		if (Ent_Generation (entities[i])) {
			printf ("fresh entity not generation 0\n");
			return 0;
		}
		for (int j = 0; j < i; j++) {
			if (Ent_Index (entities[j]) == Ent_Index (entities[i])) {
				printf ("duplicate entity id\n");
				return 0;
			}
		}
		if (reg->entities.ids[Ent_Index (entities[i])] != entities[i]) {
			printf ("wrong entity in entities array: %d %d\n",
					entities[i], reg->entities.ids[Ent_Index (entities[i])]);
			return 0;
		}
	}
	ECS_DelEntity (reg, entities[2]);
	ECS_DelEntity (reg, entities[0]);
	if (reg->entities.ids[Ent_Index (entities[2])] == entities[2]) {
		printf ("deleted entity not deleted: %d %d\n",
				entities[2], reg->entities.ids[Ent_Index (entities[2])]);
		return 0;
	}
	if (reg->entities.ids[Ent_Index (entities[0])] == entities[0]) {
		printf ("deleted entity not deleted: %d %d\n",
				entities[0], reg->entities.ids[Ent_Index (entities[0])]);
		return 0;
	}
	if (reg->entities.available != 2) {
		printf ("not 2 available entity for recycling\n");
		return 0;
	}
	for (uint32_t i = reg->entities.next, c = 0; i != Ent_Index (nullent);
		 i = Ent_Index (reg->entities.ids[i]), c++) {
		if (c >= reg->entities.available || i >= reg->entities.num_ids) {
			printf ("invalid deleted entity chain\n");
			return 0;
		}
		if (Ent_Index (reg->entities.ids[i]) == i) {
			printf ("deleted entity points to itself\n");
			return 0;
		}
		//printf ("%x\n", reg->entities[i]);
	}
	entities[2] = ECS_NewEntity (reg);
	if (reg->entities.available != 1) {
		printf ("not 1 available entity for recycling\n");
		return 0;
	}
	entities[0] = ECS_NewEntity (reg);
	if (reg->entities.available != 0) {
		printf ("not 0 available entities for recycling\n");
		return 0;
	}
	if (reg->entities.num_ids != NUM_ENTS) {
		printf ("wrong number of entities in registry: %d vs %d\n", NUM_ENTS,
				reg->entities.num_ids);
		return 0;
	}
	if (!Ent_Generation (entities[2]) || !Ent_Generation (entities[0])) {
		printf ("recycled entity generations not updated\n");
		return 0;
	}
	//for (int i = 0; i < NUM_ENTS; i++) {
	//	printf ("%x\n", entities[i]);
	//}

	ECS_DelRegistry (reg);
	return 1;
}

int
main (void)
{
	if (!test_new_del ()) { return 1; }
	if (!test_entities ()) { return 1; }
	return 0;
}
