#include <math.h>
#include <scene.h>
#include <msgbuf.h>

#include <QF/qfmodel.h>

int ico_inds[] = {
	0,  6,  4,   0,  9,  6,   0,  2,  9,   0,  8,  2,   0,  4,  8,
	3, 10,  1,   3,  5, 10,   3,  7,  5,   3, 11,  7,   3,  1, 11,
	1,  6, 11,   1,  4,  6,   1, 10,  4,   9, 11,  6,   9,  7, 11,
	9,  2,  7,   5,  8, 10,   5,  2,  8,   5,  7,  2,   4, 10,  8,
};

model_t create_ico ()
{
	float p = (sqrt (5f) + 1) / 2;
	float a = sqrt (3f) / p;
	float b = a / p;
	vec3 verts[12];
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 4; j++) {
			float my = j & 1 ? a : -a;
			float mz = j & 2 ? b : -b;
			int   vind = i * 4 + j;
			int   ix = i;
			int   iy = (i + 1) % 3;
			int   iz = (i + 2) % 3;
			verts[vind][ix] = 0;
			verts[vind][iy] = my;
			verts[vind][iz] = mz;
		}
	}

	vec3 new_verts[60];
	vec3 normals[60];
	int new_inds[60];
	int num_verts = 0;
	for (int i = 0; i < 20; i++) {
		int base = num_verts;
		for (int j = 0; j < 3; j++) {
			new_inds[num_verts] = num_verts;
			new_verts[num_verts++] = verts[ico_inds[i * 3 + j]];
		}
		vec3 a = new_verts[base + 1] - new_verts[base];
		vec3 b = new_verts[base + 2] - new_verts[base];
		vec3 norm = -a × b;
		norm /= sqrt (norm • norm);
		for (int j = 0; j < 3; j++) {
			normals[base + j] = norm;
		}
	}

	qf_model_t model = {
		.meshes = {
			.offset = sizeof (qf_model_t) * 4,
			.count = 1,
		},
	};
	qf_mesh_t mesh = {
		.triangle_count = 20,
		.index_type = qfm_u32,
		.indices = (sizeof (qf_mesh_t)
					+ 2 * sizeof (qfm_attrdesc_t)
					+ 2 * sizeof (new_verts)) * 4,
		.attributes = {
			.offset = (sizeof (qf_mesh_t)) * 4,
			.count = 2,
		},
		.vertices = {
			.offset = (sizeof (qf_mesh_t)
						+ 2 * sizeof (qfm_attrdesc_t)) * 4,
			.count = num_verts,
		},
		.vertex_stride = 4 * 2 * sizeof (new_verts[0]),
		.scale = '1 1 1',
		.bounds_min = '-2 -2 -2',
		.bounds_max = ' 2  2  2',
	};

	qfm_attrdesc_t attributes[] = {
		{
			.offset = 0,
			.stride = 2 * sizeof (vec3) * 4,
			.attr = qfm_position,
			.type = qfm_f32,
			.components = 3,
		},
		{
			.offset = sizeof (vec3) * 4,
			.stride = 2 * sizeof (vec3) * 4,
			.attr = qfm_normal,
			.type = qfm_f32,
			.components = 3,
		},
	};

	uint size = sizeof (qf_model_t)
			  + sizeof (qf_mesh_t)
			  + sizeof (qfm_attrdesc_t) * 2
			  + sizeof (new_verts) * 2
			  + sizeof (new_inds);
	auto msg = MsgBuf_New (size * 4);//size is in ints, msgbuf wants bytes
	for (int i = 0; i < sizeof (qf_model_t); i++) {
		MsgBuf_WriteLong (msg, ((int *)&model)[i]);
	}
	for (int i = 0; i < sizeof (qf_mesh_t); i++) {
		MsgBuf_WriteLong (msg, ((int *)&mesh)[i]);
	}
	for (int i = 0; i < sizeof (attributes); i++) {
		MsgBuf_WriteLong (msg, ((int *)&attributes)[i]);
	}
	for (int i = 0; i < num_verts; i++) {
		for (int j = 0; j < 3; j++) {
			MsgBuf_WriteFloat (msg, new_verts[i][j]);
		}
		for (int j = 0; j < 3; j++) {
			MsgBuf_WriteFloat (msg, normals[i][j]);
		}
	}
	for (int i = 0; i < countof (new_inds); i++) {
		MsgBuf_WriteLong (msg, new_inds[i]);
	}
	return Model_LoadMesh ("ico", msg);

	//for (int i = 0; i < sizeof (verts); i++) {
	//	MsgBuf_WriteLong (msg, ((int *)&verts)[i]);
	//}
}

//int foo(qfm_attrdesc_t x)
//{
//	return x.set;
//}
