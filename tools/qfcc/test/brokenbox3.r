typedef @handle(long) transform_h transform_t;
mat4x4 Transform_GetWorldMatrix (transform_t transform) = #0;
void printf (string fmt, ...) = #0;
typedef struct gizmo_node_s {
	vec4        plane;
	int         children[2];
} gizmo_node_t;

void Gizmo_AddBrush (vec4 orig, vec4 mins, vec4 maxs,
                     int num_nodes, gizmo_node_t *nodes, vec4 color) = #0;

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.group_mask(0x6) rotor_t;
typedef PGA.group_mask(0xc) translator_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;

typedef struct body_s {
	motor_t     R;
	bivector_t  I;
	bivector_t  Ii;
} body_t;

typedef enum col_type_e {
	col_plane,
	col_ball,
	col_capsule,
	col_box,
} col_type_t;

typedef struct collider_s {
	union {
		plane_t plane;
		struct {
			vec3 offset;	// point_t with implied 1 e123
			float radius;
		} ball;
		struct {
			vec3 offset;	// point_t with implied 1 e123
			float radius;
			vec3 axis;		// point_t with implied 0 e123
		} capsule;
		struct {
			vec3 offset;
			vec3 extent;
		} box;
	};
	col_type_t  type;
} collider_t;

void box (motor_t M, gizmo_node_t *rotated_box)
{
	auto q = M.scalar + M.bvect;
	static gizmo_node_t box_brush[] = {
		{ .plane = {1, 0, 0, 1 }, .children = { 1, -1} },
	};
	for (uint i = 0; i < countof(box_brush); i++) {
		rotated_box[i] = {
			.plane = (vec4) (q),
			.children = box_brush[i].children,
		};
	}
}

int main ()
{
	motor_t M = {
		.scalar = 0.6,
		.bvect = '0.3 0.4 0.8',
		.bvecp = '0.5 0.5 0.5',
		.qvec = 0.5,
	};
	gizmo_node_t nodes[4];
	box (M, nodes);
	printf ("%q %d %d\n", nodes[0].plane,
			nodes[0].children[0], nodes[0].children[1]);
	if (nodes[0].plane != '0.3 0.4 0.8 0.6'
		|| nodes[0].children[0] != 1
		|| nodes[0].children[1] != -1) {
		return 1;
	}
	return 0;
}
