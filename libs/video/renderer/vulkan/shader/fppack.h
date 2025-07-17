#ifndef __fppack_h
#define __fppack_h

#define GLSL(op) @intrinsic(OpExtInst, "GLSL.std.450", op)
#define highp

highp uint packUnorm2x16(vec2 v) = GLSL(PackUnorm2x16);
highp uint packSnorm2x16(vec2 v) = GLSL(PackSnorm2x16);
uint packUnorm4x8(vec4 v) = GLSL(PackUnorm4x8);
uint packSnorm4x8(vec4 v) = GLSL(PackSnorm4x8);
vec2 unpackUnorm2x16(highp uint p) = GLSL(UnpackUnorm2x16);
vec2 unpackSnorm2x16(highp uint p) = GLSL(UnpackSnorm2x16);
vec4 unpackUnorm4x8(highp uint p) = GLSL(UnpackUnorm4x8);
vec4 unpackSnorm4x8(highp uint p) = GLSL(UnpackSnorm4x8);
uint packHalf2x16(vec2 v) = GLSL(PackHalf2x16);
vec2 unpackHalf2x16(uint v) = GLSL(UnpackHalf2x16);
double packDouble2x32(uvec2 v) = GLSL(PackDouble2x32);
uvec2 unpackDouble2x32(double v) = GLSL(UnpackDouble2x32);

#undef GLSL
#undef highp

#endif//__fppack_h
