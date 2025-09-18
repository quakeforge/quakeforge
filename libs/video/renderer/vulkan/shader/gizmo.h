//·×÷†•∗∧∨⋀⋆‖⊥△▽ඞ
//e𝅘e𝅗 ⁰ⁱ²³⁴⁵⁶⁷⁸⁹ ₀₁₂₃₄₅₆₇₈₉
#ifndef __gizmo_h
#define __gizmo_h

#include "integer.h"

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

#undef SPV
#undef GLSL

ivec4 getsbytes (uint ind)
{
	int x = objects[ind];
	return ivec4 (bitfieldExtract (x,  0, 8),
				  bitfieldExtract (x,  8, 8),
				  bitfieldExtract (x, 16, 8),
				  bitfieldExtract (x, 24, 8));
}

vec3
load_vec3 (const uint ind)
{
	return vec3 (asfloat (objects[ind + 0]),
				 asfloat (objects[ind + 1]),
				 asfloat (objects[ind + 2]));
}

@overload point_t
make_point (const uint ind, const float weight)
{
	return (point_t) vec4 (load_vec3 (ind), weight);
}

@overload point_t
make_point (const vec3 vec, const float weight)
{
	return (point_t) vec4 (vec, weight);
}

#endif
