#include <string.h>

#include "animfsm.h"
#include "armature.h"

@implementation AnimState

-(void)awakeFromArchive
{
	auto clip = qfa_find_clip (clipname);
	auto evclip = qfa_find_clip ("ev|" + clipname);
	auto rmclip = qfa_find_clip ("rm|" + clipname);
	int ind = strchr (clipname, ':');
	string mod_name = str_mid (clipname, 0, ind);
	auto arma = qfa_find_armature (mod_name);
	auto rmarma = qfa_find_armature ("rm|" + mod_name);

	anim = qfa_create_animation (&clip, 1, &evclip, 1, false, arma);
	qfa_set_clip_loop (anim, 0, false);
	qfa_set_clip_loop (anim, 1, false);

	rmanim = qfa_create_animation (&rmclip, 1, nil, 0, false, rmarma);
	qfa_set_clip_loop (rmanim, 0, false);
}

@end

@implementation AnimFSM
-setEntity:(entity_t) ent
{
	self.ent = ent;
	trans = Entity_GetTransform (ent);
	return self;
}

-(void)awakeFromArchive
{
}

-(bool)check
{
	return clip_ended;
}

-(void)update:(float)dt
{
	//printf ("fsm update %s %g\n", [current_state name], dt);
	animstate_t anim = current_anim;
	animstate_t rmanim = current_rmanim;
	qfa_update_anim (anim, dt);

	qfa_update_anim (rmanim, dt);
	arm_motor_t M;
	qfa_get_pose_motors (rmanim, &M);
	set_transform (M.m * E, trans);


	int num_events;
	auto events = qfa_get_events (anim, &num_events);
	if (num_events) {
		for (int i = 0; i < num_events; i++) {
			//printf ("%d %g %g\n", events[i].data, events[i].start_frac,
			//		events[i].end_frac);
			if (events[i].data == 1) {
				clip_ended = true;
			}
		}
	}

	[super update:dt];
}

-(void)enter
{
	auto state = (AnimState *)currentState;
	//printf ("fsm enter %7s\n", state.name);
	current_anim = state.anim;
	current_rmanim = state.rmanim;
	qfa_reset_anim (current_anim);
	qfa_reset_anim (current_rmanim);
	Entity_SetAnimstate (ent, current_anim);
	E = make_motor (Transform_GetWorldPosition (trans),
					Transform_GetWorldRotation (trans));
	clip_ended = false;
}

@end
