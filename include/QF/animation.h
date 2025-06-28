/*
	animation.h

	Animation system

	Copyright (C) 2025 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_scene_animation_h
#define __QF_scene_animation_h

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/set.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

#include "QF/model.h"

/** \defgroup scene_animation Animation system
	\ingroup scene
*/
///@{

#include "QF/scene/scene.h"

typedef enum : uint32_t {
	qfc_morph,
	qfc_channel,	// qfm_channel_t
	qfc_event,		// keyframe_t.data is event id, endtime is when triggered
} qfc_type_t;

typedef enum : uint32_t {
	qfc_loop = 1,
	qfc_disabled = 2,
} qfc_flags_t;

typedef struct clipstate_s {
	qfc_type_t  type;
	float       end_time;
	uint32_t    frame;
	uint32_t    clip_id;
	float       frac;
	float       weight;
	qfc_flags_t flags;
	float       pad;
} clipstate_t;

typedef enum : uint32_t {
	qfa_op_clip,
	qfa_op_clip_weighted,
	qfa_op_mask,
	qfa_op_blerp,
	qfa_op_add,
	qfa_op_difference,
} qfa_op_code_t;

typedef struct animstate_s {
	qfm_loc_t   clip_states;
	qfm_loc_t   morph_states;
	//qfm_loc_t   blend_spec;
	//qfm_loc_t   joint_weights;
	uint32_t    armature_id;
	uint32_t    num_joints;
	uint32_t    local_pose;
	uint32_t    global_pose;
	uint32_t    matrix_palette;
	uint32_t    materials;
	float       play_rate;
	double      time;
} __attribute__((aligned (16))) animstate_t;

#define ANIMINLINE GNU89INLINE inline

ANIMINLINE clipstate_t *qfa_clip_states (animstate_t *anim);
ANIMINLINE clipstate_t *qfa_morph_states (animstate_t *anim);
//ANIMINLINE void *qfa_blend_spec (animstate_t *anim);
//ANIMINLINE float *qfa_jointweights (animstate_t *anim);
ANIMINLINE qfm_joint_t *qfa_local_pose (animstate_t *anim);
ANIMINLINE qfm_motor_t *qfa_global_pose (animstate_t *anim);
ANIMINLINE qfm_motor_t *qfa_matrix_palette (animstate_t *anim);
//ANIMINLINE void *qfa_materials (animstate_t *anim);

#undef ANIMINLINE
#ifndef IMPLEMENT_ENTITY_Funcs
#define ANIMINLINE GNU89INLINE inline
#else
#define ANIMINLINE VISIBLE
#endif

ANIMINLINE clipstate_t *
qfa_clip_states (animstate_t *anim)
{
	return (clipstate_t *) ((byte *) anim + anim->clip_states.offset);
}

ANIMINLINE clipstate_t *
qfa_morph_states (animstate_t *anim)
{
	return (clipstate_t *) ((byte *) anim + anim->morph_states.offset);
}

//ANIMINLINE void *qfa_blend_spec (animstate_t *anim);

//ANIMINLINE float *
//qfa_jointweights (animstate_t *anim)
//{
//	return (float *) ((byte *) anim + anim->joint_weights.offset);
//}

ANIMINLINE qfm_joint_t *
qfa_local_pose (animstate_t *anim)
{
	return (qfm_joint_t *) ((byte *) anim + anim->local_pose);
}

ANIMINLINE qfm_motor_t *
qfa_global_pose (animstate_t *anim)
{
	return (qfm_motor_t *) ((byte *) anim + anim->global_pose);
}

ANIMINLINE qfm_motor_t *
qfa_matrix_palette (animstate_t *anim)
{
	return (qfm_motor_t *) ((byte *) anim + anim->matrix_palette);
}

//ANIMINLINE void *qfa_materials (animstate_t *anim);

void qfa_init (void);
void qfa_shutdown (void);

void qfa_register (model_t *mod);
void qfa_deregister (model_t *mod);

bool qfa_extract_root_motion (model_t *mod);

int qfa_find_clip (const char *name);
int qfa_find_armature (const char *name);

animstate_t *qfa_create_animation (uint32_t *clips, uint32_t num_clips,
								   uint32_t armature, qf_model_t *model);
void qfa_free_animation (animstate_t *anim);
void qfa_update_anim (animstate_t *anim, float dt);
void qfa_reset_anim (animstate_t *anim);
void qfa_set_anim_clip (animstate_t *anim, uint32_t slot, uint32_t clip);
///@}

#endif//__QF_scene_animation_h
