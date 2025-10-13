#include "gizmo.h"
#include "pga3d.h"

typedef struct dnode_s {
	uint        planenum;
	int         children[2];
	vector      mins;
	vector      maxs;
	uint        firstface;
	uint        numfaces;
} dnode_t;

typedef struct dplane_s {
	vector  normal;
	float   dist;
	int     type;
} dplane_t;

static dnode_t nodes[] = {
	[0] = {
		.planenum = 0,
		.children = { -1, -2 },
		.mins = { -4032, -4032, -3968 },
		.maxs = { -3056, -3360, -3530.68872 },
		.firstface = 171508,
		.numfaces = 0,
	},
	[1] = {
		.planenum = 1,
		.children = { -1, 0 },
		.mins = { -4032, -4032, -3968 },
		.maxs = { -3056, -3360, -3504 },
		.firstface = 171464,
		.numfaces = 0,
	},
	[2] = {
		.planenum = 2,
		.children = { -1, 1 },
		.mins = { -4032, -4032, -3968 },
		.maxs = { -2848, -3360, -3504 },
		.firstface = 171290,
		.numfaces = 2,
	},
	[3] = {
		.planenum = 3,
		.children = { -1, 2 },
		.mins = { -4032, -4032, -3968 },
		.maxs = { -2848, -2752, -3504 },
		.firstface = 171078,
		.numfaces = 10,
	},
	[4] = {
		.planenum = 4,
		.children = { -1, 3 },
		.mins = { -4032, -4032, -3968 },
		.maxs = { -1664, -2752, -3504 },
		.firstface = 170165,
		.numfaces = 2,
	},
	[5] = {
		.planenum = 5,
		.children = { 4, -1 },
		.mins = { -4032, -4032, -4376 },
		.maxs = { -1664, -2752, -3504 },
		.firstface = 170165,
		.numfaces = 0,
	},
	[6] = {
		.planenum = 6,
		.children = { 5, -1 },
		.mins = { -4032, -4120, -4376 },
		.maxs = { -1664, -2752, -3504 },
		.firstface = 170165,
		.numfaces = 0,
	},
	[7] = {
		.planenum = 7,
		.children = { -1, 6 },
		.mins = { -4032, -4120, -4376 },
		.maxs = { -1664, -2304, -3504 },
		.firstface = 170165,
		.numfaces = 0,
	},
	[8] = {
		.planenum = 8,
		.children = { -1, 7 },
		.mins = { -4032, -4120, -4376 },
		.maxs = { -1664, -2304, -1792 },
		.firstface = 167214,
		.numfaces = 140,
	},
	[9] = {
		.planenum = 9,
		.children = { 8, -1 },
		.mins = { -4120, -4120, -4376 },
		.maxs = { -1664, -2304, -1792 },
		.firstface = 167214,
		.numfaces = 0,
	},
	[10] = {
		.planenum = 10,
		.children = { -1, 9 },
		.mins = { -4120, -4120, -4376 },
		.maxs = { -1664, -576, -1792 },
		.firstface = 165461,
		.numfaces = 1,
	},
	[11] = {
		.planenum = 11,
		.children = { -1, 10 },
		.mins = { -4120, -4120, -4376 },
		.maxs = { 0, -576, -1792 },
		.firstface = 159965,
		.numfaces = 65,
	},
	[12] = {
		.planenum = 12,
		.children = { -1, 11 },
		.mins = { -4120, -4120, -4376 },
		.maxs = { 0, -576, 0 },
		.firstface = 148174,
		.numfaces = 0,
	},
	[13] = {
		.planenum = 13,
		.children = { -1, 12 },
		.mins = { -4120, -4120, -4376 },
		.maxs = { 0, 4376, 0 },
		.firstface = 113040,
		.numfaces = 16,
	},
	[14] = {
		.planenum = 14,
		.children = { -1, 13 },
		.mins = { -4120, -4120, -4376 },
		.maxs = { 4120, 4376, 0 },
		.firstface = 77755,
		.numfaces = 28,
	},
	[15] = {
		.planenum = 15,
		.children = { -1, 14 },
		.mins = { -4120, -4120, -4376 },
		.maxs = { 4120, 4376, 4376 },
		.firstface = 0,
		.numfaces = 15,
	},
};
static dplane_t planes[] = {
	[0] = {
		.normal = { 0.36745742, 0.225567922, 0.902271688 },
		.dist = -5397.7749,
		.type = 5,
	},
	[1] = {
		.normal = { 0.239652231, 0.153739169, 0.958608925 },
		.dist = -4970.70361,
		.type = 5,
	},
	[2] = {
		.normal = { 1, 0, 0 },
		.dist = -3056,
		.type = 0,
	},
	[3] = {
		.normal = { 0, 1, 0 },
		.dist = -3360,
		.type = 1,
	},
	[4] = {
		.normal = { 1, 0, 0 },
		.dist = -2848,
		.type = 0,
	},
	[5] = {
		.normal = { 0, 0, 1 },
		.dist = -3968,
		.type = 2,
	},
	[6] = {
		.normal = { 0, 1, 0 },
		.dist = -4032,
		.type = 1,
	},
	[7] = {
		.normal = { 0, 1, 0 },
		.dist = -2752,
		.type = 1,
	},
	[8] = {
		.normal = { 0, 0, 1 },
		.dist = -3504,
		.type = 2,
	},
	[9] = {
		.normal = { 1, 0, 0 },
		.dist = -4032,
		.type = 0,
	},
	[10] = {
		.normal = { 0, 1, 0 },
		.dist = -2304,
		.type = 1,
	},
	[11] = {
		.normal = { 1, 0, 0 },
		.dist = -1664,
		.type = 0,
	},
	[12] = {
		.normal = { 0, 0, 1 },
		.dist = -1792,
		.type = 2,
	},
	[13] = {
		.normal = { 0, 1, 0 },
		.dist = -576,
		.type = 1,
	},
	[14] = {
		.normal = { 1, 0, 0 },
		.dist = 0,
		.type = 0,
	},
	[15] = {
		.normal = { 0, 0, 1 },
		.dist = 0,
		.type = 2,
	},
};

