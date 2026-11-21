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
	transform_t trans;
	entity_t ent;

	motor_t E;
	float dt;
	bool clip_ended;
	animstate_t current_anim;
	animstate_t current_rmanim;
}
-setEntity:(entity_t) ent;
@end

#endif//__animfsm_h
