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
	auto r = asfloat (objects[ind + 6]);
	auto col = asrgba (objects[ind + 7]);

//·×÷†•∗∧∨⋀⋆‖⊥△▽ඞ
	auto P1 = make_point (ind + 0, 1);
	auto P2 = make_point (ind + 3, 1);
	auto line = P1 ∨ P2;
	auto ray = make_point (eye, 1) ∨ make_point (v, 0);
	float uu = ray • ~ray;
	float vv = line • ~line;

	@algebra (PGA) {
		// find the times to the cylinder sides
		vec2 ts;
		auto rp = (e123 • ray * ~ray).tvec;
		auto rv = e0 * ~ray;
		{
			auto a = (rp ∨ line);
			auto b = (rv ∨ line);
			float s = uu * uu * vv;
			float aa = a • a;
			float ab = a • b;
			float bb = b • b;
			ts[0] = (-ab - sqrt (ab*ab - bb*(aa - s*r*r))) / (bb * sqrt (uu));
			ts[1] = (-ab + sqrt (ab*ab - bb*(aa - s*r*r))) / (bb * sqrt (uu));
		}

		// find the times to the end-cap planes (through hemisphere center)
		vec2 te;
		{
			auto p1 = P1 • line;
			auto p2 = P2 • line;
			auto a = vec2(rp ∨ p1, rp ∨ p2);
			auto b = vec2(rv ∨ p1, rv ∨ p2);
			te = -a / (b * sqrt (uu));
		}
		// nan check on sides (if either value is nan, the comparison will fail
		// but otherwise ts[0] is initialized to be <= ts[1])
		if (ts[0] <= ts[1]) {
			auto t = sqrt(vec2(-1, -1));
			bool swap = te[0] > te[1];
			if (swap) {
				te = te.yx;
			}
			if (ts[1] < te[0]) {
				// case 1
				// ray hits both infinite cylinder sides outside the finite
				// cylinder (near end), so just the sphere
				auto P = swap ? P2 : P1;
				auto a = (rp ∨ P);
				auto b = (rv ∨ P);
				float s = uu * uu * r * r;
				float aa = a • ~a;
				float ab = a • ~b;
				float bb = b • ~b;
				t[0] = (-ab - sqrt (ab*ab - bb*(aa - s))) / (bb * sqrt (uu));
				t[1] = (-ab + sqrt (ab*ab - bb*(aa - s))) / (bb * sqrt (uu));
				col = vec4(1,0,0,1);
			} else
			if (ts[0] < te[0] && te[0] < ts[1] && ts[1] < te[1]) {
				// case 2
				// ray hits near infinite cylinder side outside the finite
				// cylinder (near end), but the far infinite cylinder side
				// inside
				// the finite cylinder
				auto P = swap ? P2 : P1;
				auto a = (rp ∨ P);
				auto b = (rv ∨ P);
				float s = uu * uu * r * r;
				float aa = a • ~a;
				float ab = a • ~b;
				float bb = b • ~b;
				t[0] = (-ab - sqrt (ab*ab - bb*(aa - s))) / (bb * sqrt (uu));
				t[1] = ts[1];
				col = vec4(1,1,0,1);
			} else
			if (ts[0] < te[0] && te[1] < ts[1]) {
				// case 3
				// ray hits both ends of the finite cylinder inside the
				// infinite cylinder
				auto a1 = (rp ∨ (swap ? P2 : P1));
				auto b1 = (rv ∨ (swap ? P2 : P1));
				auto a2 = (rp ∨ (swap ? P1 : P2));
				auto b2 = (rv ∨ (swap ? P1 : P2));
				float s = uu * uu * r * r;
				float aa1 = a1 • ~a1;
				float ab1 = a1 • ~b1;
				float bb1 = b1 • ~b1;
				float aa2 = a2 • ~a2;
				float ab2 = a2 • ~b2;
				float bb2 = b2 • ~b2;
				t[0] = (-ab1 - sqrt (ab1*ab1 - bb1*(aa1 - s))) / bb1;
				t[1] = (-ab2 - sqrt (ab2*ab2 - bb2*(aa2 - s))) / bb2;
				col = vec4(0,1,0,1);
			} else
			if (te[0] < ts[0] && ts[1] < te[1]) {
				// case 4
				// ray hits both infinite cylinder sides inside the finite
				// cylinder, so just the sides
				t = ts;
				col = vec4(0,1,1,1);
			} else
			if (te[0] < ts[0] && ts[0] < te[1] && te[1] < ts[1]) {
				// case 5
				// ray hits near infinite cylinder side inside the finite
				// cylinder, but the far infinite cylinder side outside
				// the finite cylinder
				auto P = swap ? P1 : P2;
				auto a = (rp ∨ P);
				auto b = (rv ∨ P);
				float s = uu * uu * r * r;
				float aa = a • ~a;
				float ab = a • ~b;
				float bb = b • ~b;
				t[0] = ts[0];
				t[1] = (-ab + sqrt (ab*ab - bb*(aa - s))) / (bb * sqrt (uu));
				col = vec4(1,1,1,1);
			} else
			if (te[1] < ts[0]) {
				// case 6
				// ray hits both infinite cylinder sides outside the finite
				// cylinder (far end), so just the sphere
				auto P = swap ? P1 : P2;
				auto a = (rp ∨ P);
				auto b = (rv ∨ P);
				float s = uu * uu * r * r;
				float aa = a • ~a;
				float ab = a • ~b;
				float bb = b • ~b;
				t[0] = (-ab - sqrt (ab*ab - bb*(aa - s))) / (bb * sqrt (uu));
				t[1] = (-ab + sqrt (ab*ab - bb*(aa - s))) / (bb * sqrt (uu));
				col = vec4(1,0,1,1);
			}
			// nan check on final points (may have missed the hemisphere ends)
			if (t[0] <= t[1]) {
				float dist = t[1] - t[0];
				float factor = exp (-col.a * dist / r);
				color = mix (vec4(col.rgb, 1), color, 1-factor);
			}
		}
	}

#if 0
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
#endif
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