typedef bivector_t line_t;

void printf (string fmt, ...);

plane_t convert_plane (dplane_t *plane)
{
	return (plane_t) vec4(plane.normal, -plane.dist);
}

point_t convert_point (vector p)
{
	return (point_t) vec4(p, 1);
}

void
leafnode ()
{
	@algebra (PGA) {
		point_t point = e123;
		line_t  dir = nil;
		int inside = -2;
		for (int i = 0; i < countof (planes); i++) {
			auto plane = convert_plane (&planes[i]);
			float dist = plane ∨ point;
			if (inside == nodes[i].children[1]) {
				dist = -dist;
			}
			point_t P = (point • plane) * ~plane;
			if (dist < 0) {
				point = P;
			} else {
				line_t d = point ∨ P;
				if (d • ~dir > 0) {
					point = (point + P) / 2;
				}
			}
			dir = e123 ∨ point;
			printf ("%2d %2d %2d %q %q %g\n", inside,
				    nodes[i].children[0], nodes[i].children[1],
					plane, point, dist);
			Gizmo_AddSphere ((vec4)point / 128, 0.1, { 0, 1, 0, 0});
			inside = i;
		}
		printf ("%q\n", point);

		motor_t T =  ~e123 * point;
		T = sqrt (T);

		gizmo_node_t brush[countof(nodes)];
		const int top = countof(nodes) - 1;
		for (int i = 0; i < countof(nodes); i++) {
			int c0 = nodes[i].children[0];
			int c1 = nodes[i].children[1];
			auto p = convert_plane (&planes[nodes[i].planenum]);
			brush[top - i] = {
				.plane = (vec4) (T * p * ~T),
				.children = {
					c0 >= 0 ? top - c0 : c0,
					c1 >= 0 ? top - c1 : c1,
				},
			};
			brush[top -i].plane.w /= 128;
		}
		auto mins = (point_t) vec4(nodes[4].mins, 1);
		auto maxs = (point_t) vec4(nodes[4].maxs, 1);
		mins = T * mins * ~T;
		maxs = T * maxs * ~T;
		Gizmo_AddBrush ({0,0,0,1}, (vec4) mins / 128, (vec4) maxs / 128,
						top + 1, brush, { 0.8, 0.5, 0.2, 0.1 });
	}
}
