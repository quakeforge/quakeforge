#include <math.h>
#include <scene.h>
#include <msgbuf.h>

#include <QF/qfmodel.h>

#include "armature.h"//for edge_t

void printf(string fmt, ...);

float sign (float x)
{
	return x < 0 ? -1 : 1;
}

typedef PGA.vec pga_vec_t;
body_t
calc_inertia_tensor (msgbuf_t model_buf)
{
	qf_model_t model;
	qf_mesh_t mesh;
	MsgBuf_ReadBytes (model_buf, &model, sizeof (model) * 4);
	int offset = model.meshes.offset + sizeof(mesh) * 0 * 4;
	MsgBuf_ReadSeek (model_buf, offset, msg_set);
	MsgBuf_ReadBytes (model_buf, &mesh, sizeof (mesh) * 4);

	int indices = mesh.indices + offset;
	int vertices = mesh.vertices.offset + offset;

	point_t C = nil;
	for (uint i = 0; i < mesh.triangle_count; i++) {
		uint inds[3];
		MsgBuf_ReadSeek (model_buf, indices + i * sizeof (inds) * 4, msg_set);
		MsgBuf_ReadBytes (model_buf, inds, sizeof (inds) * 4);
		vec4 v[3];
		for (int j = 0; j < 3; j++) {
			MsgBuf_ReadSeek (model_buf, vertices + inds[j] * mesh.vertex_stride,
							 msg_set);
			MsgBuf_ReadBytes (model_buf, &v[j], sizeof(vec3) * 4);
			v[j].w = 1;
		}

		@algebra (PGA) {
			auto p0 = (point_t)v[0];
			auto p1 = (point_t)v[1];
			auto p2 = (point_t)v[2];
			//auto vol = e123 ∨ p0 ∨ p1 ∨ p2;
			auto vol = p0 ∨ p1 ∨ p2 ∨ e123;
			C += (p0 + p1 + p2 + e123) * vol;
		}
	}
	C /= 24;//4!

	vec4 com;
	@algebra (PGA) {
		com = vec4(vec3(C / (e0 ∨ C)), 0);
	}

	mat3 I = {};
	for (uint i = 0; i < mesh.triangle_count; i++) {
		uint inds[3];
		MsgBuf_ReadSeek (model_buf, indices + i * sizeof (inds) * 4, msg_set);
		MsgBuf_ReadBytes (model_buf, inds, sizeof (inds) * 4);
		vec4 v[3];
		for (int j = 0; j < 3; j++) {
			MsgBuf_ReadSeek (model_buf, vertices + inds[j] * mesh.vertex_stride,
							 msg_set);
			MsgBuf_ReadBytes (model_buf, &v[j], sizeof(vec3) * 4);
			v[j].w = 1;
			v[j] -= com;
		}

		auto X = vec3(v[0].x, v[1].x, v[2].x);
		auto Y = vec3(v[0].y, v[1].y, v[2].y);
		auto Z = vec3(v[0].z, v[1].z, v[2].z);
		float Ix = X•(X + X.yzx);
		float Iy = Y•(Y + Y.yzx);
		float Iz = Z•(Z + Z.yzx);
		float Ixy = -X•Y - (X•Y.yzx + Y•X.yzx) / 2;
		float Ixz = -X•Z - (X•Z.yzx + Z•X.yzx) / 2;
		float Iyz = -Y•Z - (Y•Z.yzx + Z•Y.yzx) / 2;
		@algebra (PGA) {
			auto p0 = (point_t)v[0];
			auto p1 = (point_t)v[1];
			auto p2 = (point_t)v[2];
			//auto vol = e123 ∨ p0 ∨ p1 ∨ p2;
			auto vol = p0 ∨ p1 ∨ p2 ∨ e123;
			I[0] += vec3 (Iy + Iz, Ixy, Ixz) * vol;
			I[1] += vec3 (Ixy, Iz + Ix, Iyz) * vol;
			I[2] += vec3 (Ixz, Iyz, Ix + Iy) * vol;
		}
	}

	@algebra (PGA) {
		I /= 60 * (e0 ∨ C);// assumes mass of 1
	}
	printf ("C=%q\n", C);
	printf ("com=%q\n", com);
	printf ("  %v\n", I[0]);
	printf ("I=%v\n", I[1]);
	printf ("  %v\n", I[2]);

	motor_t R = { .scalar = 1, };
	@algebra (PGA) {
		for (int iter = 0; iter < 10; iter++) {
			bool changed = false;
			for (int i = 0; i < 3; i++) {
				static const int p_set[] = { 0, 2, 1 };
				static const int q_set[] = { 1, 0, 2 };
				static const PGA.bvect B_set[] = { e12, e31, e23 };
				static const PGA.vec basis[] = { e1, e2, e3 };

				int p = p_set[i];
				int q = q_set[i];
				auto B = B_set[i];

				float apq = PGA.vec()(I[p], 0) • basis[q];
				float app = PGA.vec()(I[p], 0) • basis[p];
				float aqq = PGA.vec()(I[q], 0) • basis[q];

				if (fabs(apq) < 1e-10) {
					continue;
				}

				vec2 tau = [aqq - app, 2 * apq];
				if (tau.x < 0) {
					tau = -tau;
				}
				float th = atan2 (tau.y, tau.x + sqrt(tau•tau));
				auto Rk = exp (-0.5 * th * B);
				PGA.vec rba[] = {
					~Rk * e1 * Rk,
					~Rk * e2 * Rk,
					~Rk * e3 * Rk,
				};
				mat3 a = nil;
				for (int j = 0; j < 3; j++) {
					//var rba = ~rotor >>> basis;
					//a = basis.map( (ej,j)=> (rotor >>> (
					//				(1e1|rba[j])*a[0]
					//			  + (1e2|rba[j])*a[1]
					//			  + (1e3|rba[j])*a[2] )).Grade(1))
					a[j] = (e1 • rba[j]) * I[0]
						 + (e2 • rba[j]) * I[1]
						 + (e3 • rba[j]) * I[2];
					auto ax = Rk * pga_vec_t(a[j], 0) * ~Rk;
					//a[j] = vec3(ax);
					a[j] = ((vec4)ax).xyz;
				}
				R = Rk * R;
				I = a;
				changed = true;
			}
			if (!changed) break;
		}
	}
	printf ("  %v\n", I[0]);
	printf ("I=%v\n", I[1]);
	printf ("  %v\n", I[2]);
	auto c = @dual(pga_vec_t(com.xyz, 1));
	@algebra (PGA) {
		R = R * (0.5 - 0.5 * e123 * c);
	}
	printf ("%g %v %v %g\n", R.scalar, R.bvect, R.bvecp, R.qvec);
	auto Ivec = vec3 (I[0][0], I[1][1], I[2][2]);
	printf ("%v\n", Ivec);

	MsgBuf_ReadSeek (model_buf, 0, msg_set);
	return {
		.R = R,
		.I.bvect = PGA.bvect()(1,1,1),
		.I.bvecp = (PGA.bvecp)Ivec,
		.Ii.bvect = PGA.bvect()(1,1,1),
		.Ii.bvecp = (PGA.bvecp)(1/Ivec),
	};
}

