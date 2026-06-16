#ifndef __physics_h
#define __physics_h

enum {
	qent_state,
	qent_body,
	qent_transform,
	qent_collider,
	qent_grav,

	qent_comp_count
};

void set_update (uint ent, void (*update) (uint ent));
bool has_component (uint ent, uint comp);
void get_component (uint ent, uint comp, void *data);
void set_component (uint ent, uint comp, void *data);
uint new_entity ();
void del_entity (uint ent);

extern uint max_collider_ents;
extern uint num_collider_ents;
extern uint *collider_ents;

@overload float min (float x, float y);
@overload float max (float x, float y);

body_t calc_inertia_plane (collider_t collider, float invDensity);
body_t calc_inertia_ball (collider_t collider, float invDensity);
body_t calc_inertia_capsule (collider_t collider, float invDensity);

void update_physics (uint ent);

#endif//__physics_h
