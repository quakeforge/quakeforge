#ifndef __ruamoko_physics_h
#define __ruamoko_physics_h

@extern float		trace_allsolid;
@extern float		trace_startsolid;
@extern float		trace_fraction;
@extern vector		trace_endpos;
@extern vector		trace_plane_normal;
@extern float		trace_plane_dist;
@extern entity		trace_ent;
@extern float		trace_inopen;
@extern float		trace_inwater;

@extern void (vector v1, vector v2, float nomonsters, entity forent) traceline;
@extern entity () checkclient;
@extern float (float yaw, float dist) walkmove;
@extern float () droptofloor;
@extern void (float style, string value) lightstyle;
@extern float (entity e) checkbottom;
@extern float (vector v) pointcontents;
@extern vector (entity e, float speed) aim;
@extern void () ChangeYaw;
@extern void (float step) movetogoal;
@extern integer (entity ent, vector point) hullpointcontents;
@extern vector (integer hull, integer max) getboxbounds;
@extern integer () getboxhull;
@extern void (integer hull) freeboxhull;
@extern void (integer hull, vector right, vector forward, vector up, vector mins, vector maxs) rotate_bbox;

#endif//__ruamoko_physics_h
