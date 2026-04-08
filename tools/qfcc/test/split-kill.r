#include <QF/qfmodel.h>
#include "test-harness.h"

uint
fetch_name (qf_mesh_t *mesh)
{
	return mesh.name;
}

uint
foobar (bool foo)
{
	qf_mesh_t mesh_template = {
		.index_type = qfm_u32,
		.attributes = {
			.count = 1,
		},
		.vertex_stride = 3,
		.scale = '1 1 1',
		.bounds_min = '-1 -1 -1',
		.bounds_max = ' 1  1  1',
	};
	if (foo) {
		mesh_template.attributes.count++;
	}
	mesh_template.triangle_count = 1;
	return fetch_name (&mesh_template);
}

uint
setup ()
{
	qf_mesh_t mesh_template;
	for (int i = 0; i < sizeof(mesh_template)/sizeof(int); i++) {
		((int*)&mesh_template)[i] = 0xdeadbeef;
	}
	//mesh_template.triangle_count = 1;
	return fetch_name (&mesh_template);
}

int
main ()
{
	int ret = 0;
	uint s = setup ();
	if (s != 0xdeadbeef) {
		printf ("setup failed: %08x\n", s);
		ret = 1;
	}
	uint f = foobar (true);
	if (f != 0) {
		printf ("foobar failed: %08x\n", f);
		ret = 1;
	}
	return ret;
}
