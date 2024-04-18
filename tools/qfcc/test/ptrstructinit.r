#include "test-harness.h"
#pragma code optimize

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;

motor_t
make_motor (vec4 translation, vec4 rotation)
{
	@algebra(PGA) {
		auto dt = (PGA.group_mask (0x18)) translation;
		auto q = (PGA.group_mask(0x06)) rotation;
		motor_t t = { .scalar = 1, .bvecp = -dt.bvecp / 2 };
		motor_t r = { .scalar = q.scalar, .bvect = -q.bvect };
		motor_t m = t * r;
		return m;
	}
}

typedef struct {
	vector translate;
	string name;
	quaternion rotate;
	vector scale;
	int parent;
} iqmjoint_t;

typedef struct iqmpose_s {
	vec4 translate;
	vec4 rotate;
	vec4 scale;
} iqmpose_t;

typedef struct edge_s {
	int a;
	int b;
} edge_t;

typedef struct {
	int         num_joints;
	iqmjoint_t *joints;
	iqmpose_t  *basepose;
	iqmpose_t  *pose;
	vec4       *points;
	int         num_edges;
	edge_t     *edges;
	int        *edge_colors;
	int        *edge_bones;	// only one bone per edge
} armature_t;

static edge_t edges[] = {
	// bone octohedron
	{ 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4},
	{ 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 1},
	{ 1, 5 }, { 2, 5 }, { 3, 5 }, { 4, 5},
	// axes
	{ 0, 6 }, { 0, 7}, { 0, 8 },
};
static int edge_colors[] = {
	15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15,
	12, 10, 9,
};
static vec4 points[] = {
	{ 0, 0, 0, 1 },
	{ 0, 1, 1, 1 },
	{ 1, 1, 0, 1 },
	{ 0, 1,-1, 1 },
	{-1, 1, 0, 1 },
	{ 0, 4, 0, 1 },

	{ 1, 0, 0, 1 },
	{ 0, 1, 0, 1 },
	{ 0, 0, 1, 1 },
};

#define JOINT_POINTS (6 + 3)
#define JOINT_EDGES (12 + 3)

void
copy_joints (armature_t *arm, iqmjoint_t *joints, int num_joints)
{
	for (int i = 0; i < num_joints; i++) {
		arm.joints[i] = joints[i];
	}
}

#pragma code optimize
void
set_joint_points (armature_t *arm, int joint, float best_dist)
{
	vec4 scale = { best_dist, best_dist, best_dist, 1 };
	auto pose = arm.basepose[joint];
	auto M = make_motor (pose.translate, pose.rotate);
	for (int j = 0; j < JOINT_POINTS; j++) {
		auto p = (point_t) (points[j] * scale);
		p = M * p * ~M;
		arm.points[joint * JOINT_POINTS + j] = (vec4) p;
	}
}
#pragma code optimize

float
find_best_dist (armature_t *arm, int joint)
{
	// joints are always ordered such that the parent comes before any
	// children, so child joints will never have an index of 0
	int best_child = 0;
	float best_dist = 0;
	for (int j = joint + 1; j < arm.num_joints; j++) {
		if (arm.joints[j].parent == joint) {
			if (arm.joints[j].translate.y > best_dist) {
				best_dist = arm.joints[j].translate.y;
				best_child = j;
			}
		}
	}
	if (!best_child || best_dist < 0.2) {
		best_dist = 0.2;
	}
	return best_dist / 4;
}

#pragma code optimize
void
set_poses (armature_t *arm, iqmjoint_t *joints, int num_joints)
{
	for (int i = 0; i < num_joints; i++) {
		if (joints[i].parent >= 0) {
			auto p = arm.pose[joints[i].parent];
			// printf IS the bug: its parameters get set before p is loaded,
			// otherwise this code compiles correctly
			printf ("%q %q %q\n", p.translate, p.rotate, p.scale);
			arm.pose[i] = {
				.translate = p.translate + [(quaternion)p.rotate * joints[i].translate, 0],
				.rotate = (quaternion)p.rotate * joints[i].rotate,
				.scale = p.scale * [joints[i].scale, 0],
			};
		} else {
			arm.pose[i] = {
				.translate = [joints[i].translate, 0],
				.rotate = joints[i].rotate,
				.scale = [joints[i].scale, 0],
			};
		}
		arm.basepose[i] = arm.pose[i];
	}
}
#pragma code optimize

armature_t *
make_armature (int num_joints, iqmjoint_t *joints)
{
	armature_t *arm = obj_malloc (sizeof (armature_t));
	int num_points = num_joints * JOINT_POINTS;
	int num_edges = num_joints * JOINT_EDGES;
	*arm = {
		.num_joints = num_joints,
		.joints = obj_malloc (num_joints * sizeof (iqmjoint_t)),
		.basepose = obj_malloc (num_joints * sizeof (iqmpose_t)),
		.pose = obj_malloc (num_joints * sizeof (iqmpose_t)),
		.points = obj_malloc (num_points * sizeof (vec4)),
		.num_edges = num_edges,
		.edges = obj_malloc (num_edges * sizeof (edge_t)),
		.edge_colors = obj_malloc (num_edges * sizeof (int)),
		.edge_bones = obj_malloc (num_edges * sizeof (int)),
	};
	if (!ptr_valid (arm.joints)
		|| !ptr_valid (arm.basepose)
		|| !ptr_valid (arm.pose)
		|| !ptr_valid (arm.points)
		|| !ptr_valid (arm.edges)
		|| !ptr_valid (arm.edge_colors)
		|| !ptr_valid (arm.edge_bones)) {
		return arm;
	}
	copy_joints (arm, joints, num_joints);
	set_poses (arm, joints, num_joints);

	for (int i = 0; i < num_joints; i++) {
		for (int j = 0; j < JOINT_EDGES; j++) {
			arm.edges[i * JOINT_EDGES + j] = {
				.a = edges[j].a + i * JOINT_POINTS,
				.b = edges[j].b + i * JOINT_POINTS,
			};
			int color = edge_colors[j];
			arm.edge_colors[i * JOINT_EDGES + j] = color;
			arm.edge_bones[i * JOINT_EDGES + j] = i;
		}
		float best_dist = find_best_dist (arm, i);
		set_joint_points (arm, i, best_dist);
	}
	return arm;
}

