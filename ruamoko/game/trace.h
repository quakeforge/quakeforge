#ifndef __trace_h
#define __trace_h

// set by traceline / tracebox
@extern float		trace_allsolid;
@extern float		trace_startsolid;
@extern float		trace_fraction;
@extern vector		trace_endpos;
@extern vector		trace_plane_normal;
@extern float		trace_plane_dist;
@extern entity		trace_ent;
@extern float		trace_inopen;
@extern float		trace_inwater;

@extern void(vector v1, vector v2, float nomonsters, entity forent) traceline;

#endif//__trace_h
