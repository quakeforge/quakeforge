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
	qfc_event,
} qfc_type_t;

typedef enum : uint32_t {
	qfc_loop = 1,
	qfc_disabled = 2,
} qfc_flags_t;

typedef struct clipstate_s {
	qfc_type_t  type;
	float       end_time;	// animstate_t relative time clip ends
	uint32_t    frame;		// current frame in clip
	uint32_t    clip_id;	// handle for actual clip data
	float       frac;		// progress through frame for inter-frame blending
	float       weight;		// weight for blending between channels
	qfc_flags_t flags;
	float       time_scale;	// smaller = faster
} clipstate_t;

// Describes a span of events in an event channel.
// Any fully spanned events (whether immediate or durational) have an implicit
// fraction of 1.0.
// If num_events is greater than 1, then the span for the first event is from
// first_frac to 1.0 and from 0.0 to last_frac for the last event.
// If num_events is 1 then the span is from first_frac to last_frac.
// Null events (keyframe_t.data = 0) are included.
// While theoretically possible, it is not expected for an immediate event to
// be the first or last event, but if so, the corresponding frac will be 1.0
// Events within a single animstate_t do not blend between channels. Multiple
// channels allow for events to overlap in time.
typedef struct eventspan_s {
	uint32_t    first_event;// index into event channel keyframe array
	uint32_t    num_events;	// never 0?
	float       first_frac;	// position of start time in first event
	float       last_frac;	// position of ent time in last event
} eventspan_t;

typedef struct qfa_event_s {
	uint32_t    data;
	float       start_frac;
	float       end_frac;
} qfa_event_t;

typedef enum : uint32_t {
	qfa_op_clip,
	qfa_op_clip_weighted,
	qfa_op_mask,
	qfa_op_blerp,
	qfa_op_add,
	qfa_op_difference,
} qfa_op_code_t;

typedef struct animstate_s {
	qfm_loc_t   clip_states;		// clipstate_t[]
	qfm_loc_t   morph_states;		// clipstate_t[]
	qfm_loc_t   event_states;		// clipstate_t[]
	//qfm_loc_t   blend_spec;
	qfm_loc_t   joint_weights;		// float[][num_joints] (0 or clip_states)
	uint32_t    armature_id;		// handle for affected armature
	uint32_t    num_joints;			// number of joints in armature (local copy)
	uint32_t    local_pose;			// qfm_joint_t[num_joints] armature space
	uint32_t    global_pose;		// qfm_motor_t[num_joints] model space
	uint32_t    matrix_palette;		// qfm_motor_t[num_joints] global * inverse
	uint32_t    event_spans;		// eventspan_t[]
	uint32_t    materials;
	uint32_t    render_data;
	vec3_t      mins;
	vec3_t      maxs;
	float       play_rate;
	double      time;
} __attribute__((aligned (16))) animstate_t;

#define ANIMINLINE GNU89INLINE inline

ANIMINLINE clipstate_t *qfa_clip_states (animstate_t *anim);
ANIMINLINE clipstate_t *qfa_morph_states (animstate_t *anim);
ANIMINLINE clipstate_t *qfa_event_states (animstate_t *anim);
ANIMINLINE eventspan_t *qfa_event_spans (animstate_t *anim);
//ANIMINLINE void *qfa_blend_spec (animstate_t *anim);
ANIMINLINE float *qfa_jointweights (animstate_t *anim);
ANIMINLINE qfm_joint_t *qfa_local_pose (animstate_t *anim);
ANIMINLINE qfm_motor_t *qfa_global_pose (animstate_t *anim);
ANIMINLINE qfm_motor_t *qfa_matrix_palette (animstate_t *anim);
//ANIMINLINE void *qfa_materials (animstate_t *anim);

#undef ANIMINLINE
#ifndef IMPLEMENT_ANIMATION_Funcs
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

ANIMINLINE clipstate_t *
qfa_event_states (animstate_t *anim)
{
	return (clipstate_t *) ((byte *) anim + anim->event_states.offset);
}

ANIMINLINE eventspan_t *
qfa_event_spans (animstate_t *anim)
{
	return (eventspan_t *) ((byte *) anim + anim->event_spans);
}

//ANIMINLINE void *qfa_blend_spec (animstate_t *anim);

ANIMINLINE float *
qfa_jointweights (animstate_t *anim)
{
	return (float *) ((byte *) anim + anim->joint_weights.offset);
}

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
float qfa_clip_duration (int clip);
int qfa_find_armature (const char *name);

animstate_t *qfa_create_animation (uint32_t *clips, uint32_t num_clips,
								   uint32_t *evclips, uint32_t num_evclilps,
								   bool create_weighs, uint32_t armature);
void qfa_free_animation (animstate_t *anim);
void qfa_update_anim (animstate_t *anim, float dt);
void qfa_reset_anim (animstate_t *anim);
void qfa_set_anim_clip (animstate_t *anim, uint32_t slot, uint32_t clip);

// events must be pre-allocated
void qfa_get_events (animstate_t *anim, qfa_event_t *events);
///@}

#endif//__QF_scene_animation_h
