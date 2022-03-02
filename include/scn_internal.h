#ifndef __scn_internal_h
#define __scn_internal_h

#include "QF/progs.h"
#include "QF/scene/entity.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

typedef struct scene_resources_s {
	PR_RESMAP (entity_t) entities;
	PR_RESMAP (transform_t) transforms;
} scene_resources_t;

void scene_add_root (scene_t *scene, transform_t *transform);
void scene_del_root (scene_t *scene, transform_t *transform);

#endif//__scn_internal_h