int ico_inds[] = {
	0,  6,  4,   0,  9,  6,   0,  2,  9,   0,  8,  2,   0,  4,  8,
	3, 10,  1,   3,  5, 10,   3,  7,  5,   3, 11,  7,   3,  1, 11,
	1,  6, 11,   1,  4,  6,   1, 10,  4,   9, 11,  6,   9,  7, 11,
	9,  2,  7,   5,  8, 10,   5,  2,  8,   5,  7,  2,   4, 10,  8,
};

int cube_inds[] = {
	0,  5,  1,   0,  4,  5,
	0,  3,  2,   0,  1,  3,
	0,  6,  4,   0,  2,  6,
	7,  4,  6,   7,  5,  4,
	7,  1,  5,   7,  3,  1,
	7,  2,  3,   7,  6,  2,
};

msgbuf_t create_ico ()
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
	MsgBuf_WriteBytes (msg, &model, sizeof (model) * 4);//FIXME 4
	MsgBuf_WriteBytes (msg, &mesh, sizeof (mesh) * 4);
	MsgBuf_WriteBytes (msg, &attributes, sizeof (attributes) * 4);
	for (int i = 0; i < num_verts; i++) {
		MsgBuf_WriteBytes (msg, &new_verts[i], sizeof (new_verts[i]) * 4);
		MsgBuf_WriteBytes (msg, &normals[i], sizeof (normals[i]) * 4);
	}
	MsgBuf_WriteBytes (msg, &new_inds, sizeof (new_inds) * 4);
	return msg;
}

