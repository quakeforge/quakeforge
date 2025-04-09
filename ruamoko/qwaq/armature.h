#ifndef __armature_h
#define __armature_h

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

typedef struct qfm_joint_s {
	vector translate;
	string name;
	quaternion rotate;
	vector scale;
	int parent;
} qfm_joint_t;

typedef struct qfm_motor_s {
	motor_t m;
	vec3 scale;
	uint flags;
} qfm_motor_t;

typedef struct edge_s {
	int a;
	int b;
} edge_t;

typedef struct {
	int         num_joints;
	qfm_joint_t *joints;
	qfm_motor_t *basepose;
	qfm_motor_t *pose;
	vec4       *points;
	int         num_edges;
	edge_t     *edges;
	int        *edge_colors;
	int        *edge_bones;	// only one bone per edge
} armature_t;

armature_t *make_armature (model_t model);
void draw_armature (transform_t camera, armature_t *arm, transform_t ent);

#endif//__armature_h