static iqmjoint_t joint_data[] = {
	{
		.translate = { 0, 1, 0},
		.name = "root",
		.rotate = { 0.6, 0, 0, 0.8 },
		.scale = { 1, 1, 1 },
		.parent = -1,
	},
	{
		.translate = { 0, 2, 0},
		.name = "flip",
		.rotate = { 0.6, 0, 0, 0.8 },
		.scale = { 1, 1, 1 },
		.parent = 0,
	},
	{
		.translate = { 0, 3, 0},
		.name = "flop",
		.rotate = { 0.6, 0, 0, 0.8 },
		.scale = { 1, 1, 1 },
		.parent = 1,
	},
};

int
main ()
{
	int num_joints = sizeof (joint_data) / sizeof (joint_data[0]);
	auto arm = make_armature (num_joints, joint_data);
	int res = 0;
	if (!ptr_valid (arm)) {
		printf ("make_armature returned invalid pointer: %p\n", arm);
		return 1;
	}
	if (arm.num_joints != num_joints) {
		printf ("arm.num_joints: %d\n", arm.num_joints);
		res |= 1;
	}
	if (arm.num_edges != num_joints * JOINT_EDGES) {
		printf ("arm.num_edges: %d\n", arm.num_edges);
		res |= 1;
	}
	if (!ptr_valid (arm.joints)) {
		printf ("arm.joints: %p\n", arm.joints);
		res |= 1;
	}
	if (!ptr_valid (arm.basepose)) {
		printf ("arm.basepose: %p\n", arm.basepose);
		res |= 1;
	}
	if (!ptr_valid (arm.pose)) {
		printf ("arm.pose: %p\n", arm.pose);
		res |= 1;
	}
	if (!ptr_valid (arm.points)) {
		printf ("arm.points: %p\n", arm.points);
		res |= 1;
	}
	if (!ptr_valid (arm.edges)) {
		printf ("arm.edges: %p\n", arm.edges);
		res |= 1;
	}
	if (!ptr_valid (arm.edge_colors)) {
		printf ("arm.edge_colors: %p\n", arm.edge_colors);
		res |= 1;
	}
	if (!ptr_valid (arm.edge_bones)) {
		printf ("arm.edge_bones: %p\n", arm.edge_bones);
		res |= 1;
	}
	if (res) {
		return 1;
	}
	for (int i = 0; i < num_joints; i++) {
		auto e = joint_data[i];
		auto a = arm.joints[i];
		if (a.translate != e.translate || a.name != e.name
			|| a.rotate != e.rotate || a.scale != e.scale
			|| a.parent != e.parent) {
			printf ("%d %v %s %q %v %d\n", i, arm.joints[i].translate,
					arm.joints[i].name, arm.joints[i].rotate,
					arm.joints[i].scale,
					arm.joints[i].parent);
			res |= 1;
		}
	}
	if (res) {
		return 1;
	}

	for (int i = 0; i < num_joints; i++) {
		for (int j = 0; j < JOINT_EDGES; j++) {
			edge_t e = {
				.a = edges[j].a + i * JOINT_POINTS,
				.b = edges[j].b + i * JOINT_POINTS,
			};
			edge_t a = arm.edges[i * JOINT_EDGES + j];
			if (a.a != e.a || a.b != e.b) {
				printf ("edge %d.%d: a:%d,%d != e:%d,%d\n", i, j,
						a.a, a.b, e.a, e.b);
				res |= 1;
			}
			int ec = arm.edge_colors[i * JOINT_EDGES + j];
			if (ec != edge_colors[j]) {
				printf ("edge color %d.%d: a:%d != e:%d\n", i, j,
						ec, edge_colors[j]);
				res |= 1;
			}
			int eb = arm.edge_bones[i * JOINT_EDGES + j];
			if (eb != i) {
				printf ("edge bone %d.%d: a:%d != e:%d\n", i, j, eb, i);
				res |= 1;
			}
		}
		float best_dist = find_best_dist (arm, i);
		vec4 scale = { best_dist, best_dist, best_dist, 1 };
		printf ("scale: %g\n", best_dist);
		printf ("%d %q %q %q\n", i, arm.basepose[i].translate,
				arm.basepose[i].rotate, arm.basepose[i].scale);
		auto M = make_motor (arm.basepose[i].translate, arm.basepose[i].rotate);
		for (int j = 0; j < JOINT_POINTS; j++) {
			auto p = (point_t) (points[j] * scale);
			auto e = M * p * ~M;
			auto a = (point_t) arm.points[i * JOINT_POINTS + j];
			if (a != e) {
				printf ("bone point %d.%d: a:%.9q != e:%.9q\n", i, j, a, e);
				res |= 1;
			}
		}
	}

	return res;
}