quaternion exp(vector x)
{
	float l = x • x;
	if (l < 1e-8) {
		return '0 0 0 1';
	}
	float a = sqrt(l);
	auto sc = sincos (a);
	sc[0] /= a;
	return [x * sc[0], sc[1]];
}

msgbuf_t create_block ()
{
	vec3 verts[8];
	//quaternion q = exp('0.4 0.3 -0.2');//'20 15 0 60'f/65;
	//printf ("%q\n", q);
	for (int i = 0; i < 8; i++) {
		verts[i][0] = (((i & 1) >> 0) - 0.5f) * 9;
		verts[i][1] = (((i & 2) >> 1) - 0.5f) * 1;
		verts[i][2] = (((i & 4) >> 2) - 0.5f) * 4;
		//verts[i] = q * verts[i];
	}

	vec3 new_verts[36];
	vec3 normals[36];
	int new_inds[36];
	int num_verts = 0;
	for (int i = 0; i < 12; i++) {
		int base = num_verts;
		for (int j = 0; j < 3; j++) {
			new_inds[num_verts] = num_verts;
			new_verts[num_verts++] = verts[cube_inds[i * 3 + j]];
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
		.triangle_count = 12,
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
		.bounds_min = '-4.5 -0.5 -2',
		.bounds_max = ' 4.5  0.5  2',
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
	MsgBuf_WriteBytes (msg, &model, sizeof (model) * 4);//FIXME 4
	MsgBuf_WriteBytes (msg, &mesh, sizeof (mesh) * 4);
	MsgBuf_WriteBytes (msg, &attributes, sizeof (attributes) * 4);
	for (int i = 0; i < num_verts; i++) {
		MsgBuf_WriteBytes (msg, &new_verts[i], sizeof (new_verts[i]) * 4);
		MsgBuf_WriteBytes (msg, &normals[i], sizeof (normals[i]) * 4);
	}
	MsgBuf_WriteBytes (msg, &new_inds, sizeof (new_inds) * 4);
	return msg;
}

static int
find_edge (int v1, int v2, edge_t *edges, @inout int edge_count)
{
	for (int i = 0; i < edge_count; i++) {
		if ((edges[i].a == v1 && edges[i].b == v2)
			|| (edges[i].a == v2 && edges[i].b == v1)) {
			return i;
		}
	}
	edges[edge_count] = { v1, v2 };
	return edge_count++;
}

#define TWIN(h) he_in[h].twin
#define NEXT(h) he_in[h].next
#define PREV(h) he_in[h].prev
#define VERT(h) he_in[h].vert
#define FACE(h) he_in[h].face
#define EDGE(h) he_in[h].edge

@overload void
refine_halfedges (quarteredge_t *he_out, halfedge_t *he_in, int num_halfedges,
				  int Vd, int Fd, int Ed)
{
	for (int h = 0; h < num_halfedges; h++) {
		he_out[h * 4 + 0].twin = 4 * NEXT (TWIN(h)) + 3;
		he_out[h * 4 + 1].twin = 4 * NEXT (h) + 2;
		he_out[h * 4 + 2].twin = 4 * PREV (h) + 1;
		he_out[h * 4 + 3].twin = 4 * TWIN (PREV(h)) + 0;
		he_out[h * 4 + 0].vert = VERT(h),
		he_out[h * 4 + 1].vert = Vd + Fd + EDGE(h),
		he_out[h * 4 + 2].vert = Vd + FACE(h),
		he_out[h * 4 + 3].vert = Vd + Fd + EDGE(PREV(h)),
		he_out[h * 4 + 0].edge = 2 * EDGE(h) + ((h < TWIN(h)) & 1);
		he_out[h * 4 + 1].edge = 2 * Ed + h;
		he_out[h * 4 + 2].edge = 2 * Ed + PREV(h);
		he_out[h * 4 + 3].edge = 2 * EDGE(PREV(h)) + ((PREV(h) > TWIN(PREV(h))) & 1);
	}
}

@overload int
valence (halfedge_t *he_in, int h)
{
	int n = 1;
	int h1 = NEXT(TWIN(h));
	while (h1 != h) {
		n++;
		h1 = NEXT(TWIN(h1));
	}
	return n;
}

@overload int
cycle_length (halfedge_t *he_in, int h)
{
	int m = 1;
	int h1 = NEXT(h);
	while (h1 != h) {
		m++;
		h1 = NEXT(h1);
	}
	return m;
}

@overload void
face_points (vec3 *v_out, vec3 *v_in, halfedge_t *he_in, int hd, int vd)
{
	for (int h = 0; h < hd; h++) {
		int m = cycle_length (he_in, h);
		int v = VERT(h);
		int i = vd + FACE(h);
		v_out[i] += v_in[v] / m;
	}
}

@overload void
edge_points (vec3 *v_out, vec3 *v_in, halfedge_t *he_in,
			 int hd, int vd, int fd)
{
	for (int h = 0; h < hd; h++) {
		int v = VERT(h);
		int i = vd + FACE(h);
		int j = vd + fd + EDGE(h);
		if (TWIN(h) >= 0) {
			v_out[j] += (v_in[v] + v_out[i]) / 4;
		} else {
			v_out[j] += v_in[v] / 2;
		}
	}
}

@overload void
vert_points (vec3 *v_out, vec3 *v_in, halfedge_t *he_in, int hd, int vd, int fd)
{
	for (int h = 0; h < hd; h++) {
		int n = valence (he_in, h);
		int v = VERT(h);
		int i = vd + FACE(h);
		int j = vd + fd + EDGE(h);
		v_out[v] += (4 * v_out[j] - v_out[i] + (n - 3) * v_in[v]) / (n * n);
	}
}
#undef TWIN
#undef NEXT
#undef PREV
#undef VERT
#undef FACE
#undef EDGE

#define TWIN(h) he_in[h].twin
#define NEXT(h) (((h) & ~3) | (((h)+1) & 3))
#define PREV(h) (((h) & ~3) | (((h)+3) & 3))
#define VERT(h) he_in[h].vert
#define FACE(h) ((int)((uint)(h) >> 2))
#define EDGE(h) he_in[h].edge
@overload void
refine_halfedges (quarteredge_t *he_out, quarteredge_t *he_in,
				  int num_halfedges, int Vd, int Fd, int Ed)
{
	for (int h = 0; h < num_halfedges; h++) {
		he_out[h * 4 + 0].twin = 4 * NEXT (TWIN(h)) + 3;
		he_out[h * 4 + 1].twin = 4 * NEXT (h) + 2;
		he_out[h * 4 + 2].twin = 4 * PREV (h) + 1;
		he_out[h * 4 + 3].twin = 4 * TWIN (PREV(h)) + 0;
		he_out[h * 4 + 0].vert = VERT(h),
		he_out[h * 4 + 1].vert = Vd + Fd + EDGE(h),
		he_out[h * 4 + 2].vert = Vd + FACE(h),
		he_out[h * 4 + 3].vert = Vd + Fd + EDGE(PREV(h)),
		he_out[h * 4 + 0].edge = 2 * EDGE(h) + ((h < TWIN(h)) & 1);
		he_out[h * 4 + 1].edge = 2 * Ed + h;
		he_out[h * 4 + 2].edge = 2 * Ed + PREV(h);
		he_out[h * 4 + 3].edge = 2 * EDGE(PREV(h)) + ((PREV(h) > TWIN(PREV(h))) & 1);
	}
}

@overload int
valence (quarteredge_t *he_in, int h)
{
	int n = 1;
	int h1 = NEXT(TWIN(h));
	while (h1 != h) {
		n++;
		h1 = NEXT(TWIN(h1));
	}
	return n;
}

@overload int
cycle_length (quarteredge_t *he_in, int h)
{
	return 4;
}

@overload void
face_points (vec3 *v_out, vec3 *v_in, quarteredge_t *he_in, int hd, int vd)
{
	for (int h = 0; h < hd; h++) {
		int m = cycle_length (he_in, h);
		int v = VERT(h);
		int i = vd + FACE(h);
		v_out[i] += v_in[v] / m;
	}
}

@overload void
edge_points (vec3 *v_out, vec3 *v_in, quarteredge_t *he_in,
			 int hd, int vd, int fd)
{
	for (int h = 0; h < hd; h++) {
		int v = VERT(h);
		int i = vd + FACE(h);
		int j = vd + fd + EDGE(h);
		if (TWIN(h) >= 0) {
			v_out[j] += (v_in[v] + v_out[i]) / 4;
		} else {
			v_out[j] += v_in[v] / 2;
		}
	}
}

@overload void
vert_points (vec3 *v_out, vec3 *v_in, quarteredge_t *he_in,
			 int hd, int vd, int fd)
{
	for (int h = 0; h < hd; h++) {
		int n = valence (he_in, h);
		int v = VERT(h);
		int i = vd + FACE(h);
		int j = vd + fd + EDGE(h);
		v_out[v] += (4 * v_out[j] - v_out[i] + (n - 3) * v_in[v]) / (n * n);
	}
}
#undef TWIN
#undef NEXT
#undef PREV
#undef VERT
#undef FACE
#undef EDGE

int
calc_num_halfedges (int h0, uint d)
{
	return (1 << (2 * d)) * h0;
}

int
calc_num_edges (int h0, int e0, uint d)
{
	if (d < 1) {
		return e0;
	}
	return (1 << (d - 1)) * (2 * e0 + ((1 << d) - 1) * h0);
}

int
calc_num_faces (int h0, int f0, uint d)
{
	if (d < 1) {
		return f0;
	}
	return (1 << (2*(d - 1))) * h0;
}

int
calc_num_vertices (int h0, int v0, int f0, int e0, uint d)
{
	if (d < 1) {
		return v0;
	}
	int v1 = v0 + f0 + e0;
	if (d < 2) {
		return v1;
	}
	int f1 = calc_num_faces (h0, f0, 1);
	int e1 = calc_num_edges (h0, e0, 1);
	return ((1 << (d - 1)) - 1) * e1
		 + ((1 << d) * ((1 << (d - 2)) - 1) + 1) * f1 + v1;
}

int
calc_extra_halfedges (int h0, uint D)
{
	int h1 = 4 * h0;
	return ((1 << (2 * D)) - 1) * h1 / 3;
}

int
calc_extra_vertices (int h0, int v0, int f0, int e0, uint D)
{
	int f1 = calc_num_faces (h0, f0, 1);
	int e1 = calc_num_edges (h0, e0, 1);
	int v1 = calc_num_vertices (h0, v0, f0, e0, 1);
	return ((1 << D) - 1) * (e1 - 2 * f1)
		 + ((1 << (2 * D)) - 1) * f1 / 3
		 + D * (f1 - e1 + v1);
}

int
calc_extra_faces (int h0, uint D)
{
	return ((1 << (2 * D)) - 1) * h0 / 3;
}

msgbuf_t
create_quadsphere ()
{
	// start with a simple cube
	const int num_verts = 8;
	const int num_faces = 6;	// quad faces
	const int num_edges = 12;
	const int num_halfedges = 24;

	vec3 verts[num_verts];
	vec3 normals[num_verts];
	halfedge_t halfedges[num_halfedges];
	int edge_count = 0;
	edge_t edges[num_edges];

	for (int i = 0; i < num_verts; i++) {
		for (int j = 0; j < 3; j++) {
			verts[i][j] = i & (1 << j) ? 1 : -1;
		}
		normals[i] = verts[i] / sqrt (verts[i] • verts[i]);
	}
	static const int axes[] = { -1, -2, -3, 2, 1, 0 };//ones complement
	static const int face[][4] = {
#if 0
#define R
		{1, 3, 4, 2},
		{2, 5, 3, 0},
		{0, 4, 5, 1},
		{4, 0, 1, 5},
		{5, 2, 0, 3},
		{3, 1, 2, 4},
#else
#define R !
		{2, 4, 3, 1},
		{0, 3, 5, 2},
		{1, 5, 4, 0},
		{5, 1, 0, 4},
		{3, 0, 2, 5},
		{4, 2, 1, 3},
#endif
	};
	for (int i = 0; i < num_faces; i++) {
		bool neg = axes[i] < 0;
		int axis = neg ? ~axes[i] : axes[i];
		int flip = 7 ^ (1 << axis);
		int xor = 7 & (9 >> (2 - axis)) ^ ((R neg) & flip);//neg is 0 or -1
		int v = (!neg) & 7;
		for (int j = 0; j < 4; j++) {
			bool mid = bool ((j ^ (j>>1)) & 1);
			halfedges[i * 4 + j] = {
				.twin = face[i][j] * 4 + (mid ? j : 3 - j),
				.next = i * 4 + ((j + 1) & 3),
				.prev = i * 4 + ((j - 1) & 3),
				.vert = v,
				.edge = find_edge (v ^ xor, v, edges, edge_count),
				.face = i,
			};
			v ^= xor;
			xor ^= flip;
		}
	}

	qf_model_t model = {
		.meshes = {
			.offset = sizeof (qf_model_t) * 4,
			.count = 6,
		},
	};
	qf_mesh_t mesh_template = {
		.index_type = qfm_u32,
		.attributes = {
			.count = 2,
		},
		.vertex_stride = 4 * 2 * sizeof (verts[0]),
		.scale = '1 1 1',
		.bounds_min = '-1 -1 -1',
		.bounds_max = ' 1  1  1',
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

	int max_halfedges = calc_num_halfedges (num_halfedges, 5);
	int max_vertices = calc_num_vertices (num_halfedges, num_verts,
										  num_faces, num_edges, 5);
	int extra_halfedges = calc_extra_halfedges (num_halfedges, 5);
	int extra_verts = calc_extra_vertices (num_halfedges, num_verts,
										   num_faces, num_edges, 5);
	int extra_faces = calc_extra_faces (num_halfedges, 5);
	uint size = sizeof (qf_model_t)
			  + sizeof (qf_mesh_t) * 6
			  + sizeof (qfm_attrdesc_t) * 2// need only one copy
			  + sizeof (halfedge_t) * num_halfedges
			  + sizeof (quarteredge_t) * extra_halfedges
			  + sizeof (vec3) * 2 * (num_verts + extra_verts)
			  + sizeof (uint) * 6 * (num_faces + extra_faces);
	quarteredge_t *subdiv_halfedges[2] = {
		obj_malloc (sizeof (quarteredge_t) * max_halfedges),
		obj_malloc (sizeof (quarteredge_t) * max_halfedges),
	};
	vec3 *subdiv_verts[2] = {
		obj_malloc (sizeof (quarteredge_t) * max_halfedges),
		obj_malloc (sizeof (quarteredge_t) * max_halfedges),
	};
	auto msg = MsgBuf_New (size * 4);//size is in ints, msgbuf wants bytes
	MsgBuf_WriteBytes (msg, &model, sizeof (model) * 4);
	int offset = MsgBuf_WriteSeek (msg, 0, msg_cur);
	int mesh_offsets[] = {
		offset + sizeof (qf_mesh_t) * 0 * 4,
		offset + sizeof (qf_mesh_t) * 1 * 4,
		offset + sizeof (qf_mesh_t) * 2 * 4,
		offset + sizeof (qf_mesh_t) * 3 * 4,
		offset + sizeof (qf_mesh_t) * 4 * 4,
		offset + sizeof (qf_mesh_t) * 5 * 4,
	};
	printf ("offset: %d\n", offset);
	for (int i = 0; i < 6; i++) {
		printf ("  %d:offset: %d\n", i, mesh_offsets[i]);
	}
	MsgBuf_WriteSeek (msg, sizeof (qf_mesh_t) * 6 * 4, msg_cur);

	int attributes_offset = MsgBuf_WriteSeek (msg, 0, msg_cur);
	printf ("attributes_offset: %d\n", attributes_offset);
	mesh_template.attributes.offset = attributes_offset;
	mesh_template.attributes.offset -= mesh_offsets[0];
	MsgBuf_WriteBytes (msg, attributes, sizeof (attributes) * 4);

	mesh_template.adjacency.offset = MsgBuf_WriteSeek (msg, 0, msg_cur);
	mesh_template.adjacency.offset -= mesh_offsets[0];
	mesh_template.adjacency.count = num_halfedges;
	MsgBuf_WriteBytes (msg, halfedges, sizeof (halfedges) * 4);

	mesh_template.vertices.offset = MsgBuf_WriteSeek (msg, 0, msg_cur);
	mesh_template.vertices.offset -= mesh_offsets[0];
	mesh_template.vertices.count = num_verts;
	for (int i = 0; i < num_verts; i++) {
		MsgBuf_WriteBytes (msg, &verts[i], sizeof (verts[i]) * 4);
		MsgBuf_WriteBytes (msg, &normals[i], sizeof (normals[i]) * 4);
	}

	mesh_template.indices = MsgBuf_WriteSeek (msg, 0, msg_cur);
	mesh_template.indices -= mesh_offsets[0];
	mesh_template.triangle_count = num_faces * 2;
	for (int i = 0; i < num_faces; i++) {
		uint indices[6];
		// the half-edges are always contiguous for a face: the initial
		// set-up of the cube ensured that for subdivision level 0, and
		// the subdivision algorithm ensures it for all subsequent
		// subdivisions
		for (int j = 0; j < 4; j++) {
			indices[j] = halfedges[i * 4 + j].vert;
		}
		indices[4] = indices[0];
		indices[5] = indices[2];
		MsgBuf_WriteBytes (msg, indices, sizeof (indices) * 4);
	}

	offset = MsgBuf_WriteSeek (msg, 0, msg_cur);
	MsgBuf_WriteSeek (msg, mesh_offsets[0], msg_set);
	MsgBuf_WriteBytes (msg, &mesh_template, sizeof (mesh_template) * 4);
	MsgBuf_WriteSeek (msg, offset, msg_set);

	for (int d = 1; d < 6; d++) {
		int hd = calc_num_halfedges (num_halfedges, d - 1);
		int vd = calc_num_vertices (num_halfedges, num_verts,
									num_faces, num_edges, d - 1);
		int fd = calc_num_faces (num_halfedges, num_faces, d - 1);
		int ed = calc_num_edges (num_halfedges, num_edges, d - 1);
		int hd_1 = calc_num_halfedges (num_halfedges, d);
		int vd_1 = calc_num_vertices (num_halfedges, num_verts,
									  num_faces, num_edges, d);
		int fd_1 = calc_num_faces (num_halfedges, num_faces, d);
		int ind = d & 1;

		for (int i = 0; i < vd_1; i++) {
			subdiv_verts[ind][i] = '0 0 0';
		}
		if (d > 1) {
			refine_halfedges (subdiv_halfedges[ind],
							  subdiv_halfedges[ind^1], hd, vd, fd, ed);
			face_points (subdiv_verts[ind], subdiv_verts[ind^1],
						 subdiv_halfedges[ind^1], hd, vd);
			edge_points (subdiv_verts[ind], subdiv_verts[ind^1],
						 subdiv_halfedges[ind^1], hd, vd, fd);
			vert_points (subdiv_verts[ind], subdiv_verts[ind^1],
						 subdiv_halfedges[ind^1], hd, vd, fd);
		} else {
			refine_halfedges (subdiv_halfedges[ind],
							  halfedges, hd, vd, fd, ed);
			face_points (subdiv_verts[ind], verts, halfedges, hd, vd);
			edge_points (subdiv_verts[ind], verts, halfedges, hd, vd, fd);
			vert_points (subdiv_verts[ind], verts, halfedges, hd, vd, fd);
		}
		for (int i = 0; i < vd_1; i++) {
			vec3 v = subdiv_verts[ind][i];
			subdiv_verts[ind][i] /= sqrt (v • v);
		}

		mesh_template.attributes.offset = attributes_offset;
		mesh_template.attributes.offset -= mesh_offsets[d];

		mesh_template.adjacency.offset = MsgBuf_WriteSeek (msg, 0, msg_cur);
		mesh_template.adjacency.offset -= mesh_offsets[d];
		mesh_template.adjacency.count = hd_1;
		MsgBuf_WriteBytes (msg, subdiv_halfedges[ind],
						   sizeof (quarteredge_t) * hd_1 * 4);

		mesh_template.vertices.offset = MsgBuf_WriteSeek (msg, 0, msg_cur);
		mesh_template.vertices.offset -= mesh_offsets[d];
		mesh_template.vertices.count = vd_1;
		for (int i = 0; i < vd_1; i++) {
			MsgBuf_WriteBytes (msg, &subdiv_verts[ind][i],
							   sizeof (subdiv_verts[ind][i]) * 4);
			// normals (verts are normalized)
			MsgBuf_WriteBytes (msg, &subdiv_verts[ind][i],
							   sizeof (subdiv_verts[ind][i]) * 4);
		}

		mesh_template.indices = MsgBuf_WriteSeek (msg, 0, msg_cur);
		mesh_template.indices -= mesh_offsets[d];
		mesh_template.triangle_count = fd_1 * 2;
		for (int i = 0; i < fd_1; i++) {
			uint indices[6];
			// the half-edges are always contiguous for a face: the initial
			// set-up of the cube ensured that for subdivision level 0, and
			// the subdivision algorithm ensures it for all subsequent
			// subdivisions
			for (int j = 0; j < 4; j++) {
				indices[j] = subdiv_halfedges[ind][i * 4 + j].vert;
			}
			indices[4] = indices[0];
			indices[5] = indices[2];
			MsgBuf_WriteBytes (msg, indices, sizeof (indices) * 4);
		}

		offset = MsgBuf_WriteSeek (msg, 0, msg_cur);
		MsgBuf_WriteSeek (msg, mesh_offsets[d], msg_set);
		MsgBuf_WriteBytes (msg, &mesh_template, sizeof (mesh_template) * 4);
		MsgBuf_WriteSeek (msg, offset, msg_set);
	}

	//QFile f = Qopen ("foo.qfm", "wb");
	//MsgBuf_ToFile (f, msg);
	//Qclose (f);

	return msg;
}
