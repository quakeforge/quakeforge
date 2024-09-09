/*
	glsl-builtins.c

	Builtin symbols for GLSL

	Copyright (C) 2024 Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/rua-lang.h"

#define SRC_LINE_EXP2(l,f) "#line " #l " " #f "\n"
#define SRC_LINE_EXP(l,f) SRC_LINE_EXP2(l,f)
#define SRC_LINE SRC_LINE_EXP (__LINE__ + 1, __FILE__)

static const char *glsl_Vulkan_vertex_vars =
SRC_LINE
"in int gl_VertexIndex;"        "\n"
"in int gl_InstanceIndex;"      "\n"
"in int gl_DrawID;"             "\n"
"in int gl_BaseVertex;"         "\n"
"in int gl_BaseInstance;"       "\n"
"#ifdef GL_EXT_multiview"       "\n"
"highp int gl_ViewIndex;"       "\n"
"#endif"                        "\n"
"out gl_PerVertex {"            "\n"
"    vec4 gl_Position;"         "\n"
"    float gl_PointSize;"       "\n"
"    float gl_ClipDistance[];"  "\n"
"    float gl_CullDistance[];"  "\n"
"};"                            "\n";

//tesselation control
static const char *glsl_tesselation_control_vars =
SRC_LINE
"in gl_PerVertex {"                     "\n"
"    vec4 gl_Position;"                 "\n"
"    float gl_PointSize;"               "\n"
"    float gl_ClipDistance[];"          "\n"
"    float gl_CullDistance[];"          "\n"
"} gl_in[gl_MaxPatchVertices];"         "\n"
"in int gl_PatchVerticesIn;"            "\n"
"in int gl_PrimitiveID;"                "\n"
"in int gl_InvocationID;"               "\n"
"#ifdef GL_EXT_multiview"               "\n"
"highp int gl_ViewIndex;"               "\n"
"#endif"                                "\n"
"out gl_PerVertex {"                    "\n"
"    vec4 gl_Position;"                 "\n"
"    float gl_PointSize;"               "\n"
"    float gl_ClipDistance[];"          "\n"
"    float gl_CullDistance[];"          "\n"
"} gl_out[];"                           "\n"
"patch out float gl_TessLevelOuter[4];" "\n"
"patch out float gl_TessLevelInner[2];" "\n";

static const char *glsl_tesselation_evaluation_vars =
SRC_LINE
"in gl_PerVertex {"                     "\n"
"    vec4 gl_Position;"                 "\n"
"    float gl_PointSize;"               "\n"
"    float gl_ClipDistance[];"          "\n"
"    float gl_CullDistance[];"          "\n"
"} gl_in[gl_MaxPatchVertices];"         "\n"
"in int gl_PatchVerticesIn;"            "\n"
"in int gl_PrimitiveID;"                "\n"
"in vec3 gl_TessCoord;"                 "\n"
"patch in float gl_TessLevelOuter[4];"  "\n"
"patch in float gl_TessLevelInner[2];"  "\n"
"#ifdef GL_EXT_multiview"               "\n"
"highp int gl_ViewIndex;"               "\n"
"#endif"                                "\n"
"out gl_PerVertex {"                    "\n"
"    vec4 gl_Position;"                 "\n"
"    float gl_PointSize;"               "\n"
"    float gl_ClipDistance[];"          "\n"
"    float gl_CullDistance[];"          "\n"
"};"                                    "\n";

static const char *glsl_geometry_vars =
SRC_LINE
"in gl_PerVertex {"             "\n"
"    vec4 gl_Position;"         "\n"
"    float gl_PointSize;"       "\n"
"    float gl_ClipDistance[];"  "\n"
"    float gl_CullDistance[];"  "\n"
"} gl_in[];"                    "\n"
"in int gl_PrimitiveIDIn;"      "\n"
"in int gl_InvocationID;"       "\n"
"#ifdef GL_EXT_multiview"       "\n"
"highp int gl_ViewIndex;"       "\n"
"#endif"                        "\n"
"out gl_PerVertex {"            "\n"
"    vec4 gl_Position;"         "\n"
"    float gl_PointSize;"       "\n"
"    float gl_ClipDistance[];"  "\n"
"    float gl_CullDistance[];"  "\n"
"};"                            "\n"
"out int gl_PrimitiveID;"       "\n"
"out int gl_Layer;"             "\n"
"out int gl_ViewportIndex;"     "\n";

static const char *glsl_fragment_vars =
SRC_LINE
"in vec4 gl_FragCoord;"         "\n"
"in bool gl_FrontFacing;"       "\n"
"in float gl_ClipDistance[];"   "\n"
"in float gl_CullDistance[];"   "\n"
"in vec2 gl_PointCoord;"        "\n"
"in int gl_PrimitiveID;"        "\n"
"in int gl_SampleID;"           "\n"
"in vec2 gl_SamplePosition;"    "\n"
"in int gl_SampleMaskIn[];"     "\n"
"in int gl_Layer;"              "\n"
"in int gl_ViewportIndex;"      "\n"
"in bool gl_HelperInvocation;"  "\n"
"#ifdef GL_EXT_multiview"       "\n"
"highp int gl_ViewIndex;"       "\n"
"#endif"                        "\n"
"out float gl_FragDepth;"       "\n"
"out int gl_SampleMask[];"      "\n";

static const char *glsl_compute_vars =
SRC_LINE
"// workgroup dimensions"           "\n"
"in uvec3 gl_NumWorkGroups;"        "\n"
"const uvec3 gl_WorkGroupSize;"     "\n"
"// workgroup and invocation IDs"   "\n"
"in uvec3 gl_WorkGroupID;"          "\n"
"in uvec3 gl_LocalInvocationID;"    "\n"
"// derived variables"              "\n"
"in uvec3 gl_GlobalInvocationID;"   "\n"
"in uint gl_LocalInvocationIndex;"  "\n";

static const char *glsl_system_constants =
SRC_LINE
"//"                                                                    "\n"
"// Implementation-dependent constants. The example values below"       "\n"
"// are the minimum values allowed for these maximums."                 "\n"
"//"                                                                    "\n"
"const int gl_MaxVertexAttribs = 16;"                                   "\n"
"const int gl_MaxVertexUniformVectors = 256;"                           "\n"
"const int gl_MaxVertexUniformComponents = 1024;"                       "\n"
"const int gl_MaxVertexOutputComponents = 64;"                          "\n"
"const int gl_MaxVaryingComponents = 60;"                               "\n"
"const int gl_MaxVaryingVectors = 15;"                                  "\n"
"const int gl_MaxVertexTextureImageUnits = 16;"                         "\n"
"const int gl_MaxVertexImageUniforms = 0;"                              "\n"
"const int gl_MaxVertexAtomicCounters = 0;"                             "\n"
"const int gl_MaxVertexAtomicCounterBuffers = 0;"                       "\n"
""                                                                      "\n"
"const int gl_MaxTessPatchComponents = 120;"                            "\n"
"const int gl_MaxPatchVertices = 32;"                                   "\n"
"const int gl_MaxTessGenLevel = 64;"                                    "\n"
""                                                                      "\n"
"const int gl_MaxTessControlInputComponents = 128;"                     "\n"
"const int gl_MaxTessControlOutputComponents = 128;"                    "\n"
"const int gl_MaxTessControlTextureImageUnits = 16;"                    "\n"
"const int gl_MaxTessControlUniformComponents = 1024;"                  "\n"
"const int gl_MaxTessControlTotalOutputComponents = 4096;"              "\n"
"const int gl_MaxTessControlImageUniforms = 0;"                         "\n"
"const int gl_MaxTessControlAtomicCounters = 0;"                        "\n"
"const int gl_MaxTessControlAtomicCounterBuffers = 0;"                  "\n"
""                                                                      "\n"
"const int gl_MaxTessEvaluationInputComponents = 128;"                  "\n"
"const int gl_MaxTessEvaluationOutputComponents = 128;"                 "\n"
"const int gl_MaxTessEvaluationTextureImageUnits = 16;"                 "\n"
"const int gl_MaxTessEvaluationUniformComponents = 1024;"               "\n"
"const int gl_MaxTessEvaluationImageUniforms = 0;"                      "\n"
"const int gl_MaxTessEvaluationAtomicCounters = 0;"                     "\n"
"const int gl_MaxTessEvaluationAtomicCounterBuffers = 0;"               "\n"
""                                                                      "\n"
"const int gl_MaxGeometryInputComponents = 64;"                         "\n"
"const int gl_MaxGeometryOutputComponents = 128;"                       "\n"
"const int gl_MaxGeometryImageUniforms = 0;"                            "\n"
"const int gl_MaxGeometryTextureImageUnits = 16;"                       "\n"
"const int gl_MaxGeometryOutputVertices = 256;"                         "\n"
"const int gl_MaxGeometryTotalOutputComponents = 1024;"                 "\n"
"const int gl_MaxGeometryUniformComponents = 1024;"                     "\n"
"const int gl_MaxGeometryVaryingComponents = 64;         // deprecated" "\n"
"const int gl_MaxGeometryAtomicCounters = 0;"                           "\n"
"const int gl_MaxGeometryAtomicCounterBuffers = 0;"                     "\n"
""                                                                      "\n"
"const int gl_MaxFragmentImageUniforms = 8;"                            "\n"
"const int gl_MaxFragmentInputComponents = 128;"                        "\n"
"const int gl_MaxFragmentUniformVectors = 256;"                         "\n"
"const int gl_MaxFragmentUniformComponents = 1024;"                     "\n"
"const int gl_MaxFragmentAtomicCounters = 8;"                           "\n"
"const int gl_MaxFragmentAtomicCounterBuffers = 1;"                     "\n"
""                                                                      "\n"
"const int gl_MaxDrawBuffers = 8;"                                      "\n"
"const int gl_MaxTextureImageUnits = 16;"                               "\n"
"const int gl_MinProgramTexelOffset = -8;"                              "\n"
"const int gl_MaxProgramTexelOffset = 7;"                               "\n"
"const int gl_MaxImageUnits = 8;"                                       "\n"
"const int gl_MaxSamples = 4;"                                          "\n"
"const int gl_MaxImageSamples = 0;"                                     "\n"
"const int gl_MaxClipDistances = 8;"                                    "\n"
"const int gl_MaxCullDistances = 8;"                                    "\n"
"const int gl_MaxViewports = 16;"                                       "\n"
""                                                                      "\n"
"const int gl_MaxComputeImageUniforms = 8;"                             "\n"
"const ivec3 gl_MaxComputeWorkGroupCount = { 65535, 65535, 65535 };"    "\n"
"const ivec3 gl_MaxComputeWorkGroupSize = { 1024, 1024, 64 };"          "\n"
"const int gl_MaxComputeUniformComponents = 1024;"                      "\n"
"const int gl_MaxComputeTextureImageUnits = 16;"                        "\n"
"const int gl_MaxComputeAtomicCounters = 8;"                            "\n"
"const int gl_MaxComputeAtomicCounterBuffers = 8;"                      "\n"
""                                                                      "\n"
"const int gl_MaxCombinedTextureImageUnits = 96;"                       "\n"
"const int gl_MaxCombinedImageUniforms = 48;"                           "\n"
"const int gl_MaxCombinedImageUnitsAndFragmentOutputs = 8;  // deprecated\n"
"const int gl_MaxCombinedShaderOutputResources = 16;"                   "\n"
"const int gl_MaxCombinedAtomicCounters = 8;"                           "\n"
"const int gl_MaxCombinedAtomicCounterBuffers = 1;"                     "\n"
"const int gl_MaxCombinedClipAndCullDistances = 8;"                     "\n"
"const int gl_MaxAtomicCounterBindings = 1;"                            "\n"
"const int gl_MaxAtomicCounterBufferSize = 32;"                         "\n"
""                                                                      "\n"
"const int gl_MaxTransformFeedbackBuffers = 4;"                         "\n"
"const int gl_MaxTransformFeedbackInterleavedComponents = 64;"          "\n"
""                                                                      "\n"
"const highp int gl_MaxInputAttachments = 1;"                           "\n";
#if 0
static const type_t *Ftypes[] = {
	&type_float,
	&type_vec2,
	&type_vec3,
	&type_vec4,
	nullptr
};

static const type_t *Itypes[] = {
	&type_int,
	&type_ivec2,
	&type_ivec3,
	&type_ivec4,
	nullptr
};

static const type_t *Utypes[] = {
	&type_uint,
	&type_uivec2,
	&type_uivec3,
	&type_uivec4,
	nullptr
};

static const type_t *Btypes[] = {
	&type_bool,
	&type_bvec2,
	&type_bvec3,
	&type_bvec4,
	nullptr
};

static const type_t *Dtypes[] = {
	&type_double,
	&type_dvec2,
	&type_dvec3,
	&type_dvec4,
	nullptr
};

static gentype_t genFType = {
	.name = "genFType",
	.valid_types = Ftypes,
};

static gentype_t genIType = {
	.name = "genIType",
	.valid_types = Itypes,
};

static gentype_t genUType = {
	.name = "genUType",
	.valid_types = Utypes,
};

static gentype_t genBType = {
	.name = "genBType",
	.valid_types = Btypes,
};

static gentype_t genDType = {
	.name = "genDType",
	.valid_types = Dtypes,
};
#endif

static const char *glsl_general_functions =
SRC_LINE
"#define out @out"                                                  "\n"
"#define highp"                                                     "\n"
"#define lowp"                                                      "\n"
"#define uint unsigned"                                             "\n"
"#define uvec2 uivec2"                                              "\n"
"@generic(genFType=@vector(float),"                                 "\n"
"         genDType=@vector(double),"                                "\n"
"         genIType=@vector(int),"                                   "\n"
"         genUType=@vector(unsigned int),"                          "\n"
"         genBType=@vector(bool),"                                  "\n"
"         mat=@matrix(float),"                                      "\n"
"         vec=[vec2,vec3,vec4,dvec2,dvec3,dvec4],"                  "\n"
"         ivec=[ivec2,ivec3,ivec4],"                                "\n"
"         uvec=[uivec2,uivec3,uivec4],"                             "\n"
"         bvec=[bvec2,bvec3,bvec4]) {"                              "\n"
"genFType radians(genFType degrees);"                               "\n"
"genFType degrees(genFType radians);"                               "\n"
"genFType sin(genFType angle);"                                     "\n"
"genFType cos(genFType angle);"                                     "\n"
"genFType tan(genFType angle);"                                     "\n"
"genFType asin(genFType x);"                                        "\n"
"genFType acos(genFType x);"                                        "\n"
"genFType atan(genFType y, genFType x);"                            "\n"
"genFType atan(genFType y_over_x);"                                 "\n"
"genFType sinh(genFType x);"                                        "\n"
"genFType cosh(genFType x);"                                        "\n"
"genFType tanh(genFType x);"                                        "\n"
"genFType asinh(genFType x);"                                       "\n"
"genFType acosh(genFType x);"                                       "\n"
"genFType atanh(genFType x);"                                       "\n"

//exponential functions
SRC_LINE
"genFType pow(genFType x, genFType y);"                             "\n"
"genFType exp(genFType x);"                                         "\n"
"genFType log(genFType x);"                                         "\n"
"genFType exp2(genFType x);"                                        "\n"
"genFType log2(genFType x);"                                        "\n"
"genFType sqrt(genFType x);"                                        "\n"
"genDType sqrt(genDType x);"                                        "\n"
"genFType inversesqrt(genFType x);"                                 "\n"
"genDType inversesqrt(genDType x);"                                 "\n"

//common functions
SRC_LINE
"genFType abs(genFType x);"                                         "\n"
"genIType abs(genIType x);"                                         "\n"
"genDType abs(genDType x);"                                         "\n"
"genFType sign(genFType x);"                                        "\n"
"genIType sign(genIType x);"                                        "\n"
"genDType sign(genDType x);"                                        "\n"
"genFType floor(genFType x);"                                       "\n"
"genDType floor(genDType x);"                                       "\n"
"genFType trunc(genFType x);"                                       "\n"
"genDType trunc(genDType x);"                                       "\n"
"genFType round(genFType x);"                                       "\n"
"genDType round(genDType x);"                                       "\n"
"genFType roundEven(genFType x);"                                   "\n"
"genDType roundEven(genDType x);"                                   "\n"
"genFType ceil(genFType x);"                                        "\n"
"genDType ceil(genDType x);"                                        "\n"
"genFType fract(genFType x);"                                       "\n"
"genDType fract(genDType x);"                                       "\n"
"genFType mod(genFType x, float y);"                                "\n"
"genFType mod(genFType x, genFType y);"                             "\n"
"genDType mod(genDType x, double y);"                               "\n"
"genDType mod(genDType x, genDType y);"                             "\n"
"genFType modf(genFType x, out genFType i);"                        "\n"
"genDType modf(genDType x, out genDType i);"                        "\n"
"genFType min(genFType x, genFType y);"                             "\n"
"genFType min(genFType x, float y);"                                "\n"
"genDType min(genDType x, genDType y);"                             "\n"
"genDType min(genDType x, double y);"                               "\n"
"genIType min(genIType x, genIType y);"                             "\n"
"genIType min(genIType x, int y);"                                  "\n"
"genUType min(genUType x, genUType y);"                             "\n"
"genUType min(genUType x, uint y);"                                 "\n"
"genFType max(genFType x, genFType y);"                             "\n"
"genFType max(genFType x, float y);"                                "\n"
"genDType max(genDType x, genDType y);"                             "\n"
"genDType max(genDType x, double y);"                               "\n"
"genIType max(genIType x, genIType y);"                             "\n"
"genIType max(genIType x, int y);"                                  "\n"
"genUType max(genUType x, genUType y);"                             "\n"
"genUType max(genUType x, uint y);"                                 "\n"
"genFType clamp(genFType x, genFType minVal, genFType maxVal);"     "\n"
"genFType clamp(genFType x, float minVal, float maxVal);"           "\n"
"genDType clamp(genDType x, genDType minVal, genDType maxVal);"     "\n"
"genDType clamp(genDType x, double minVal, double maxVal);"         "\n"
"genIType clamp(genIType x, genIType minVal, genIType maxVal);"     "\n"
"genIType clamp(genIType x, int minVal, int maxVal);"               "\n"
"genUType clamp(genUType x, genUType minVal, genUType maxVal);"     "\n"
"genUType clamp(genUType x, uint minVal, uint maxVal);"             "\n"
"genFType mix(genFType x, genFType y, genFType a);"                 "\n"
"genFType mix(genFType x, genFType y, float a);"                    "\n"
"genDType mix(genDType x, genDType y, genDType a);"                 "\n"
"genDType mix(genDType x, genDType y, double a);"                   "\n"
"genFType mix(genFType x, genFType y, genBType a);"                 "\n"
"genDType mix(genDType x, genDType y, genBType a);"                 "\n"
"genIType mix(genIType x, genIType y, genBType a);"                 "\n"
"genUType mix(genUType x, genUType y, genBType a);"                 "\n"
"genBType mix(genBType x, genBType y, genBType a);"                 "\n"
"genFType step(genFType edge, genFType x);"                         "\n"
"genFType step(float edge, genFType x);"                            "\n"
"genDType step(genDType edge, genDType x);"                         "\n"
"genDType step(double edge, genDType x);"                           "\n"
"genFType smoothstep(genFType edge0, genFType edge1, genFType x);"  "\n"
"genFType smoothstep(float edge0, float edge1, genFType x);"        "\n"
"genDType smoothstep(genDType edge0, genDType edge1, genDType x);"  "\n"
"genDType smoothstep(double edge0, double edge1, genDType x);"      "\n"
"genBType isnan(genFType x);"                                       "\n"
"genBType isnan(genDType x);"                                       "\n"
"genBType isinf(genFType x);"                                       "\n"
"genBType isinf(genDType x);"                                       "\n"
"genIType floatBitsToInt(highp genFType value);"                    "\n"
"genUType floatBitsToUint(highp genFType value);"                   "\n"
"genFType intBitsToFloat(highp genIType value);"                    "\n"
"genFType uintBitsToFloat(highp genUType value);"                   "\n"
"genFType fma(genFType a, genFType b, genFType c);"                 "\n"
"genDType fma(genDType a, genDType b, genDType c);"                 "\n"
"genFType frexp(highp genFType x, out highp genIType exp);"         "\n"
"genDType frexp(genDType x, out genIType exp);"                     "\n"
"genFType ldexp(highp genFType x, highp genIType exp);"             "\n"
"genDType ldexp(genDType x, genIType exp);"                         "\n"

//floating-point pack and unpack functions
SRC_LINE
"highp uint packUnorm2x16(vec2 v);"                                 "\n"
"highp uint packSnorm2x16(vec2 v);"                                 "\n"
"uint packUnorm4x8(vec4 v);"                                        "\n"
"uint packSnorm4x8(vec4 v);"                                        "\n"
"vec2 unpackUnorm2x16(highp uint p);"                               "\n"
"vec2 unpackSnorm2x16(highp uint p);"                               "\n"
"vec4 unpackUnorm4x8(highp uint p);"                                "\n"
"vec4 unpackSnorm4x8(highp uint p);"                                "\n"
"uint packHalf2x16(vec2 v);"                                        "\n"
"vec2 unpackHalf2x16(uint v);"                                      "\n"
"double packDouble2x32(uvec2 v);"                                   "\n"
"uvec2 unpackDouble2x32(double v);"                                 "\n"

//geometric functions
SRC_LINE
"float length(genFType x);"                                         "\n"
"double length(genDType x);"                                        "\n"
"float distance(genFType p0, genFType p1);"                         "\n"
"double distance(genDType p0, genDType p1);"                        "\n"
"float dot(genFType x, genFType y);"                                "\n"
"double dot(genDType x, genDType y);"                               "\n"
"@overload vec3 cross(vec3 x, vec3 y);"                             "\n"
"@overload dvec3 cross(dvec3 x, dvec3 y);"                          "\n"
"genFType normalize(genFType x);"                                   "\n"
"genDType normalize(genDType x);"                                   "\n"
"genFType faceforward(genFType N, genFType I, genFType Nref);"      "\n"
"genDType faceforward(genDType N, genDType I, genDType Nref);"      "\n"
"genFType reflect(genFType I, genFType N);"                         "\n"
"genDType reflect(genDType I, genDType N);"                         "\n"
"genFType refract(genFType I, genFType N, float eta);"              "\n"
"genDType refract(genDType I, genDType N, double eta);"             "\n"

//matrix functions
SRC_LINE
"mat matrixCompMult(mat x, mat y);"                                 "\n"
"@overload mat2 outerProduct(vec2 c, vec2 r);"                      "\n"
"@overload mat3 outerProduct(vec3 c, vec3 r);"                      "\n"
"@overload mat4 outerProduct(vec4 c, vec4 r);"                      "\n"
"@overload mat2x3 outerProduct(vec3 c, vec2 r);"                    "\n"
"@overload mat3x2 outerProduct(vec2 c, vec3 r);"                    "\n"
"@overload mat2x4 outerProduct(vec4 c, vec2 r);"                    "\n"
"@overload mat4x2 outerProduct(vec2 c, vec4 r);"                    "\n"
"@overload mat3x4 outerProduct(vec4 c, vec3 r);"                    "\n"
"@overload mat4x3 outerProduct(vec3 c, vec4 r);"                    "\n"
"@overload mat2 transpose(mat2 m);"                                 "\n"
"@overload mat3 transpose(mat3 m);"                                 "\n"
"@overload mat4 transpose(mat4 m);"                                 "\n"
"@overload mat2x3 transpose(mat3x2 m);"                             "\n"
"@overload mat3x2 transpose(mat2x3 m);"                             "\n"
"@overload mat2x4 transpose(mat4x2 m);"                             "\n"
"@overload mat4x2 transpose(mat2x4 m);"                             "\n"
"@overload mat3x4 transpose(mat4x3 m);"                             "\n"
"@overload mat4x3 transpose(mat3x4 m);"                             "\n"
"@overload float determinant(mat2 m);"                              "\n"
"@overload float determinant(mat3 m);"                              "\n"
"@overload float determinant(mat4 m);"                              "\n"
"@overload mat2 inverse(mat2 m);"                                   "\n"
"@overload mat3 inverse(mat3 m);"                                   "\n"
"@overload mat4 inverse(mat4 m);"                                   "\n"

//vector relational functions
SRC_LINE
"@vector(bool,@width(vec)) lessThan(vec x, vec y);"                 "\n"
"@vector(bool,@width(ivec)) lessThan(ivec x, ivec y);"              "\n"
"@vector(bool,@width(uvec)) lessThan(uvec x, uvec y);"              "\n"
"@vector(bool,@width(vec)) lessThanEqual(vec x, vec y);"            "\n"
"@vector(bool,@width(ivec)) lessThanEqual(ivec x, ivec y);"         "\n"
"@vector(bool,@width(uvec)) lessThanEqual(uvec x, uvec y);"         "\n"
"@vector(bool,@width(vec)) greaterThan(vec x, vec y);"              "\n"
"@vector(bool,@width(ivec)) greaterThan(ivec x, ivec y);"           "\n"
"@vector(bool,@width(uvec)) greaterThan(uvec x, uvec y);"           "\n"
"@vector(bool,@width(vec)) greaterThanEqual(vec x, vec y);"         "\n"
"@vector(bool,@width(ivec)) greaterThanEqual(ivec x, ivec y);"      "\n"
"@vector(bool,@width(uvec)) greaterThanEqual(uvec x, uvec y);"      "\n"
"@vector(bool,@width(vec)) equal(vec x, vec y);"                    "\n"
"@vector(bool,@width(ivec)) equal(ivec x, ivec y);"                 "\n"
"@vector(bool,@width(uvec)) equal(uvec x, uvec y);"                 "\n"
"@vector(bool,@width(bvec)) equal(bvec x, bvec y);"                 "\n"
"@vector(bool,@width(vec)) notEqual(vec x, vec y);"                 "\n"
"@vector(bool,@width(ivec)) notEqual(ivec x, ivec y);"              "\n"
"@vector(bool,@width(uvec)) notEqual(uvec x, uvec y);"              "\n"
"@vector(bool,@width(bvec)) notEqual(bvec x, bvec y);"              "\n"
"bool any(bvec x);"                                                 "\n"
"bool all(bvec x);"                                                 "\n"
"@vector(bool,@width(bvec)) not(bvec x);"                           "\n"

//integer functions
SRC_LINE
"genUType uaddCarry(highp genUType x, highp genUType y,"            "\n"
"                   out lowp genUType carry);"                      "\n"
"genUType usubBorrow(highp genUType x, highp genUType y,"           "\n"
"                    out lowp genUType borrow);"                    "\n"
"void umulExtended(highp genUType x, highp genUType y,"             "\n"
"                  out highp genUType msb,"                         "\n"
"                  out highp genUType lsb);"                        "\n"
"void imulExtended(highp genIType x, highp genIType y,"             "\n"
"                  out highp genIType msb,"                         "\n"
"                  out highp genIType lsb);"                        "\n"
"genIType bitfieldExtract(genIType value, int offset, int bits);"   "\n"
"genUType bitfieldExtract(genUType value, int offset, int bits);"   "\n"
"genIType bitfieldInsert(genIType base, genIType insert,"           "\n"
"                        int offset, int bits);"                    "\n"
"genUType bitfieldInsert(genUType base, genUType insert,"           "\n"
"                        int offset, int bits);"                    "\n"
"genIType bitfieldReverse(highp genIType value);"                   "\n"
"genUType bitfieldReverse(highp genUType value);"                   "\n"
"genIType bitCount(genIType value);"                                "\n"
"genIType bitCount(genUType value);"                                "\n"
"genIType findLSB(genIType value);"                                 "\n"
"genIType findLSB(genUType value);"                                 "\n"
"genIType findMSB(highp genIType value);"                           "\n"
"genIType findMSB(highp genUType value);"                           "\n"
"};"                                                                "\n"
"#undef out"                                                        "\n"
"#undef highp"                                                      "\n"
"#undef lowp"                                                       "\n"
"#undef uint"                                                       "\n"
"#undef uvec2"                                                      "\n";
#if 0
//texture functions
//query
int textureSize(gsampler1D sampler, int lod)
ivec2 textureSize(gsampler2D sampler, int lod)
ivec3 textureSize(gsampler3D sampler, int lod)
ivec2 textureSize(gsamplerCube sampler, int lod)
int textureSize(sampler1DShadow sampler, int lod)
ivec2 textureSize(sampler2DShadow sampler, int lod)
ivec2 textureSize(samplerCubeShadow sampler, int lod) ivec3 textureSize(gsamplerCubeArray sampler, int lod)
ivec3 textureSize(samplerCubeArrayShadow sampler, int lod)
ivec2 textureSize(gsampler2DRect sampler)
ivec2 textureSize(sampler2DRectShadow sampler)
ivec2 textureSize(gsampler1DArray sampler, int lod)
ivec2 textureSize(sampler1DArrayShadow sampler, int lod)
ivec3 textureSize(gsampler2DArray sampler, int lod)
ivec3 textureSize(sampler2DArrayShadow sampler, int lod)
int textureSize(gsamplerBuffer sampler)
ivec2 textureSize(gsampler2DMS sampler)
ivec3 textureSize(gsampler2DMSArray sampler)
vec2 textureQueryLod(gsampler1D sampler, float P)
vec2 textureQueryLod(gsampler2D sampler, vec2 P)
vec2 textureQueryLod(gsampler3D sampler, vec3 P)
vec2 textureQueryLod(gsamplerCube sampler, vec3 P)
vec2 textureQueryLod(gsampler1DArray sampler, float P)
vec2 textureQueryLod(gsampler2DArray sampler, vec2 P)
vec2 textureQueryLod(gsamplerCubeArray sampler, vec3 P)
vec2 textureQueryLod(sampler1DShadow sampler, float P)
vec2 textureQueryLod(sampler2DShadow sampler, vec2 P)
vec2 textureQueryLod(samplerCubeShadow sampler, vec3 P)
vec2 textureQueryLod(sampler1DArrayShadow sampler, float P)
vec2 textureQueryLod(sampler2DArrayShadow sampler, vec2 P)
vec2 textureQueryLod(samplerCubeArrayShadow sampler, vec3 P)
int textureQueryLevels(gsampler1D sampler)
int textureQueryLevels(gsampler2D sampler)
int textureQueryLevels(gsampler3D sampler)
int textureQueryLevels(gsamplerCube sampler)
int textureQueryLevels(gsampler1DArray sampler)
int textureQueryLevels(gsampler2DArray sampler)
int textureQueryLevels(gsamplerCubeArray sampler)
int textureQueryLevels(sampler1DShadow sampler)
int textureQueryLevels(sampler2DShadow sampler)
int textureQueryLevels(samplerCubeShadow sampler)
int textureQueryLevels(sampler1DArrayShadow sampler)
int textureQueryLevels(sampler2DArrayShadow sampler)
int textureQueryLevels(samplerCubeArrayShado w sampler)
int textureSamples(gsampler2DMS sampler)
int textureSamples(gsampler2DMSArray sampler)
//texel lookup
gvec4 texture(gsampler1D sampler, float P [, float bias] )
gvec4 texture(gsampler2D sampler, vec2 P [, float bias] )
gvec4 texture(gsampler3D sampler, vec3 P [, float bias] )
gvec4 texture(gsamplerCube sampler, vec3 P[, float bias] )
float texture(sampler1DShadow sampler, vec3 P [, float bias])
float texture(sampler2DShadow sampler, vec3 P [, float bias])
float texture(samplerCubeShadow sampler, vec4 P [, float bias] )
gvec4 texture(gsampler2DArray sampler, vec3 P [, float bias] )
gvec4 texture(gsamplerCubeArray sampler, vec4 P [, float bias] )
gvec4 texture(gsampler1DArray sampler, vec2 P [, float bias] )
float texture(sampler1DArrayShadow sampler, vec3 P [, float bias] )
float texture(sampler2DArrayShadow sampler, vec4 P)
gvec4 texture(gsampler2DRect sampler, vec2 P)
float texture(sampler2DRectShadow sampler, vec3 P)
float texture(samplerCubeArrayShadow sampler, vec4 P, float compare)
gvec4 textureProj(gsampler1D sampler, vec2 P [, float bias] )
gvec4 textureProj(gsampler1D sampler, vec4 P [, float bias] )
gvec4 textureProj(gsampler2D sampler, vec3 P [, float bias] )
gvec4 textureProj(gsampler2D sampler, vec4 P [, float bias] )
gvec4 textureProj(gsampler3D sampler, vec4 P [, float bias] )
float textureProj(sampler1DShadow sampler, vec4 P [, float bias] )
float textureProj(sampler2DShadow sampler, vec4 P [, float bias] )
gvec4 textureProj(gsampler2DRect sampler, vec3 P)
gvec4 textureProj(gsampler2DRect sampler, vec4 P)
float textureProj(sampler2DRectShadow sampler, vec4 P)
gvec4 textureLod(gsampler1D sampler, float P, float lod)
gvec4 textureLod(gsampler2D sampler, vec2 P, float lod)
gvec4 textureLod(gsampler3D sampler, vec3 P, float lod)
gvec4 textureLod(gsamplerCube sampler, vec3 P, float lod)
float textureLod(sampler2DShadow sampler, vec3 P, float lod)
float textureLod(sampler1DShadow sampler, vec3 P, float lod)
gvec4 textureLod(gsampler1DArray sampler, vec2 P, float lod)
float textureLod(sampler1DArrayShadow sampler, vec3 P, float lod)
gvec4 textureLod(gsampler2DArray sampler, vec3 P, float lod)
gvec4 textureLod(gsamplerCubeArray sampler, vec4 P, float lod)
gvec4 textureOffset(gsampler1D sampler, float P, int offset [, float bias] )
gvec4 textureOffset(gsampler2D sampler, vec2 P, ivec2 offset [, float bias] )
gvec4 textureOffset(gsampler3D sampler, vec3 P, ivec3 offset [, float bias] )
float textureOffset(sampler2DShadow sampler, vec3 P, ivec2 offset [, float bias] )
gvec4 textureOffset(gsampler2DRect sampler, vec2 P, ivec2 offset)
float textureOffset(sampler2DRectShadow sampler, vec3 P, ivec2 offset)
float textureOffset(sampler1DShadow sampler, vec3 P, int offset [, float bias] )
gvec4 textureOffset(gsampler1DArray sampler, vec2 P, int offset [, float bias] )
gvec4 textureOffset(gsampler2DArray sampler, vec3 P, ivec2 offset [, float bias] )
float textureOffset(sampler1DArrayShadow sampler, vec3 P, int offset [, float bias] )
float textureOffset(sampler2DArrayShadow sampler, vec4 P, ivec2 offset)
gvec4 texelFetch(gsampler1D sampler, int P, int lod)
gvec4 texelFetch(gsampler2D sampler, ivec2 P, int lod)
gvec4 texelFetch(gsampler3D sampler, ivec3 P, int lod)
gvec4 texelFetch(gsampler2DRect sampler, ivec2 P)
gvec4 texelFetch(gsampler1DArray sampler, ivec2 P, int lod)
gvec4 texelFetch(gsampler2DArray sampler, ivec3 P, int lod)
gvec4 texelFetch(gsamplerBuffer sampler, int P)
gvec4 texelFetch(gsampler2DMS sampler, ivec2 P, int sample)
gvec4 texelFetch(gsampler2DMSArray sampler, ivec3 P, int sample)
gvec4 texelFetchOffset(gsampler1D sampler, int P, int lod, int offset)
gvec4 texelFetchOffset(gsampler2D sampler, ivec2 P, int lod, ivec2 offset)
gvec4 texelFetchOffset(gsampler3D sampler, ivec3 P, int lod, ivec3 offset)
gvec4 texelFetchOffset(gsampler2DRect sampler, ivec2 P, ivec2 offset)
gvec4 texelFetchOffset(gsampler1DArray sampler, ivec2 P, int lod, int offset)
gvec4 texelFetchOffset(gsampler2DArray sampler, ivec3 P, int lod, ivec2 offset)
gvec4 textureProjOffset(gsampler1D sampler, vec2 P, int offset [, float bias] )
gvec4 textureProjOffset(gsampler1D sampler, vec4 P, int offset [, float bias] )
gvec4 textureProjOffset(gsampler2D sampler, vec3 P, ivec2 offset [, float bias] )
gvec4 textureProjOffset(gsampler2D sampler, vec4 P, ivec2 offset [, float bias] )
gvec4 textureProjOffset(gsampler3D sampler, vec4 P, ivec3 offset [, float bias] )
gvec4 textureProjOffset(gsampler2DRect sampler, vec3 P, ivec2 offset)
gvec4 textureProjOffset(gsampler2DRect sampler, vec4 P, ivec2 offset)
float textureProjOffset(sampler2DRectShadow sampler, vec4 P, ivec2 offset)
float textureProjOffset(sampler1DShadow sampler, vec4 P, int offset [, float bias] )
float textureProjOffset(sampler2DShadow sampler, vec4 P, ivec2 offset [, float bias] )
gvec4 textureLodOffset(gsampler1D sampler, float P, float lod, int offset)
gvec4 textureLodOffset(gsampler2D sampler, vec2 P, float lod, ivec2 offset)
gvec4 textureLodOffset(gsampler3D sampler, vec3 P, float lod, ivec3 offset)
float textureLodOffset(sampler1DShadow sampler, vec3 P, float lod, int offset)
float textureLodOffset(sampler2DShadow sampler, vec3 P, float lod, ivec2 offset)
gvec4 textureLodOffset(gsampler1DArray sampler, vec2 P, float lod, int offset)
gvec4 textureLodOffset(gsampler2DArray sampler, vec3 P, float lod, ivec2 offset)
float textureLodOffset(sampler1DArrayShadow sampler, vec3 P, float lod, int offset)
gvec4 textureProjLod(gsampler1D sampler, vec2 P, float lod)
gvec4 textureProjLod(gsampler1D sampler, vec4 P, float lod)
gvec4 textureProjLod(gsampler2D sampler, vec3 P, float lod)
gvec4 textureProjLod(gsampler2D sampler, vec4 P, float lod)
gvec4 textureProjLod(gsampler3D sampler, vec4 P, float lod)
float textureProjLod(sampler1DShadow sampler, vec4 P, float lod)
float textureProjLod(sampler2DShadow sampler, vec4 P, float lod)
gvec4 textureProjLodOffset(gsampler1D sampler, vec2 P, float lod, int offset)
gvec4 textureProjLodOffset(gsampler1D sampler, vec4 P, float lod, int offset)
gvec4 textureProjLodOffset(gsampler2D sampler, vec3 P, float lod, ivec2 offset)
gvec4 textureProjLodOffset(gsampler2D sampler, vec4 P, float lod, ivec2 offset)
gvec4 textureProjLodOffset(gsampler3D sampler, vec4 P, float lod, ivec3 offset)
float textureProjLodOffset(sampler1DShadow sampler, vec4 P, float lod, int offset)
float textureProjLodOffset(sampler2DShadow sampler, vec4 P, float lod, ivec2 offset)
gvec4 textureGrad(gsampler1D sampler, float _P, float dPdx, float dPdy)
gvec4 textureGrad(gsampler2D sampler, vec2 P, vec2 dPdx, vec2 dPdy)
gvec4 textureGrad(gsampler3D sampler, P, vec3 dPdx, vec3 dPdy)
gvec4 textureGrad(gsamplerCube sampler, vec3 P, vec3 dPdx, vec3 dPdy)
gvec4 textureGrad(gsampler2DRect sampler, vec2 P, vec2 dPdx, vec2 dPdy)
float textureGrad(sampler2DRectShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy)
float textureGrad(sampler1DShadow sampler, vec3 P, float dPdx, float dPdy)
gvec4 textureGrad(gsampler1DArray sampler, vec2 P, float dPdx, float dPdy)
gvec4 textureGrad(gsampler2DArray sampler, vec3 P, vec2 dPdx, vec2 dPdy)
float textureGrad(sampler1DArrayShadow sampler, vec3 P, float dPdx, float dPdy)
float textureGrad(sampler2DShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy)
float textureGrad(samplerCubeShadow sampler, vec4 P, vec3 dPdx, vec3 dPdy)
float textureGrad(sampler2DArrayShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)
gvec4 textureGrad(gsamplerCubeArray sampler, vec4 P, vec3 dPdx, vec3 dPdy)
gvec4 textureGradOffset(gsampler1D sampler, float P, float dPdx, float dPdy, int offset)
gvec4 textureGradOffset(gsampler2D sampler, vec2 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
gvec4 textureGradOffset(gsampler3D sampler, vec3 P, vec3 dPdx, vec3 dPdy, ivec3 offset)
gvec4 textureGradOffset(gsampler2DRect sampler, vec2 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
float textureGradOffset(sampler2DRectShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
float textureGradOffset(sampler1DShadow sampler, vec3 P, float dPdx, float dPdy, int offset)
float textureGradOffset(sampler2DShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
gvec4 textureGradOffset(gsampler2DArray sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
gvec4 textureGradOffset(gsampler1DArray sampler, vec2 P, float dPdx, float dPdy, int offset)
float textureGradOffset(sampler1DArrayShadow sampler, vec3 P, float dPdx, float dPdy, int offset)
float textureGradOffset(sampler2DArrayShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
gvec4 textureProjGrad(gsampler1D sampler, vec2 P, float dPdx, float dPdy)
gvec4 textureProjGrad(gsampler1D sampler, vec4 P, float dPdx, float dPdy)
gvec4 textureProjGrad(gsampler2D sampler, vec3 P, vec2 dPdx, vec2 dPdy)
gvec4 textureProjGrad(gsampler2D sampler, vec4 P, vec2 dPdx, vec2 dPdy)
gvec4 textureProjGrad(gsampler3D sampler, vec4 P, vec3 dPdx, vec3 dPdy)
gvec4 textureProjGrad(gsampler2DRect sampler, vec3 P, vec2 dPdx, vec2 dPdy)
gvec4 textureProjGrad(gsampler2DRect sampler, vec4 P, vec2 dPdx, vec2 dPdy)
float textureProjGrad(sampler2DRectShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)
float textureProjGrad(sampler1DShadow sampler, vec4 P, float dPdx, float dPdy)
float textureProjGrad(sampler2DShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)
gvec4 textureProjGradOffset(gsampler1D sampler, vec2 P, float dPdx, float dPdy, int offset)
gvec4 textureProjGradOffset(gsampler1D sampler, vec4 P, float dPdx, float dPdy, int offset)
gvec4 textureProjGradOffset(gsampler2D sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
gvec4 textureProjGradOffset(gsampler2D sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
gvec4 textureProjGradOffset(gsampler3D sampler, vec4 P, vec3 dPdx, vec3 dPdy, ivec3 offset)
gvec4 textureProjGradOffset(gsampler2DRect sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
gvec4 textureProjGradOffset(gsampler2DRect sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
float textureProjGradOffset(sampler2DRectShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
float textureProjGradOffset(sampler1DShadow sampler, vec4 P, float dPdx, float dPdy, int offset)
float textureProjGradOffset(sampler2DShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
//gather
gvec4 textureGather(gsampler2D sampler, vec2 P [, int comp])
gvec4 textureGather(gsampler2DArray sampler, vec3 P [, int comp])
gvec4 textureGather(gsamplerCube sampler, vec3 P [, int comp])
gvec4 textureGather(gsamplerCubeArray sampler, vec4 P[, int comp])
gvec4 textureGather(gsampler2DRect sampler, vec2 P[, int comp])
vec4 textureGather(sampler2DShadow sampler, vec2 P, float refZ)
vec4 textureGather(sampler2DArrayShadow sampler, vec3 P, float refZ)
vec4 textureGather(samplerCubeShadow sampler, vec3 P, float refZ)
vec4 textureGather(samplerCubeArrayShadow sampler, vec4 P, float refZ)
vec4 textureGather(sampler2DRectShadow sampler, vec2 P, float refZ)
gvec4 textureGatherOffset(gsampler2D sampler, vec2 P, ivec2 offset, [ int comp])
gvec4 textureGatherOffset(gsampler2DArray sampler, vec3 P, ivec2 offset [ int comp])
vec4 textureGatherOffset(sampler2DShadow sampler, vec2 P, float refZ, ivec2 offset)
vec4 textureGatherOffset(sampler2DArrayShadow sampler, vec3 P, float refZ, ivec2 offset)
gvec4 textureGatherOffset(gsampler2DRect sampler, vec2 P, ivec2 offset [ int comp])
vec4 textureGatherOffset(sampler2DRectShadow sampler, vec2 P, float refZ, ivec2 offset)
gvec4 textureGatherOffsets(gsampler2D sampler, vec2 P, ivec2 offsets[4] [, int comp])
gvec4 textureGatherOffsets(gsampler2DArray sampler, vec3 P, ivec2 offsets[4] [, int comp])
vec4 textureGatherOffsets(sampler2DShadow sampler, vec2 P, float refZ, ivec2 offsets[4])
vec4 textureGatherOffsets(sampler2DArrayShadow sampler, vec3 P, float refZ, ivec2 offsets[4])
gvec4 textureGatherOffsets(gsampler2DRect sampler, vec2 P, ivec2 offsets[4] [, int comp])
vec4 textureGatherOffsets(sampler2DRectShadow sampler, vec2 P, float refZ, ivec2 offsets[4])

//atomic memory functions
uint atomicAdd(inout uint mem, uint data)
int atomicAdd(inout int mem, int data)
uint atomicMin(inout uint mem, uint data)
int atomicMin(inout int mem, int data)
uint atomicMax(inout uint mem, uint data)
int atomicMax(inout int mem, int data)
uint atomicAnd(inout uint mem, uint data)
int atomicAnd(inout int mem, int data)
uint atomicOr(inout uint mem, uint data)
int atomicOr(inout int mem, int data)
uint atomicXor(inout uint mem, uint data)
int atomicXor(inout int mem, int data)
uint atomicExchange(inout uint mem, uint data)
int atomicExchange(inout int mem, int data)
uint atomicCompSwap(inout uint mem, uint compare, uint data)
int atomicCompSwap(inout int mem, int compare, int data)

//image functions
int imageSize(readonly writeonly gimage1D image)
ivec2 imageSize(readonly writeonly gimage2D image)
ivec3 imageSize(readonly writeonly gimage3D image)
ivec2 imageSize(readonly writeonly gimageCube image)
ivec3 imageSize(readonly writeonly gimageCubeArray image)
ivec3 imageSize(readonly writeonly gimage2DArray image)
ivec2 imageSize(readonly writeonly gimageRect image)
ivec2 imageSize(readonly writeonly gimage1DArray image)
ivec2 imageSize(readonly writeonly gimage2DMS image)
ivec3 imageSize(readonly writeonly gimage2DMSArray image)
int imageSize(readonly writeonly gimageBuffer image)
int imageSamples(readonly writeonly gimage2DMS image)
int imageSamples(readonly writeonly gimage2DMSArray image)
gvec4 imageLoad(readonly IMAGE_PARAMS)
void imageStore(writeonly IMAGE_PARAMS, gvec4 data)
uint imageAtomicAdd(IMAGE_PARAMS, uint data)
int imageAtomicAdd(IMAGE_PARAMS, int data)
uint imageAtomicMin(IMAGE_PARAMS, uint data)
int imageAtomicMin(IMAGE_PARAMS, int data)
uint imageAtomicMax(IMAGE_PARAMS, uint data)
int imageAtomicMax(IMAGE_PARAMS, int data)
uint imageAtomicAnd(IMAGE_PARAMS, uint data)
int imageAtomicAnd(IMAGE_PARAMS, int data)
uint imageAtomicOr(IMAGE_PARAMS, uint data)
int imageAtomicOr(IMAGE_PARAMS, int data)
uint imageAtomicXor(IMAGE_PARAMS, uint data)
int imageAtomicXor(IMAGE_PARAMS, int data)
uint imageAtomicExchange(IMAGE_PARAMS, uint data)
int imageAtomicExchange(IMAGE_PARAMS, int data)
float imageAtomicExchange(IMAGE_PARAMS, float data)
uint imageAtomicCompSwap(IMAGE_PARAMS, uint compare, uint data)
int imageAtomicCompSwap(IMAGE_PARAMS, int compare, int data)
#endif
//geometry shader functions
static const char *glsl_geometry_functions =
SRC_LINE
"void EmitStreamVertex(int stream);"    "\n"
"void EndStreamPrimitive(int stream);"  "\n"
"void EmitVertex();"                    "\n"
"void EndPrimitive();"                  "\n";
#if 0
//fragment processing functions
//derivative
genFType dFdx(genFType p)
genFType dFdy(genFType p)
genFType dFdxFine(genFType p)
genFType dFdyFine(genFType p)
genFType dFdxCoarse(genFType p)
genFType dFdyCoarse(genFType p)
genFType fwidth(genFType p)
genFType fwidthFine(genFType p)
genFType fwidthCoarse(genFType p)
//interpolation
float interpolateAtCentroid(float interpolant)
vec2 interpolateAtCentroid(vec2 interpolant)
vec3 interpolateAtCentroid(vec3 interpolant)
vec4 interpolateAtCentroid(vec4 interpolant)
float interpolateAtSample(float interpolant, int sample)
vec2 interpolateAtSample(vec2 interpolant, int sample)
vec3 interpolateAtSample(vec3 interpolant, int sample)
vec4 interpolateAtSample(vec4 interpolant, int sample)
float interpolateAtOffset(float interpolant, vec2 offset)
vec2 interpolateAtOffset(vec2 interpolant, vec2 offset)
vec3 interpolateAtOffset(vec3 interpolant, vec2 offset)
vec4 interpolateAtOffset(vec4 interpolant, vec2 offset)

//shader invocation control functions
void barrier()

//shader memory control functions
void memoryBarrier()
void memoryBarrierAtomicCounter()
void memoryBarrierBuffer()
void memoryBarrierShared()
void memoryBarrierImage()
void groupMemoryBarrier()

//subpass-input functions
gvec4 subpassLoad(gsubpassInput subpass)
gvec4 subpassLoad(gsubpassInputMS subpass, int sample)

//shader invocation group functions
bool anyInvocation(bool value)
bool allInvocations(bool value)
bool allInvocationsEqual(bool value)
#endif

void
glsl_multiview (int behavior, void *scanner)
{
	if (behavior) {
		rua_parse_define ("GL_EXT_multiview 1\n");
	} else {
		rua_undefine ("GL_EXT_multiview", scanner);
	}
}

static int glsl_include_state = 0;

bool
glsl_on_include (const char *name)
{
	if (!glsl_include_state) {
		error (0, "'#include' : required extension not requested");
		return false;
	}
	if (glsl_include_state > 1) {
		warning (0, "'#include' : required extension not requested");
	}
	return true;
}

void
glsl_include (int behavior, void *scanner)
{
	glsl_include_state = behavior;
}

static void
glsl_parse_vars (const char *var_src)
{
	glsl_parse_string (var_src);
}

static void
glsl_init_common (void)
{
	current_language.initialized = true;
	glsl_block_clear ();
	qc_parse_string (glsl_general_functions);
	glsl_parse_vars (glsl_system_constants);
}

void
glsl_init_comp (void)
{
	glsl_init_common ();
	glsl_parse_vars (glsl_compute_vars);
}

void
glsl_init_vert (void)
{
	glsl_init_common ();
	glsl_parse_vars (glsl_Vulkan_vertex_vars);
}

void
glsl_init_tesc (void)
{
	glsl_init_common ();
	glsl_parse_vars (glsl_tesselation_control_vars);
}

void
glsl_init_tese (void)
{
	glsl_init_common ();
	glsl_parse_vars (glsl_tesselation_evaluation_vars);
}

void
glsl_init_geom (void)
{
	glsl_init_common ();
	glsl_parse_vars (glsl_geometry_vars);
	qc_parse_string (glsl_geometry_functions);
}

void
glsl_init_frag (void)
{
	glsl_init_common ();
	glsl_parse_vars (glsl_fragment_vars);
}
