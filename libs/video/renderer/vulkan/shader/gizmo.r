//·×÷†•∗∧∨⋀⋆‖⊥△▽ඞ
//e𝅘e𝅗 ⁰ⁱ²³⁴⁵⁶⁷⁸⁹ ₀₁₂₃₄₅₆₇₈₉

#include "image.h"
#include "general.h"
#include "matrix.h"

[uniform, set(0), binding(0)] @block
#include "matrices.h"
;

[uniform, set(1), binding(0)] @image(uint, 2D, Storage, R32ui) cmd_heads;
[buffer, set(1), binding(1)] @block Commands {
    uint cmd_queue[];
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
	auto c = vec3 (asfloat (cmd_queue[ind + 0]),
				   asfloat (cmd_queue[ind + 1]),
				   asfloat (cmd_queue[ind + 2])) - eye;
	if (c • v < 0) {
		return;
	}
	auto r = asfloat (cmd_queue[ind + 3]);
	auto col = asrgba (cmd_queue[ind + 4]);
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
	auto cmd_coord = ivec2 (gl_FragCoord.xy) / 8;
	uint cmd_ind = imageLoad (cmd_heads, cmd_coord).r;

	auto color = vec4 (0, 0, 0, 0);
	while (cmd_ind != ~0u) {
		//FIXME might be a good time to look into BDA
		uint cmd = cmd_queue[cmd_ind + 0];
		uint next = cmd_queue[cmd_ind + 1];
		switch (cmd) {
		case 0:
			draw_sphere (cmd_ind + 2, v, eye, color);
			break;
		}
		cmd_ind = next;
	}
	frag_color = color;
}
