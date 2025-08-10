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

#define SPV(op) @intrinsic(op)
float asfloat (uint x) = SPV(OpBitcast);

void
transform (uint obj_id, @inout vec3 vert_pos, @inout vec3 vert_norm)
{
	uint cmd = objects[obj_id + 0];
	if (cmd != 0) {
		return;
	}
	uint ind = obj_id + 1;
	auto pos = vec3 (asfloat (objects[ind + 0]),
					 asfloat (objects[ind + 1]),
					 asfloat (objects[ind + 2]));
	auto rad = asfloat (objects[ind + 3]);
	vert_pos = pos + rad * vert_pos;
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
	transform (vert_id, vert_pos, vert_norm);
	vec4 pos = Projection3d * (View[gl_ViewIndex] * vec4 (vert_pos, 1));
	vec2 dir = (View[gl_ViewIndex] * vec4 (vert_norm, 0)).xy;
	vec2 ofs = mix (mix(vec2(0,0), vec2(1,1), dir > vec2(0,0)),
					vec2(-1,-1), dir < vec2(0,0));
	pos = vec4 (pos.xy + ofs * frag_size, pos.zw);
	gl_Position = pos;
	obj_id = vert_id;
}
