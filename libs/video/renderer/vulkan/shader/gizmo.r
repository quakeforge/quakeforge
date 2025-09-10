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

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x0a) bivec_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;
typedef bivec_t line_t;

#define SPV(op) @intrinsic(op)
#define GLSL(op) @intrinsic(OpExtInst,"GLSL.std.450",op)

float asfloat (uint x) = SPV(OpBitcast);
vec4 asrgba (uint x) = GLSL(UnpackUnorm4x8);

@overload point_t
make_point (const uint ind, const float weight)
{
	return (point_t) vec4 (asfloat (objects[ind + 0]),
						   asfloat (objects[ind + 1]),
						   asfloat (objects[ind + 2]), weight);
}

@overload point_t
make_point (const vec3 vec, const float weight)
{
	return (point_t) vec4 (vec, weight);
}

void
draw_sphere (uint ind, vec3 v, vec3 eye, @inout vec4 color)
{
	auto c = vec3 (asfloat (objects[ind + 0]),
				   asfloat (objects[ind + 1]),
				   asfloat (objects[ind + 2])) - eye;
	//if (c • v < 0) {
	//	return;
	//}
	auto r = asfloat (objects[ind + 3]);
	auto col = asrgba (objects[ind + 4]);
	float d = r * r - c • c + (c • v) * (c • v) / (v • v);
	//if (d < 0) {
	//	color = col;
	//	return;
	//}
	float dist = d > 0 ? sqrt (d) : 0;
	float factor = d > 0 ? exp (-col.a * 3 * dist / r) : 0;
	color = mix (vec4(col.rgb, 1), color, 1-factor);
}

void
draw_capsule (uint ind, vec3 v, vec3 eye, @inout vec4 color)
{
	auto p1 = vec3 (asfloat (objects[ind + 0]),
					asfloat (objects[ind + 1]),
					asfloat (objects[ind + 2])) - eye;
	auto p2 = vec3 (asfloat (objects[ind + 3]),
					asfloat (objects[ind + 4]),
					asfloat (objects[ind + 5])) - eye;
	auto r = asfloat (objects[ind + 6]);
	auto col = asrgba (objects[ind + 7]);

//·×÷†•∗∧∨⋀⋆‖⊥△▽ඞ
	auto line = make_point (ind + 0, 1) ∨ make_point (ind + 3, 1);
	auto ray = make_point (eye, 1) ∨ make_point (v, 0);
	auto M = line * ray;
	// find squared sin(th) of the angle between the line and ray
	// not interested in the actual angle, only whether it's close enough to
	// 0 so that the correct components for calculating distance can be chosen
	float s2 = M.bvect • ~M.bvect / ((line • line) * (ray • ray));
	float d2 = s2 < 1e-6 ? ⋆M.bvecp • ~⋆M.bvecp : ⋆M.qvec • ⋆M.qvec;
	float w2 = s2 < 1e-6 ? M.scalar • M.scalar  : M.bvect • ~M.bvect;
	float disc = w2 * r * r - d2;
	float dist = disc > 0 ? sqrt (disc / w2) : 0;
	float factor = disc > 0 ? exp (-col.a * 3 * dist / r) : 0;
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
		case 0: draw_sphere (obj_id + 1, v, eye, color); break;
		case 1: draw_capsule (obj_id + 1, v, eye, color); break;
		}
		queue_ind = next;
	}
	frag_color = color;
}
