#include "GLSL/general.h"
#include "GLSL/image.h"

@generic(genFType = @vector(float)) {
genFType lerp(genFType x, genFType y, genFType a) = GLSL(FMix);
genFType lerp(genFType x, genFType y, float a)
    = GLSL(FMix) [x, y, @construct (genFType, a)];
};

[in("FragCoord")] vec4 gl_FragCoord;

[uniform, set(0), binding(0)] @image(uint, 2D, Storage, R32ui) cmd_heads;
[buffer, set(0), binding(1)] @block Commands {
	uint cmd_queue[];
};

float asfloat (uint x) = SPV(OpBitcast);
vec4 asrgba (uint x) = GLSL(UnpackUnorm4x8);

void
draw_line (uint ind, vec2 p, @inout vec4 color)
{
	auto a = vec2 (asfloat (cmd_queue[ind + 0]),
				   asfloat (cmd_queue[ind + 1]));
	auto b = vec2 (asfloat (cmd_queue[ind + 2]),
				   asfloat (cmd_queue[ind + 3]));
	auto r = asfloat (cmd_queue[ind + 4]);
	auto col = asrgba (cmd_queue[ind + 5]);
	float h = min (1, max (0, (p - a) • (b - a) / (b - a) • (b - a)));
	float d = length (p - a - h * (b - a)) - r;
	color = lerp (color, col, clamp (1 - d, 0, 1));
}

void
draw_circle (uint ind, vec2 p, @inout vec4 color)
{
	auto c = vec2 (asfloat (cmd_queue[ind + 0]),
				   asfloat (cmd_queue[ind + 1]));
	auto r = asfloat (cmd_queue[ind + 2]);
	auto col = asrgba (cmd_queue[ind + 3]);
	float d = length (p - c) - r;
	color = lerp (color, col, clamp (1 - d, 0, 1));
}

void
draw_box (uint ind, vec2 p, @inout vec4 color)
{
	auto c = vec2 (asfloat (cmd_queue[ind + 0]),
				   asfloat (cmd_queue[ind + 1]));
	auto e = vec2 (asfloat (cmd_queue[ind + 2]),
				   asfloat (cmd_queue[ind + 3]));
	auto r = asfloat (cmd_queue[ind + 4]);
	auto col = asrgba (cmd_queue[ind + 5]);
	auto q = abs (p - c) - e;
	float d = length (max (q, 0f)) + min (max (q.x, q.y), 0) - r;
	color = lerp (color, col, clamp (1 - d, 0, 1));
}

[out(0)] vec4 frag_color;

[shader("Fragment")]
void
main (void)
{
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
			draw_line (cmd_ind + 2, gl_FragCoord.xy, color);
			break;
		case 1:
			draw_circle (cmd_ind + 2, gl_FragCoord.xy, color);
			break;
		case 2:
			draw_box (cmd_ind + 2, gl_FragCoord.xy, color);
			break;
		}
		cmd_ind = next;
	}
	frag_color = color;
}
