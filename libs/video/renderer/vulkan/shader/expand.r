#include "general.h"

[uniform, set(0), binding(0)] @block
#include "matrices.h"
;

[buffer, readonly, set(1), binding(3)] @block Objects {
    uint objects[];
};

[push_constant] @block PushConstants {
	vec2 frag_size;
};

[in(0)] uint vert_id;
[out(0)] uint obj_id;

[out("Position")] vec4 gl_Position;
[in("VertexIndex")] int gl_VertexIndex;
[in("ViewIndex")] int gl_ViewIndex;

#include "gizmo.h"

const uint verts[] = {
	0x023310u,
	0x046620u,
	0x015540u,
	0x5199d5u,
	0x9133b9u,
	0x32aab3u,
	0xa266eau,
	0x64cce6u,
	0xc455dcu,
	0x9bffd9u,
	0xaeffbau,
	0xcdffecu,
};

void
transform_sphere (uint ind, @inout vec3 vert_pos, @inout vec3 vert_norm)
{
	auto c = vec3 (asfloat (objects[ind + 0]),
				   asfloat (objects[ind + 1]),
				   asfloat (objects[ind + 2]));
	auto r = asfloat (objects[ind + 3]);
	vert_pos = c + r * vert_pos;
}

void
transform_capsule (uint ind, uint vert, @inout vec3 vert_pos,
				   @inout vec3 vert_norm)
{
	auto p1 = vec3 (asfloat (objects[ind + 0]),
					asfloat (objects[ind + 1]),
					asfloat (objects[ind + 2]));
	auto p2 = vec3 (asfloat (objects[ind + 3]),
					asfloat (objects[ind + 4]),
					asfloat (objects[ind + 5]));
	auto r = asfloat (objects[ind + 6]);
	auto sign = p2 < p1;
	if (sign.z) {
		auto t = p1;
		p1 = p2;
		p2 = t;
		sign = !sign;
	}
	mat2 m = mat2 (sign.x == sign.y ? vec4 (1, 0, 0, 1) : vec4 (0, 1, -1, 0))
			* (sign.y ? -1 : 1);
	auto p = mix (p1, p2, bool(vert & 8));
	vert_pos = p + r * vec3 (m * vert_pos.xy, vert_pos.z);
	vert_norm = vec3(m * vert_norm.xy, vert_norm.z);
}

void
transform_brush (uint ind, uint vert, @inout vec3 vert_pos,
				 @inout vec3 vert_norm)
{
	auto orig = ((vec4)make_point (ind + 0, 1)).xyz;
	auto mins = ((vec4)make_point (ind + 3, 0)).xyz;
	auto maxs = ((vec4)make_point (ind + 6, 0)).xyz;
	bvec3 mask = vert_pos > vec3(0);
	auto r = mix (mins, maxs, mask);
	vert_pos = orig + r;
}

void
transform (uint obj_id, uint vert, @inout vec3 vert_pos, @inout vec3 vert_norm)
{
	uint cmd = objects[obj_id + 0];
	uint ind = obj_id + 1;
	switch (cmd) {
	case 0: transform_sphere (ind, vert_pos, vert_norm); break;
	case 1: transform_capsule (ind, vert, vert_pos, vert_norm); break;
	case 2: transform_brush (ind, vert, vert_pos, vert_norm); break;
	}
}

[shader("Vertex")]
[capability("MultiView")]
void
main ()
{
	int ind = gl_VertexIndex / 6;
	int nib = gl_VertexIndex % 6;
	uint vert = (verts[ind] >> (4 * nib)) & 0xf;
	auto vert_pos = vec3 (vert & 1, (vert & 2) >> 1, (vert & 4) >> 2) * 2 - 1;
	auto vert_norm = vert_pos;
	transform (vert_id, vert, vert_pos, vert_norm);
	vec4 pos = Projection3d * (View[gl_ViewIndex] * vec4 (vert_pos, 1));
	vec2 dir = (View[gl_ViewIndex] * vec4 (vert_norm, 0)).xy;
	vec2 ofs = mix (mix(vec2(0,0), vec2(1,1), dir > vec2(0,0)),
					vec2(-1,-1), dir < vec2(0,0));
	pos = vec4 (pos.xy + ofs * frag_size, pos.zw);
	gl_Position = pos;
	obj_id = vert_id;
}
