void printf (string fmt, ...) = #0;
typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;

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
	qfm_motor_t *pose;
	vec4       *points;
	int         num_edges;
	edge_t     *edges;
	int        *edge_colors;
	int        *edge_bones; // only one bone per edge
} armature_t;

void
do_joint (armature_t *arm, int i)
{
	arm.pose[i].m = arm.pose[arm.joints[i].parent].m * arm.pose[i].m;
}

qfm_motor_t pose[] = {
{.m={.bvect='0 0 0', .scalar=-1, .bvecp='0 0 0', .qvec=0}, .flags=~0u},
{.m={.bvect='0.790455461 0 0', .scalar=-0.612519562, .bvecp='0 0.358341873 0.247603387', .qvec=0}, .flags=0},
{.m={.bvect='-0.0644765124 1.18961232e-07 -7.68619834e-09', .scalar=-0.997919261, .bvecp='-5.03857622e-10 0.0654171705 0.00422666455', .qvec=-7.79833353e-09}, .flags=1},
};
qfm_motor_t out_pose[countof(pose)];
qfm_motor_t expect[] = {
{.m={.bvect='0 0 0', .scalar=-1, .bvecp='0 0 0', .qvec=0}, .flags=~0u},
{.m={.bvect='-0.790455461 0 0', .scalar=0.612519562, .bvecp='0 -0.358341873 -0.247603387', .qvec=0}, .flags=0},
{.m={.bvect='0.749317586 7.89416745e-08 8.93256029e-08', .scalar=-0.662210882, .bvecp='-3.86823515e-08 0.378359944 0.324491084', .qvec=-4.51040165e-08}, .flags=1},
};

void
set_stack ()
{
	//set the contents of the stack to something that won't cause a segfault
	//but will still reliably show the problem in do_joint when the indexing
	//breaks due to using an uninitialized value
	//XXX very UB :)
	int array[40];
	for (int i = 0; i < countof (array); i++) {
		array[i] = 3;
	}
}

int
main()
{
	for (int i = 0; i < countof(pose); i++) {
		out_pose[i] = pose[i];
	}
	armature_t arm = {
		.joints = (qfm_joint_t *) pose,
		.pose = out_pose,
	};
	for (int i = 1; i < countof(pose); i++) {
		set_stack ();
		do_joint (&arm, i);
	}
	bool fail = false;
	for (int i = 0; i < countof(pose); i++) {
		printf ("%d %.9v %.9g %.9v %.9g\n", i,
				out_pose[i].m.bvect, out_pose[i].m.scalar,
				out_pose[i].m.bvecp, out_pose[i].m.qvec);
		if (out_pose[i].m != expect[i].m) {
			fail = true;
		}
	}
	return fail & 1;
}
