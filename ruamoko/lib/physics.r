#include <physics.h>

float		trace_allsolid;
float		trace_startsolid;
float		trace_fraction;
vector		trace_endpos;
vector		trace_plane_normal;
float		trace_plane_dist;
entity		trace_ent;
float		trace_inopen;
float		trace_inwater;


void (vector ang) makevectors = #1;
void (vector v1, vector v2, float nomonsters, entity forent) traceline = #16;
entity () checkclient = #17;
float (float yaw, float dist) walkmove = #32;
float () droptofloor = #34;
void (float style, string value) lightstyle = #35;
float (entity e) checkbottom = #40;
float (vector v) pointcontents = #41;
vector (entity e, float speed) aim = #44;
void () ChangeYaw = #49;
void (float step) movetogoal = #67;
int (entity ent, vector point) hullpointcontents = #0x000f0000 + 93;
vector (int hull, int max) getboxbounds = #0x000f0000 + 94;
int () getboxhull = #0x000f0000 + 95;
void (int hull) freeboxhull = #0x000f0000 + 96;
void (int hull, vector right, vector forward, vector up, vector mins, vector maxs) rotate_bbox = #0x000f0000 + 97;
