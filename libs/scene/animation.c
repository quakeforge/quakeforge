#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/model.h"
#include "QF/scene/scene.h"
#include "QF/scene/entity.h"

#include "r_local.h"

float
R_EntityBlend (double time, animation_t *animation, int pose, float interval)
{
	float       blend;

	if (animation->nolerp) {
		animation->nolerp = 0;
		animation->pose1 = pose;
		animation->pose2 = pose;
		return 0.0;
	}
	animation->frame_interval = interval;
	if (animation->pose2 != pose) {
		animation->frame_start_time = time;
		if (animation->pose2 == -1) {
			animation->pose1 = pose;
		} else {
			animation->pose1 = animation->pose2;
		}
		animation->pose2 = pose;
		blend = 0.0;
//	} else if (vr_data.paused) {
//		blend = 1.0;
	} else {
		blend = (time - animation->frame_start_time)
				/ animation->frame_interval;
		blend = min (blend, 1.0);
	}
	return blend;
}

static uint32_t
get_frame_data (double time, const anim_t *anim, int framenum,
				const void *base)
{
	if ((framenum >= anim->numdesc) || (framenum < 0)) {
		Sys_MaskPrintf (SYS_dev, "R_GetFrame: no such frame # %d\n",
						framenum);
		return 0;
	}

	auto fdesc = (framedesc_t *) (base + anim->descriptors);
	fdesc += framenum;
	if (fdesc->numframes < 1) {
		return 0;
	}

	auto frame = (frame_t *) (base + anim->frames);
	frame += fdesc->firstframe;

	int   numframes = fdesc->numframes;
	float fullinterval = frame[numframes - 1].endtime;
	// when loading in Mod_LoadAliasSkinGroup, we guaranteed all endtime
	// values are positive, so we don't have to worry about division by 0
	float target = time - ((int) (time / fullinterval)) * fullinterval;

	for (int i = 0; i < (numframes - 1); i++, frame++) {
		if (frame->endtime > target) {
			break;
		}
	}

	return frame->data;
}

static void
update_alias (double time, animation_t *anim, model_t *m)
{
	auto model = m->model;
	if (!model) {
		model = Cache_Get (&m->cache);
	}
	auto mesh = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
	int frame = anim->frame;
	uint32_t data = get_frame_data (time, &mesh->morph, frame, mesh);
	anim->blend = R_EntityBlend (time, anim, data, 0.1);
	if (!m->model) {
		Cache_Release (&m->cache);
	}
}

static void
update_iqm (double time, animation_t *anim, model_t *model)
{
}

static void
update_sprite (double time, animation_t *anim, model_t *model)
{
	auto sprite = (msprite_t *) model->cache.data;	//FIXME
	int frame = anim->frame;
	uint32_t data = get_frame_data (time, &sprite->skin, frame, sprite);
	anim->pose2 = anim->pose1 = data;
}

static void
alias_skin (double time, renderer_t *rend, model_t *m)
{
	auto model = m->model;
	if (!model) {
		model = Cache_Get (&m->cache);
	}
	auto mesh = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
	int skinnum = rend->skin;
	rend->skindesc = get_frame_data (time, &mesh->skin, skinnum, mesh);
	if (!m->model) {
		Cache_Release (&m->cache);
	}
}

static renderer_t *
get_renderer (const ecs_pool_t *pool, uint32_t ent)
{
	uint32_t    ind = pool->sparse[Ent_Index (ent)];
	if (ind < pool->count && pool->dense[ind] == ent) {
		return (renderer_t *) pool->data + ind;
	}
	return nullptr;
}

void
Anim_Update (double time, const ecs_pool_t *animpool,
			 const ecs_pool_t *rendpool)
{
	qfZoneNamed (zone, true);
	uint32_t    count = animpool->count;
	uint32_t   *ent = animpool->dense;
	auto anim = (animation_t *) animpool->data;
	while (count--) {
		auto renderer = get_renderer (rendpool, *ent++);
		if (renderer && renderer->model) {
			float syncbase = anim->syncbase;
			switch (renderer->model->type) {
				case mod_num_types:
				case mod_brush:
				case mod_light:
					break;
				case mod_mesh:
					update_alias (time + syncbase, anim, renderer->model);
					alias_skin (time + syncbase, renderer, renderer->model);
					break;
				case mod_iqm:
					update_iqm (time + syncbase, anim, renderer->model);
					break;
				case mod_sprite:
					update_sprite (time + syncbase, anim, renderer->model);
					break;
			}
		}
		anim++;
	}
}
