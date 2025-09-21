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

#include "gizmo.h"

vec4
volumetric_color (const bool enable, const float depth,
				  const vec4 base_color, const vec4 mix_color)
{
	float factor = enable ? exp (-mix_color.a * depth) : 0;
	return mix (vec4(mix_color.rgb, 1), base_color, 1 - factor);
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
	color = volumetric_color (d > 0, 3 * dist / r, color, col);
}

//FIXME get generics with declarations working
@generic (genObj = [line_t, point_t]) {

vec2
icept (line_t ray, genObj obj, float r)
{
	@algebra (PGA) {
		auto rp = (e123 • ray * ~ray).tvec;
		auto rv = e0 * ~ray;

		float uu = ray • ~ray;
		float vv = obj • ~obj;

		auto a = (rp ∨ obj);
		auto b = (rv ∨ obj);
		float s = uu * uu * vv;
		float aa = a • ~a;
		float ab = a • ~b;
		float bb = b • ~b;
		float offs = sqrt (ab*ab - bb*(aa - s*r*r));
		auto ts = vec2 (-ab - offs, -ab + offs) / (bb * sqrt (uu));
		return ts;
	}
}

};

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

	@algebra (PGA) {
		// find the times to the cylinder sides
		vec2 ts = icept (ray, line, r);

		// find the times to the end-cap planes (through hemisphere center)
		vec2 te;
		{
			auto rp = (e123 • ray * ~ray).tvec;
			auto rv = e0 * ~ray;
			float uu = ray • ~ray;
			float vv = line • ~line;
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
				t = icept (ray, swap ? P2 : P1, r);
			} else if (ts[0] < te[0] && te[0] < ts[1] && ts[1] < te[1]) {
				// case 2
				// ray hits near infinite cylinder side outside the finite
				// cylinder (near end), but the far infinite cylinder side
				// inside
				// the finite cylinder
				t = icept (ray, swap ? P2 : P1, r);
				t[1] = ts[1];
			} else if (ts[0] < te[0] && te[1] < ts[1]) {
				// case 3
				// ray hits both ends of the finite cylinder inside the
				// infinite cylinder
				auto t1 = icept (ray, swap ? P2 : P1, r);
				auto t2 = icept (ray, swap ? P1 : P2, r);
				t = vec2 (t1[0], t1[1]);
			} else if (te[0] < ts[0] && ts[1] < te[1]) {
				// case 4
				// ray hits both infinite cylinder sides inside the finite
				// cylinder, so just the sides
				t = ts;
			} else if (te[0] < ts[0] && ts[0] < te[1] && te[1] < ts[1]) {
				// case 5
				// ray hits near infinite cylinder side inside the finite
				// cylinder, but the far infinite cylinder side outside
				// the finite cylinder
				t = icept (ray, swap ? P1 : P2, r);
				t[0] = ts[0];
			} else if (te[1] < ts[0]) {
				// case 6
				// ray hits both infinite cylinder sides outside the finite
				// cylinder (far end), so just the sphere
				t = icept (ray, swap ? P1 : P2, r);
			}
			// nan check on final points (may have missed the hemisphere ends)
			if (t[0] <= t[1]) {
				float dist = t[1] - t[0];
				color = volumetric_color (true, dist / r, color, col);
			}
		}
	}
}

typedef struct {
	point_t     backpt;
	int         back_num;
} trace_state_t;

typedef struct {
	plane_t     plane;
	uint        children;	// children[0] in 0:8, children[1] in 8:16
} trace_node_t;

#define get_node(ind) (trace_node_t) { \
	.plane = (plane_t) load_vec4 (node_base + ind * 5), \
	.children = objects[node_base + ind * 5 + 4] \
}
#define get_child(n, c) bitfieldExtract ((int) n.children, (c) * 8, 8)
#define SOLID -2
#define EMPTY -1

void
draw_brush (uint ind, vec3 v, vec3 eye, @inout vec4 color)
{
	auto orig = load_vec3 (ind + 0);
	auto mins = load_vec3 (ind + 3);
	auto maxs = load_vec3 (ind + 6);
	// bounding sphere radius for the brush (
	float r = length (maxs - mins) / 2;
	auto col = asrgba (objects[ind + 9]);
	uint node_base = ind + 10;
	int sp = 0;
	trace_state_t state_stack[16];
	int num = 0;

	@algebra(PGA) {
		auto frontpt = make_point (eye - orig, 1);	// start at the eye
		auto backpt =  make_point (v, 0);			// point at infinity
		auto ray = frontpt ∨ backpt;
		auto rv = e0 * ~ray;						// doesn't change
		bool empty = false;
		bool solid = false;
		auto start = frontpt;						// in case of start solid

		// just give up if the stack over/under-flows
		while (sp >= 0 && sp < countof (state_stack)) {
			while (num < 0) {
				if (solid) {
					if (num != SOLID) {
						solid = false;
						empty = true;
						auto d = start ∨ backpt;
						color = volumetric_color (true, sqrt(d • ~d), color, col);
					}
				} else if (empty) {
					if (num == SOLID) {
						solid = true;
						empty = false;
						start = backpt;
					}
				} else {
					//first leaf node
					if (num == SOLID) {
						solid = true;
						// starting solid. probably shouldn't happen
						auto d = start ∨ backpt;
						color = volumetric_color (true, sqrt (d • ~d),
												  color, vec4(1,0,1,1));
					} else {
						empty = true;
						start = backpt;
					}
				}
				// pop up the stack for a back side
				if (sp-- <= 0) {
					break;
				}
				frontpt = backpt;
				backpt = state_stack[sp].backpt;
				num = state_stack[sp].back_num;
			}
			//FIXME need a nicer way to break out of nested loops (eg, get
			//goto working for spir-v)
			if (sp < 0) {
				break;
			}
			auto node = get_node (num);
			// -_t.x/_t.y gives actual time, sign(_t.x) gives side
			auto front_t = vec2(node.plane ∨ frontpt, node.plane ∨ rv);
			auto back_t = vec2(node.plane ∨ backpt, node.plane ∨ rv);
			int front_side = (front_t.x) < 0 ? 1 : 0;
			int back_side =  (back_t.x)  < 0 ? 1 : 0;
			if (front_side == back_side) {
				num = get_child(node, front_side);
				continue;
			}

			state_stack[sp++] = {
				.backpt = backpt,
				.back_num = get_child (node, front_side ^ 1),
			};
			backpt = node.plane ∧ ray;
			if (((vec4)backpt).w < 0) {
				backpt = -backpt;
			}
			num = get_child (node, front_side);
		}
		if (solid) {
			//last leaf node was solid, probably shouldn't happen
			auto d = start ∨ backpt;
			color = volumetric_color (true, sqrt(d • ~d), color, vec4(1,0,1,1));
		}
	}
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
		case 2: draw_brush (obj_id + 1, v, eye, color); break;
		}
		queue_ind = next;
	}
	frag_color = color;
}
