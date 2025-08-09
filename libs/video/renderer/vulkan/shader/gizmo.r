//·×÷†•∗∧∨⋀⋆‖⊥△▽ඞ
//e𝅘e𝅗 ⁰ⁱ²³⁴⁵⁶⁷⁸⁹ ₀₁₂₃₄₅₆₇₈₉

#include "image.h"
#include "general.h"
#include "matrix.h"

[uniform, set(0), binding(0)] @block
#include "matrices.h"
;

#include "queueobj.h"

[buffer, set(1), binding(3)] @block Objects {
    uint objects[];
};

[in(0)] vec2 uv;
[out(0)] vec4 frag_color;
[in("FragCoord")] vec4 gl_FragCoord;
[in("ViewIndex"), flat] int gl_ViewIndex;

#define SPV(op) @intrinsic(op)
#define GLSL(op) @intrinsic(OpExtInst,"GLSL.std.450",op)

float asfloat (uint x) = SPV(OpBitcast);
vec4 asrgba (uint x) = GLSL(UnpackUnorm4x8);

void
draw_sphere (uint ind, vec3 v, vec3 eye, @inout vec4 color)
{
	auto c = vec3 (asfloat (objects[ind + 0]),
				   asfloat (objects[ind + 1]),
				   asfloat (objects[ind + 2])) - eye;
	if (c • v < 0) {
		return;
	}
	auto r = asfloat (objects[ind + 3]);
	auto col = asrgba (objects[ind + 4]);
	float d = r * r - c • c + (c • v) * (c • v) / (v • v);
	float dist = d > 0 ? sqrt (d) : 0;
	float factor = d > 0 ? exp (-col.a * 3 * dist / r) : 0;
	color = mix (vec4(col.rgb, 1), color, 1-factor);
}

[shader("Fragment")]
[capability("MultiView")]
void main()
{
	// asumes non-shearing camera
	auto view = View[gl_ViewIndex];
	auto p = transpose (mat3 (view[0].xyz, view[1].xyz, view[2].xyz));
	auto cam = mat3 (p[0] / Projection3d[0][0],
					 p[1] / Projection3d[1][1],
					 p[2]);
	auto eye = -(p * view[3].xyz);

	vec2 UV = 2 * uv - vec2(1,1);
	auto v = cam * vec3 (UV, 1);

	// use 8x8 grid for screen grid
	auto queue_coord = ivec3 (gl_FragCoord.xy, gl_ViewIndex * 8) / 8;
	uint queue_ind = imageLoad (queue_heads, queue_coord).r;

	auto color = vec4 (0, 0, 0, 0);
	while (queue_ind != ~0u) {
		//FIXME might be a good time to look into BDA
		uint obj_id = queue[queue_ind].obj_id;
		uint next = queue[queue_ind].next;
		uint cmd = objects[obj_id + 0];
		switch (cmd) {
		case 0:
			draw_sphere (obj_id + 1, v, eye, color);
			break;
		}
		queue_ind = next;
	}
	frag_color = color;
}
