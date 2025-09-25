#ifndef __gizmo_h
#define __gizmo_h

typedef struct gizmo_node_s {
	vec4        plane;
	int         children[2];
} gizmo_node_t;

void Gizmo_AddSphere (vec4 c, float r, vec4 color);
void Gizmo_AddCapsule (vec4 p1, vec4 p2, float r, vec4 color);
void Gizmo_AddBrush (vec4 orig, vec4 mins, vec4 maxs,
					 int num_nodes, gizmo_node_t *nodes, vec4 color);

#endif//__gizmo_h
