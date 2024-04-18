#include "test-harness.h"

typedef struct {
	vector translate;
	string name;
	quaternion rotate;
	vector scale;
	int parent;
} iqmjoint_t;

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
	if (joint_data[0].translate != '0 1 0'
		|| joint_data[0].name != "root"
		|| joint_data[0].rotate != '0.6 0 0 0.8'
		|| joint_data[0].scale != '1 1 1'
		|| joint_data[0].parent != -1) {
		printf ("joint_data[0] bad\n");
		return 1;
	}
	if (joint_data[1].translate != '0 2 0'
		|| joint_data[1].name != "flip"
		|| joint_data[1].rotate != '0.6 0 0 0.8'
		|| joint_data[1].scale != '1 1 1'
		|| joint_data[1].parent != 0) {
		printf ("joint_data[1] bad\n");
		return 1;
	}
	if (joint_data[2].translate != '0 3 0'
		|| joint_data[2].name != "flop"
		|| joint_data[2].rotate != '0.6 0 0 0.8'
		|| joint_data[2].scale != '1 1 1'
		|| joint_data[2].parent != 1) {
		printf ("joint_data[2] bad\n");
		return 1;
	}
	return 0;
}
