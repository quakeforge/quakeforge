/**
	\defgroup physics Physics builtins
	\{
*/
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
@extern vector		v_forward;
@extern vector		v_up;
@extern vector		v_right;

/**
	Set #v_forward, #v_up, #v_right global vectors from the vector \a ang
*/
@extern void makevectors (vector ang);
@extern void traceline (vector v1, vector v2, float nomonsters, entity forent);
@extern entity checkclient ();
@extern float walkmove (float yaw, float dist);
@extern float droptofloor ();
@extern void lightstyle (float style, string value);
@extern float checkbottom (entity e);
@extern float pointcontents (vector v);
@extern vector aim (entity e, float speed);
@extern void ChangeYaw (void);
@extern void movetogoal (float step);
@extern int hullpointcontents (entity ent, vector point);
@extern vector getboxbounds (int hull, int max);
@extern int getboxhull (void);
@extern void freeboxhull (int hull);
@extern void rotate_bbox (int hull, vector right, vector forward, vector up, vector mins, vector maxs);

/**
	\}
*/

#endif//__ruamoko_physics_h
