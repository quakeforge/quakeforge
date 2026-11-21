#ifndef __animfsm_h
#define __animfsm_h

#include <scene.h>
#include "fsm.h"
#include "pga3d.h"

@interface AnimState : FSMState
{
@public
	string clipname;
	animstate_t anim;
	animstate_t rmanim;
}
@end

@interface AnimFSM : FSM
{
	Array *anim_events;
	transform_t trans;
	entity_t ent;

	bool root_enabled;
	motor_t root_anchor;	// entity motor at start of animation
	motor_t root_motion;	// relative motor from root motion track
	float dt;
	bool clip_ended;
	animstate_t current_anim;
	animstate_t current_rmanim;
}
-setEntity:(entity_t) ent;
-pivotRoot:(motor_t) m;
-enableRootMotion:(bool)enabled;
@end

#endif//__animfsm_h
