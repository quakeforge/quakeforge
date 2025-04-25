#include <string.h>
#include <math.h>
#include <imui.h>
#include <input.h>
#include <plist.h>
#include <cvar.h>
#include <draw.h>
#include <scene.h>
#include <qfs.h>

#include "pga3d.h"
#include "armature.h"

void printf (string fmt, ...);

void
dump_model (model_t model, string name)
{
	int num_joints = Model_NumJoints (model);
	qfm_joint_t *joints = obj_malloc (num_joints * sizeof (qfm_joint_t));
	for (int i = 0; i < num_joints; i++) {
		joints[i] = {};
	}
	printf ("%s clips: %d\n", name, Model_NumClips (model));
	printf ("bones: %d %d\n", num_joints, sizeof (qfm_joint_t));
	Model_GetJoints (model, joints);
	for (int i = 0; i < num_joints; i++) {
		//quaternion q = {joints[i].rotate[0], joints[i].rotate[1],
		//				joints[i].rotate[2], joints[i].rotate[3]};
		quaternion q = joints[i].rotate;
		printf ("%2d %2d %s\n    %v\n    %q\n    %v\n",
				i, joints[i].parent, joints[i].name,
				joints[i].translate, q, joints[i].scale);
	}
	obj_free (joints);
}

armature_t *
make_armature (model_t model)
{
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
	int num_joints = Model_NumJoints (model);
	armature_t *arm = obj_malloc (sizeof (armature_t));
#define JOINT_POINTS (6 + 3)
#define JOINT_EDGES (12 + 3)
	int num_points = num_joints * JOINT_POINTS;
	int num_edges = num_joints * JOINT_EDGES;
	*arm = {
		.num_joints  = num_joints,
		.joints      = obj_malloc (num_joints * sizeof (qfm_joint_t)),
		.pose        = obj_malloc (num_joints * sizeof (qfm_motor_t)),
		.points      = obj_malloc (num_points * sizeof (vec4)),
		.num_edges   = num_edges,
		.edges       = obj_malloc (num_edges * sizeof (edge_t)),
		.edge_colors = obj_malloc (num_edges * sizeof (int)),
		.edge_bones  = obj_malloc (num_edges * sizeof (int)),
	};
	Model_GetJoints (model, arm.joints);

	for (int i = 0; i < num_joints; i++) {
		arm.pose[i].m = make_motor (vec4(arm.joints[i].translate, 0),
									arm.joints[i].rotate);
		if (arm.joints[i].parent >= 0) {
			arm.pose[i].m = arm.pose[arm.joints[i].parent].m * arm.pose[i].m;
		}
		bool IK = str_str (arm.joints[i].name, "IK") >= 0;
		bool Null = str_str (arm.joints[i].name, "Null") >= 0;
		bool Goal = str_str (arm.joints[i].name, "Goal") >= 0;
		bool root = arm.joints[i].parent == -1;
		for (int j = 0; j < JOINT_EDGES; j++) {
			arm.edges[i * JOINT_EDGES + j] = {
				.a = edges[j].a + i * JOINT_POINTS,
				.b = edges[j].b + i * JOINT_POINTS,
			};
			int color = edge_colors[j];
			if (j < JOINT_EDGES - 3) {
				if (IK) {
					color = 14;
				} else if (Null) {
					color = 13;
				} else if (Goal) {
					color = 11;
				} else if (root) {
					color = 6;
				}
			}
			arm.edge_colors[i * JOINT_EDGES + j] = color;
			arm.edge_bones[i * JOINT_EDGES + j] = i;
		}
		// joints are always ordered such that the parent comes before any
		// children, so child joints will never have an index of 0
		int best_child = 0;
		float best_dist = 0;
		for (int j = i + 1; j < num_joints; j++) {
			if (arm.joints[j].parent == i) {
				if (arm.joints[j].translate.y > best_dist) {
					best_dist = arm.joints[j].translate.y;
					best_child = j;
				}
			}
		}
		if (!best_child) {
			best_dist = 0.05;
		}
		best_dist /= 4;
		vec4 scale = { best_dist, best_dist, best_dist, 1 };
		auto M = arm.pose[i].m;
		for (int j = 0; j < JOINT_POINTS; j++) {
			auto p = (point_t) (points[j] * scale);
			p = M * p * ~M;
			arm.points[i * JOINT_POINTS + j] = (vec4) p;
		}
	}
	return arm;
}

void
free_armature (armature_t *arm)
{
	if (!arm) {
		return;
	}
	obj_free (arm.joints);
	obj_free (arm.pose);
	obj_free (arm.points);
	obj_free (arm.edges);
	obj_free (arm.edge_colors);
	obj_free (arm.edge_bones);
	obj_free (arm);
}

vec4 do_scale (vec4 p, vec3 s)
{
	return p * vec4(s, 1);
}

void
draw_armature (transform_t camera, armature_t *arm, transform_t ent)
{
	@algebra (PGA) {
		auto E = make_motor (Transform_GetWorldPosition (ent),
							 Transform_GetWorldRotation (ent));
		for (int i = 0; i < arm.num_edges; i++) {
			auto pose = &arm.pose[arm.edge_bones[i]];
			auto M = E * pose.m;
			auto edge = arm.edges[i];
			auto p1 = (point_t) do_scale (arm.points[edge.a], pose.scale);
			auto p2 = (point_t) do_scale (arm.points[edge.b], pose.scale);
			p1 = M * p1 * ~M;
			p2 = M * p2 * ~M;
			int color = arm.edge_colors[i];
			if (pose.m.scalar < 0) {
				color = 254;
			}
			draw_3dline (camera, (vec4) p1, (vec4) p2, color);
		}
#if 0
		for (int i = 0; i < arm.num_joints; i++) {
			auto pose = &arm.pose[i];
			auto M = E * make_motor (pose.translate, pose.rotate);
			point_t p1 = e123;
			point_t p2 = 0.5f * e032 + e123;
			p1 = M * p1 * ~M;
			p2 = M * p2 * ~M;
			printf ("%-2d ", i);
		}
#endif
	}
}
