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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/iface_block.h"
#include "tools/qfcc/include/image.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

glsl_sublang_t glsl_sublang;

#define SRC_LINE_EXP2(l,f) "#line " #l " " #f "\n"
#define SRC_LINE_EXP(l,f) SRC_LINE_EXP2(l,f)
#define SRC_LINE SRC_LINE_EXP (__LINE__ + 1, __FILE__)

static const char *glsl_Vulkan_vertex_vars =
SRC_LINE
"layout (builtin=\"VertexIndex\") in int gl_VertexIndex;"               "\n"
"layout (builtin=\"InstanceIndex\") in int gl_InstanceIndex;"           "\n"
"#ifdef GL_EXT_multiview"                                               "\n"
"layout (builtin=\"ViewIndex\") in highp int gl_ViewIndex;"             "\n"
"#endif"                                                                "\n"
"out gl_PerVertex {"                                                    "\n"
"    layout (builtin=\"Position\") vec4 gl_Position;"                   "\n"
"    layout (builtin=\"PointSize\") float gl_PointSize;"                "\n"
"    layout (builtin=\"ClipDistance\") float gl_ClipDistance[1];"        "\n"
"    layout (builtin=\"CullDistance\") float gl_CullDistance[1];"        "\n"
"};"                                                                    "\n";

//tesselation control
static const char *glsl_tesselation_control_vars =
SRC_LINE
"in gl_PerVertex {"                                                     "\n"
"    layout (builtin=\"Position\") vec4 gl_Position;"                   "\n"
"    layout (builtin=\"PointSize\") float gl_PointSize;"                "\n"
"    layout (builtin=\"ClipDistance\") float gl_ClipDistance[1];"        "\n"
"    layout (builtin=\"CullDistance\") float gl_CullDistance[1];"        "\n"
"} gl_in[gl_MaxPatchVertices];"                                         "\n"
"layout (builtin=\"PatchVertices\") in int gl_PatchVerticesIn;"         "\n"
"layout (builtin=\"PrimitiveId\") in int gl_PrimitiveID;"               "\n"
"layout (builtin=\"InvocationId\") in int gl_InvocationID;"             "\n"
"#ifdef GL_EXT_multiview"                                               "\n"
"layout (builtin=\"ViewIndex\") in highp int gl_ViewIndex;"             "\n"
"#endif"                                                                "\n"
"out gl_PerVertex {"                                                    "\n"
"    layout (builtin=\"Position\") vec4 gl_Position;"                   "\n"
"    layout (builtin=\"PointSize\") float gl_PointSize;"                "\n"
"    layout (builtin=\"ClipDistance\") float gl_ClipDistance[1];"        "\n"
"    layout (builtin=\"CullDistance\") float gl_CullDistance[1];"        "\n"
"} gl_out[];"                                                           "\n"
"layout (builtin=\"TessLevelOuter\") patch out float gl_TessLevelOuter[4];\n"
"layout (builtin=\"TessLevelInner\") patch out float gl_TessLevelInner[2];\n";

static const char *glsl_tesselation_evaluation_vars =
SRC_LINE
"in gl_PerVertex {"                                                     "\n"
"    layout (builtin=\"Position\") vec4 gl_Position;"                   "\n"
"    layout (builtin=\"PointSize\") float gl_PointSize;"                "\n"
"    layout (builtin=\"ClipDistance\") float gl_ClipDistance[1];"        "\n"
"    layout (builtin=\"CullDistance\") float gl_CullDistance[1];"        "\n"
"} gl_in[gl_MaxPatchVertices];"                                         "\n"
"layout (builtin=\"PatchVertices\") in int gl_PatchVerticesIn;"         "\n"
"layout (builtin=\"PrimitiveId\") in int gl_PrimitiveID;"               "\n"
"layout (builtin=\"TessCoord\") in vec3 gl_TessCoord;"                  "\n"
"layout (builtin=\"TessLevelOuter\") patch in float gl_TessLevelOuter[4];" "\n"
"layout (builtin=\"TessLevelInner\") patch in float gl_TessLevelInner[2];" "\n"
"#ifdef GL_EXT_multiview"                                               "\n"
"layout (builtin=\"ViewIndex\") in highp int gl_ViewIndex;"             "\n"
"#endif"                                                                "\n"
"out gl_PerVertex {"                                                    "\n"
"    layout (builtin=\"Position\") vec4 gl_Position;"                   "\n"
"    layout (builtin=\"PointSize\") float gl_PointSize;"                "\n"
"    layout (builtin=\"ClipDistance\") float gl_ClipDistance[1];"        "\n"
"    layout (builtin=\"CullDistance\") float gl_CullDistance[1];"        "\n"
"};"                                                                    "\n";

static const char *glsl_geometry_vars =
SRC_LINE
"in gl_PerVertex {"                                                     "\n"
"    layout (builtin=\"Position\") vec4 gl_Position;"                   "\n"
"    layout (builtin=\"PointSize\") float gl_PointSize;"                "\n"
"    layout (builtin=\"ClipDistance\") float gl_ClipDistance[1];"        "\n"
"    layout (builtin=\"CullDistance\") float gl_CullDistance[1];"        "\n"
"} gl_in[];"                                                            "\n"
"layout (builtin=\"PrimitiveId\") in int gl_PrimitiveIDIn;"             "\n"
"layout (builtin=\"InvocationId\") in int gl_InvocationID;"             "\n"
"#ifdef GL_EXT_multiview"                                               "\n"
"layout (builtin=\"ViewIndex\") in highp int gl_ViewIndex;"             "\n"
"#endif"                                                                "\n"
"out gl_PerVertex {"                                                    "\n"
"    layout (builtin=\"Position\") vec4 gl_Position;"                   "\n"
"    layout (builtin=\"PointSize\") float gl_PointSize;"                "\n"
"    layout (builtin=\"ClipDistance\") float gl_ClipDistance[1];"        "\n"
"    layout (builtin=\"CullDistance\") float gl_CullDistance[1];"        "\n"
"};"                                                                    "\n"
"layout (builtin=\"PrimitiveId\") out int gl_PrimitiveID;"              "\n"
"layout (builtin=\"Layer\") out int gl_Layer;"                          "\n"
"layout (builtin=\"ViewportIndex\") out int gl_ViewportIndex;"          "\n";

static const char *glsl_fragment_vars =
SRC_LINE
"layout (builtin=\"FragCoord\") in vec4 gl_FragCoord;"                  "\n"
"layout (builtin=\"FrontFacing\") in bool gl_FrontFacing;"              "\n"
"layout (builtin=\"ClipDistance\") in float gl_ClipDistance[1];"         "\n"
"layout (builtin=\"CullDistance\") in float gl_CullDistance[1];"         "\n"
"layout (builtin=\"PointCoord\") in vec2 gl_PointCoord;"                "\n"
"layout (builtin=\"PrimitiveId\") in int gl_PrimitiveID;"               "\n"
"layout (builtin=\"SampleId\") in int gl_SampleID;"                     "\n"
"layout (builtin=\"SamplePosition\") in vec2 gl_SamplePosition;"        "\n"
"layout (builtin=\"SampleMask\") in int gl_SampleMaskIn[1];"             "\n"
"layout (builtin=\"Layer\") in int gl_Layer;"                           "\n"
"layout (builtin=\"ViewportIndex\") in int gl_ViewportIndex;"           "\n"
"layout (builtin=\"HelperInvocation\") in bool gl_HelperInvocation;"    "\n"
"#ifdef GL_EXT_multiview"                                               "\n"
"layout (builtin=\"ViewIndex\") in highp flat int gl_ViewIndex;"        "\n"
"#endif"                                                                "\n"
"layout (builtin=\"FragDepth\") out float gl_FragDepth;"                "\n"
"layout (builtin=\"SampleMask\") out int gl_SampleMask[1];"              "\n";

static const char *glsl_compute_vars =
SRC_LINE
"// workgroup dimensions"                                               "\n"
"layout (builtin=\"NumWorkgroups\") in uvec3 gl_NumWorkGroups;"         "\n"
"layout (builtin=\"WorkgroupSize\") in uvec3 gl_WorkGroupSize;"         "\n"
"// workgroup and invocation IDs"                                       "\n"
"layout (builtin=\"WorkgroupId\") in uvec3 gl_WorkGroupID;"             "\n"
"layout (builtin=\"LocalInvocationId\") in uvec3 gl_LocalInvocationID;" "\n"
"// derived variables"                                                  "\n"
"layout (builtin=\"GlobalInvocationId\") in uvec3 gl_GlobalInvocationID;""\n"
"layout (builtin=\"LocalInvocationIndex\") in uint gl_LocalInvocationIndex;\n";

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
	&type_uvec2,
	&type_uvec3,
	&type_uvec4,
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

#define SPV(op) "@intrinsic(" #op ")"
#define GLSL(op) "@intrinsic(OpExtInst, \"GLSL.std.450\", " #op ")"

static const char *glsl_general_functions =
SRC_LINE
"#define out @out"                                                  "\n"
"#define highp"                                                     "\n"
"#define lowp"                                                      "\n"
"#define gbvec(base) @vector(bool, @width(base))"                   "\n"
"#define gvec(base) @vector(float, @width(base))"                   "\n"
"#define gdvec(base) @vector(double, @width(base))"                 "\n"
"#define givec(base) @vector(int, @width(base))"                    "\n"
"#define guvec(base) @vector(uint, @width(base))"                   "\n"
"@generic(genFType=@vector(float),"                                 "\n"
"         genDType=@vector(double),"                                "\n"
"         genIType=@vector(int),"                                   "\n"
"         genUType=@vector(uint),"                                  "\n"
"         genBType=@vector(bool),"                                  "\n"
"         mat=@matrix(float),"                                      "\n"
"         vec=[vec2,vec3,vec4,dvec2,dvec3,dvec4],"                  "\n"
"         ivec=[ivec2,ivec3,ivec4],"                                "\n"
"         uvec=[uvec2,uvec3,uvec4],"                                "\n"
"         bvec=[bvec2,bvec3,bvec4]) {"                              "\n"
"genFType radians(genFType degrees) = " GLSL(Radians) ";"           "\n"
"genFType degrees(genFType radians) = " GLSL(Degrees) ";"           "\n"
"genFType sin(genFType angle) = " GLSL(Sin) ";"                     "\n"
"genFType cos(genFType angle) = " GLSL(Cos) ";"                     "\n"
"genFType tan(genFType angle) = " GLSL(Tan) ";"                     "\n"
"genFType asin(genFType x) = " GLSL(Asin) ";"                       "\n"
"genFType acos(genFType x) = " GLSL(Acos) ";"                       "\n"
"genFType atan(genFType y, genFType x) = " GLSL(Atan2) ";"          "\n"
"genFType atan(genFType y_over_x) = " GLSL(Atan) ";"                "\n"
"genFType sinh(genFType x) = " GLSL(Sinh) ";"                       "\n"
"genFType cosh(genFType x) = " GLSL(Cosh) ";"                       "\n"
"genFType tanh(genFType x) = " GLSL(Tanh) ";"                       "\n"
"genFType asinh(genFType x) = " GLSL(Asinh) ";"                     "\n"
"genFType acosh(genFType x) = " GLSL(Acosh) ";"                     "\n"
"genFType atanh(genFType x) = " GLSL(Atanh) ";"                     "\n"

//exponential functions
SRC_LINE
"genFType pow(genFType x, genFType y) = " GLSL(Pow) ";"             "\n"
"genFType exp(genFType x) = " GLSL(Exp) ";"                         "\n"
"genFType log(genFType x) = " GLSL(Log) ";"                         "\n"
"genFType exp2(genFType x) = " GLSL(Exp2) ";"                       "\n"
"genFType log2(genFType x) = " GLSL(Log2) ";"                       "\n"
"genFType sqrt(genFType x) = " GLSL(Sqrt) ";"                       "\n"
"genDType sqrt(genDType x) = " GLSL(Sqrt) ";"                       "\n"
"genFType inversesqrt(genFType x) = " GLSL(InverseSqrt) ";"         "\n"
"genDType inversesqrt(genDType x) = " GLSL(InverseSqrt) ";"         "\n"

//common functions
SRC_LINE
"genFType abs(genFType x) = " GLSL(FAbs) ";"                        "\n"
"genIType abs(genIType x) = " GLSL(SAbs) ";"                        "\n"
"genDType abs(genDType x) = " GLSL(FAbs) ";"                        "\n"
"genFType sign(genFType x) = " GLSL(FSign) ";"                      "\n"
"genIType sign(genIType x) = " GLSL(SSign) ";"                      "\n"
"genDType sign(genDType x) = " GLSL(FSign) ";"                      "\n"
"genFType floor(genFType x) = " GLSL(Floor) ";"                     "\n"
"genDType floor(genDType x) = " GLSL(Floor) ";"                     "\n"
"genFType trunc(genFType x) = " GLSL(Trunc) ";"                     "\n"
"genDType trunc(genDType x) = " GLSL(Trunc) ";"                     "\n"
"genFType round(genFType x) = " GLSL(Round) ";"                     "\n"
"genDType round(genDType x) = " GLSL(Round) ";"                     "\n"
"genFType roundEven(genFType x) = " GLSL(RoundEven) ";"             "\n"
"genDType roundEven(genDType x) = " GLSL(RoundEven) ";"             "\n"
"genFType ceil(genFType x) = " GLSL(Ceil) ";"                       "\n"
"genDType ceil(genDType x) = " GLSL(Ceil) ";"                       "\n"
"genFType fract(genFType x) = " GLSL(Fract) ";"                     "\n"
"genDType fract(genDType x) = " GLSL(Fract) ";"                     "\n"
"genFType mod(genFType x, float y) = " SPV(OpFMod) "[x, @construct (genFType, y)];" "\n"
"genFType mod(genFType x, genFType y) = " SPV(OpFMod) ";"           "\n"
"genDType mod(genDType x, double y) = " SPV(OpFMod) "[x, @construct (genDType, y)];" "\n"
"genDType mod(genDType x, genDType y) = " SPV(OpFMod) ";"           "\n"
"genFType modf(genFType x, out genFType i);"                        "\n"
"genDType modf(genDType x, out genDType i);"                        "\n"
"genFType min(genFType x, genFType y) = " GLSL(FMin) ";"            "\n"
"genFType min(genFType x, float y) = " GLSL(FMin) "[x, @construct (genFType, y)];" "\n"
"genDType min(genDType x, genDType y) = " GLSL(FMin) ";"            "\n"
"genDType min(genDType x, double y) = " GLSL(FMin) "[x, @construct (genDType, y)];" "\n"
"genIType min(genIType x, genIType y) = " GLSL(SMin) ";"            "\n"
"genIType min(genIType x, int y) = " GLSL(SMin) "[x, @construct (genIType, y)];" "\n"
"genUType min(genUType x, genUType y) = " GLSL(UMin) ";"            "\n"
"genUType min(genUType x, uint y) = " GLSL(UMin) "[x, @construct (genUType, y)];" "\n"
"genFType max(genFType x, genFType y) = " GLSL(FMax) ";"            "\n"
"genFType max(genFType x, float y) = " GLSL(FMax) "[x, @construct (genFType, y)];" "\n"
"genDType max(genDType x, genDType y) = " GLSL(FMax) ";"            "\n"
"genDType max(genDType x, double y) = " GLSL(FMax) "[x, @construct (genDType, y)];" "\n"
"genIType max(genIType x, genIType y) = " GLSL(SMax) ";"            "\n"
"genIType max(genIType x, int y) = " GLSL(SMax) "[x, @construct (genIType, y)];" "\n"
"genUType max(genUType x, genUType y) = " GLSL(UMax) ";"            "\n"
"genUType max(genUType x, uint y) = " GLSL(UMax) "[x, @construct (genUType, y)];" "\n"
"genFType clamp(genFType x, genFType minVal, genFType maxVal)"      "\n"
	" = " GLSL(FClamp) ";"                                          "\n"
"genFType clamp(genFType x, float minVal, float maxVal)"            "\n"
	" = " GLSL(FClamp) "[x, @construct (genFType, minVal),"         "\n"
						"@construct (genFType, maxVal)];"           "\n"
"genDType clamp(genDType x, genDType minVal, genDType maxVal)"      "\n"
	" = " GLSL(FClamp) ";"                                          "\n"
"genDType clamp(genDType x, double minVal, double maxVal)"          "\n"
	" = " GLSL(FClamp) "[x, @construct (genDType, minVal),"         "\n"
						"@construct (genDType, maxVal)];"           "\n"
"genIType clamp(genIType x, genIType minVal, genIType maxVal)"      "\n"
	" = " GLSL(SClamp) ";"                                          "\n"
"genIType clamp(genIType x, int minVal, int maxVal)"                "\n"
	" = " GLSL(SClamp) "[x, @construct (genIType, minVal),"         "\n"
						"@construct (genIType, maxVal)];"           "\n"
"genUType clamp(genUType x, genUType minVal, genUType maxVal)"      "\n"
	" = " GLSL(UClamp) ";"                                          "\n"
"genUType clamp(genUType x, uint minVal, uint maxVal)"              "\n"
	" = " GLSL(UClamp) "[x, @construct (genUType, minVal),"         "\n"
						"@construct (genUType, maxVal)];"           "\n"
"genFType mix(genFType x, genFType y, genFType a)"                  "\n"
	" = " GLSL(FMix) ";"                                            "\n"
"genFType mix(genFType x, genFType y, float a)"                     "\n"
	" = " GLSL(FMix) "[x, y, @construct (genFType, a)];"            "\n"
"genDType mix(genDType x, genDType y, genDType a)"                  "\n"
	" = " GLSL(FMix) ";"                                            "\n"
"genDType mix(genDType x, genDType y, double a)"                    "\n"
	" = " GLSL(FMix) "[x, y, @construct (genDType, a)];"            "\n"
"genFType mix(genFType x, genFType y, genBType a)"                  "\n"
	" = " SPV(OpSelect) "[a,y,x];"                                  "\n"
"genDType mix(genDType x, genDType y, genBType a)"                  "\n"
	" = " SPV(OpSelect) "[a,y,x];"                                  "\n"
"genIType mix(genIType x, genIType y, genBType a)"                  "\n"
	" = " SPV(OpSelect) "[a,y,x];"                                  "\n"
"genUType mix(genUType x, genUType y, genBType a)"                  "\n"
	" = " SPV(OpSelect) "[a,y,x];"                                  "\n"
"genBType mix(genBType x, genBType y, genBType a)"                  "\n"
	" = " SPV(OpSelect) "[a,y,x];"                                  "\n"
"genFType step(genFType edge, genFType x)"                          "\n"
	" = " GLSL(Step) ";"                                            "\n"
"genFType step(float edge, genFType x)"                             "\n"
	" = " GLSL(Step) "[@construct(genFType, edge), x];"             "\n"
"genDType step(genDType edge, genDType x)"                          "\n"
	" = " GLSL(Step) ";"                                            "\n"
"genDType step(double edge, genDType x)"                            "\n"
	" = " GLSL(Step) "[@construct(genDType, edge), x];"             "\n"
"genFType smoothstep(genFType edge0, genFType edge1, genFType x)"   "\n"
	" = " GLSL(SmoothStep) ";"                                      "\n"
"genFType smoothstep(float edge0, float edge1, genFType x)"         "\n"
	" = " GLSL(SmoothStep) "[@construct(genFType, edge0),"          "\n"
							"@construct(genFType, edge1), x];"      "\n"
"genDType smoothstep(genDType edge0, genDType edge1, genDType x)"   "\n"
	" = " GLSL(SmoothStep) ";"                                      "\n"
"genDType smoothstep(double edge0, double edge1, genDType x)"       "\n"
	" = " GLSL(SmoothStep) "[@construct(genDType, edge0),"          "\n"
							"@construct(genDType, edge1), x];"      "\n"
"gbvec(genFType) isnan(genFType x) = " SPV(OpIsNan) ";"             "\n"
"gbvec(genDType) isnan(genDType x) = " SPV(OpIsNan) ";"             "\n"
"gbvec(genFType) isinf(genFType x) = " SPV(OpIsInf) ";"             "\n"
"gbvec(genDType) isinf(genDType x) = " SPV(OpIsInf) ";"             "\n"
"givec(genFType) floatBitsToInt(highp genFType value) = " SPV(OpBitcast) ";" "\n"
"guvec(genFType) floatBitsToUint(highp genFType value) = " SPV(OpBitcast) ";" "\n"
"gvec(genIType) intBitsToFloat(highp genIType value) = " SPV(OpBitcast) ";" "\n"
"gvec(genUType) uintBitsToFloat(highp genUType value) = " SPV(OpBitcast) ";" "\n"
"genFType fma(genFType a, genFType b, genFType c) = " GLSL(Fma) ";" "\n"
"genDType fma(genDType a, genDType b, genDType c) = " GLSL(Fma) ";" "\n"
"genFType frexp(highp genFType x, out highp genIType exp) = " GLSL(Frexp) ";" "\n"
"genDType frexp(genDType x, out genIType exp) = " GLSL(Frexp) ";"   "\n"
"genFType ldexp(highp genFType x, highp genIType exp) = " GLSL(Ldexp) ";" "\n"
"genDType ldexp(genDType x, genIType exp) = " GLSL(Ldexp) ";"       "\n"

//floating-point pack and unpack functions
SRC_LINE
"highp uint packUnorm2x16(vec2 v) = " GLSL(PackUnorm2x16) ";"       "\n"
"highp uint packSnorm2x16(vec2 v) = " GLSL(PackSnorm2x16) ";"       "\n"
"uint packUnorm4x8(vec4 v) = " GLSL(PackUnorm4x8) ";"               "\n"
"uint packSnorm4x8(vec4 v) = " GLSL(PackSnorm4x8) ";"               "\n"
"vec2 unpackUnorm2x16(highp uint p) = " GLSL(UnpackUnorm2x16) ";"   "\n"
"vec2 unpackSnorm2x16(highp uint p) = " GLSL(UnpackSnorm2x16) ";"   "\n"
"vec4 unpackUnorm4x8(highp uint p) = " GLSL(UnpackUnorm4x8) ";"     "\n"
"vec4 unpackSnorm4x8(highp uint p) = " GLSL(UnpackSnorm4x8) ";"     "\n"
"uint packHalf2x16(vec2 v) = " GLSL(PackHalf2x16) ";"               "\n"
"vec2 unpackHalf2x16(uint v) = " GLSL(UnpackHalf2x16) ";"           "\n"
"double packDouble2x32(uvec2 v) = " GLSL(PackDouble2x32) ";"        "\n"
"uvec2 unpackDouble2x32(double v) = " GLSL(UnpackDouble2x32) ";"    "\n"

//geometric functions
SRC_LINE
"float length(genFType x) = " GLSL(Length) ";"                      "\n"
"double length(genDType x) = " GLSL(Length) ";"                     "\n"
"float distance(genFType p0, genFType p1) = " GLSL(Distance) ";"    "\n"
"double distance(genDType p0, genDType p1) = " GLSL(Distance) ";"   "\n"
"float dot(genFType x, genFType y) = " SPV(OpDot) ";"               "\n"
"double dot(genDType x, genDType y) = " SPV(OpDot) ";"              "\n"
"@overload vec3 cross(vec3 x, vec3 y) = " GLSL(Cross) ";"           "\n"
"@overload dvec3 cross(dvec3 x, dvec3 y) = " GLSL(Cross) ";"        "\n"
"genFType normalize(genFType x) = " GLSL(Normalize) ";"             "\n"
"genDType normalize(genDType x) = " GLSL(Normalize) ";"             "\n"
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
"@overload mat2 inverse(mat2 m) = " GLSL(MatrixInverse) ";"         "\n"
"@overload mat3 inverse(mat3 m) = " GLSL(MatrixInverse) ";"         "\n"
"@overload mat4 inverse(mat4 m) = " GLSL(MatrixInverse) ";"         "\n"

//vector relational functions
SRC_LINE
"gbvec(vec) lessThan(vec x, vec y) = " SPV(OpFOrdLessThan) ";" "\n"
"gbvec(ivec) lessThan(ivec x, ivec y) = " SPV(OpSLessThan) ";" "\n"
"gbvec(uvec) lessThan(uvec x, uvec y) = " SPV(OpULessThan) ";" "\n"
"gbvec(vec) lessThanEqual(vec x, vec y) = " SPV(OpFOrdLessThanEqual) ";" "\n"
"gbvec(ivec) lessThanEqual(ivec x, ivec y) = " SPV(OpSLessThanEqual) ";" "\n"
"gbvec(uvec) lessThanEqual(uvec x, uvec y) = " SPV(OpULessThanEqual) ";" "\n"
"gbvec(vec) greaterThan(vec x, vec y) = " SPV(OpFOrdGreaterThan) ";" "\n"
"gbvec(ivec) greaterThan(ivec x, ivec y) = " SPV(OpSGreaterThan) ";" "\n"
"gbvec(uvec) greaterThan(uvec x, uvec y) = " SPV(OpUGreaterThan) ";" "\n"
"gbvec(vec) greaterThanEqual(vec x, vec y) = "SPV(OpFOrdGreaterThanEqual)";""\n"
"gbvec(ivec) greaterThanEqual(ivec x, ivec y) = "SPV(OpUGreaterThanEqual)";""\n"
"gbvec(uvec) greaterThanEqual(uvec x, uvec y) = "SPV(OpSGreaterThanEqual)";""\n"
"gbvec(vec) equal(vec x, vec y) = " SPV(OpFOrdEqual) ";" "\n"
"gbvec(ivec) equal(ivec x, ivec y) = " SPV(OpIEqual) ";" "\n"
"gbvec(uvec) equal(uvec x, uvec y) = " SPV(OpIEqual) ";" "\n"
"gbvec(bvec) equal(bvec x, bvec y) = " SPV(OpLogicalEqual) ";" "\n"
"gbvec(vec) notEqual(vec x, vec y) = " SPV(OpFOrdNotEqual) ";" "\n"
"gbvec(ivec) notEqual(ivec x, ivec y) = " SPV(OpINotEqual) ";" "\n"
"gbvec(uvec) notEqual(uvec x, uvec y) = " SPV(OpINotEqual) ";" "\n"
"gbvec(bvec) notEqual(bvec x, bvec y) = " SPV(OpLogicalNotEqual) ";" "\n"
"bool any(bvec x) = " SPV(OpAny) ";"                 "\n"
"bool all(bvec x) = " SPV(OpAll) ";"                 "\n"
"bvec not(bvec x) = " SPV(OpLogicalNot) ";"          "\n"

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
"genIType bitfieldExtract(genIType value, int offset, int bits) = " SPV(OpBitFieldSExtract) ";" "\n"
"genUType bitfieldExtract(genUType value, int offset, int bits) = " SPV(OpBitFieldUExtract) ";" "\n"
"genIType bitfieldInsert(genIType base, genIType insert,"           "\n"
"                        int offset, int bits) = " SPV(OpBitFieldInsert) ";" "\n"
"genUType bitfieldInsert(genUType base, genUType insert,"           "\n"
"                        int offset, int bits) = " SPV(OpBitFieldInsert) ";" "\n"
"genIType bitfieldReverse(highp genIType value) = " SPV(OpBitReverse) ";" "\n"
"givec(genUType) bitfieldReverse(highp genUType value) = " SPV(OpBitReverse) ";" "\n"
"genIType bitCount(genIType value) = " SPV(OpBitCount) ";"          "\n"
"givec(genUType) bitCount(genUType value) = " SPV(OpBitCount) ";"   "\n"
"genIType findLSB(genIType value);"                                 "\n"
"givec(genUType) findLSB(genUType value);"                          "\n"
"genIType findMSB(highp genIType value);"                           "\n"
"givec(genUType) findMSB(highp genUType value);"                    "\n"
"};"                                                                "\n"
"#undef out"                                                        "\n"
"#undef highp"                                                      "\n"
"#undef lowp"                                                       "\n"
"#undef uvec2"                                                      "\n";

//texture functions
static const char *glsl_texture_size_functions =
SRC_LINE
"#define __sampler(...) @sampler(@image(__VA_ARGS__))"              "\n"
"#define _sampler(...) __sampler(float __VA_OPT__(,) __VA_ARGS__), \\\n"
"                      __sampler(int __VA_OPT__(,) __VA_ARGS__),   \\\n"
"                      __sampler(uint __VA_OPT__(,) __VA_ARGS__)"   "\n"
"@generic(gsamplerLod=[_sampler(1D),   _sampler(1D,Array),"         "\n"
"                      _sampler(2D),   _sampler(2D,Array),"         "\n"
"                      _sampler(Cube), _sampler(Cube,Array),"       "\n"
"                      _sampler(3D),"                               "\n"
"                      _sampler(1D,Depth),   _sampler(1D,Array,Depth),\n"
"                      _sampler(2D,Depth),   _sampler(2D,Array,Depth),\n"
"                      _sampler(Cube,Depth), _sampler(Cube,Array,Depth)],\n"
"         gsampler=[_sampler(2D,MS), _sampler(2D,MS,Array),"        "\n"
"                   _sampler(Rect),  _sampler(Buffer),"             "\n"
"                   _sampler(Rect,Depth)]) {"                      "\n"
"gsamplerLod.size_type textureSize(gsamplerLod sampler, int lod) = " SPV(OpImageQuerySizeLod) ";" "\n"
"gsampler.size_type textureSize(gsampler sampler) = " SPV(OpImageQuerySize) ";" "\n"
"};"                                                                "\n"
"#undef __sampler"                                                  "\n"
"#undef _sampler"                                                   "\n"
;

static const char *glsl_texture_lod_functions =
SRC_LINE
"#define __sampler(...) @sampler(@image(__VA_ARGS__))"              "\n"
"#define _sampler(...) __sampler(float __VA_OPT__(,) __VA_ARGS__), \\\n"
"                      __sampler(int __VA_OPT__(,) __VA_ARGS__),   \\\n"
"                      __sampler(uint __VA_OPT__(,) __VA_ARGS__)"   "\n"
"@generic(gsamplerLod=[_sampler(1D), _sampler(1D,Array),"           "\n"
"                      _sampler(2D), _sampler(2D,Array),"           "\n"
"                      _sampler(Cube), _sampler(Cube,Array),"       "\n"
"                      _sampler(3D),"                               "\n"
"                      _sampler(1D,Depth),   _sampler(1D,Array,Depth),\n"
"                      _sampler(2D,Depth),   _sampler(2D,Array,Depth),\n"
"                      _sampler(Cube,Depth), _sampler(Cube,Array,Depth)]) {\n"
"vec2 textureQueryLod(gsamplerLod sampler, gsamplerLod.lod_coord P) = " SPV(OpImageQueryLod) ";" "\n"
"};"                                                                "\n"
"#undef __sampler"                                                  "\n"
"#undef _sampler"                                                   "\n"
;

static const char *glsl_texture_levels_functions =
SRC_LINE
"#define __sampler(...) @sampler(@image(__VA_ARGS__))"              "\n"
"#define _sampler(...) __sampler(float __VA_OPT__(,) __VA_ARGS__), \\\n"
"                      __sampler(int __VA_OPT__(,) __VA_ARGS__),   \\\n"
"                      __sampler(uint __VA_OPT__(,) __VA_ARGS__)"   "\n"
"@generic(gsamplerLod=[_sampler(1D), _sampler(1D,Array),"           "\n"
"                      _sampler(2D), _sampler(2D,Array),"           "\n"
"                      _sampler(Cube), _sampler(Cube,Array),"       "\n"
"                      _sampler(3D),"                               "\n"
"                      _sampler(1D,Depth),   _sampler(1D,Array,Depth),\n"
"                      _sampler(2D,Depth),   _sampler(2D,Array,Depth),\n"
"                      _sampler(Cube,Depth), _sampler(Cube,Array,Depth)],\n"
"         gsamplerMS=[_sampler(2D,MS), _sampler(2D,MS,Array)]) {"   "\n"
"int textureQueryLevels(gsamplerLod sampler) = " SPV(OpImageQueryLevels) ";" "\n"
"int textureSamples(gsamplerMS sampler) = " SPV(OpImageQuerySamples) ";" "\n"
"};"                                                                "\n"
"#undef __sampler"                                                  "\n"
"#undef _sampler"                                                   "\n"
;

static const char *glsl_other_texture_functions =
SRC_LINE
"#define gvec4 @vector(gsampler.sample_type, 4)"                    "\n"
"#define gtex_coord gsampler.tex_coord"                             "\n"
"#define gshadow_coord gsamplerSh.shadow_coord"                     "\n"
"#define gproj_coord gsampler.proj_coord"                           "\n"
"#define __sampler(...) @sampler(@image(__VA_ARGS__))"              "\n"
"#define _sampler(...) __sampler(float __VA_OPT__(,) __VA_ARGS__), \\\n"
"                      __sampler(int __VA_OPT__(,) __VA_ARGS__),   \\\n"
"                      __sampler(uint __VA_OPT__(,) __VA_ARGS__)"   "\n"
"@generic(gsampler=[_sampler(1D),   _sampler(1D,Array),"            "\n"
"                   _sampler(2D),   _sampler(2D,Array),"            "\n"
"                   _sampler(Cube), _sampler(Cube,Array),"          "\n"
"                   _sampler(3D)],"                                 "\n"
"         gsamplerSh=[__sampler(float,1D,Depth),"                   "\n"
"                     __sampler(float,1D,Array,Depth),"             "\n"
"                     __sampler(float,2D,Depth),"                   "\n"
"                     __sampler(float,2D,Array,Depth),"             "\n"
"                     __sampler(float,Cube,Depth)],"                "\n"
"         gsamplerCAS=[__sampler(float,Cube,Array,Depth)]) {"       "\n"
"gvec4 texture(gsampler sampler, gtex_coord P)"                     "\n"
	"= " SPV(OpImageSampleExplicitLod)                              "\n"
	"[sampler, P, =ImageOperands.Lod, 0f];"                         "\n"
"float texture(gsamplerSh sampler, gshadow_coord P)"                "\n"
	"= " SPV(OpImageSampleDrefExplicitLod)                          "\n"
	"[sampler, [gsamplerSh shadow_coord(P)], [gsamplerSh comp(P)]," "\n"
	" =ImageOperands.Lod, 0f];"                                     "\n"
"float texture(gsamplerCAS sampler, vec4 P, float comp)"            "\n"
	"= " SPV(OpImageSampleDrefExplicitLod) "[sampler, P, comp,"     "\n"
	" =ImageOperands.Lod, 0f];"                                     "\n"
"gvec4 textureProj(gsampler sampler, gproj_coord P)"                "\n"
	"= " SPV(OpImageSampleProjExplicitLod)                          "\n"
	"[sampler, P, =ImageOperands.Lod, 0f];"                         "\n"
"};"                                                                "\n"
"#undef __sampler"                                                  "\n"
"#undef _sampler"                                                   "\n"
"#undef gtex_coord"                                                 "\n"
"#undef gvec4"                                                      "\n"
;

static const char *glsl_frag_texture_functions =
SRC_LINE
"#define gvec4 @vector(gsampler.sample_type, 4)"                    "\n"
"#define gtex_coord gsampler.tex_coord"                             "\n"
"#define gshadow_coord gsamplerSh.shadow_coord"                     "\n"
"#define gproj_coord gsampler.proj_coord"                           "\n"
"#define __sampler(...) @sampler(@image(__VA_ARGS__))"              "\n"
"#define _sampler(...) __sampler(float __VA_OPT__(,) __VA_ARGS__), \\\n"
"                      __sampler(int __VA_OPT__(,) __VA_ARGS__),   \\\n"
"                      __sampler(uint __VA_OPT__(,) __VA_ARGS__)"   "\n"
"@generic(gsampler=[_sampler(1D),   _sampler(1D,Array),"            "\n"
"                   _sampler(2D),   _sampler(2D,Array),"            "\n"
"                   _sampler(Cube), _sampler(Cube,Array),"          "\n"
"                   _sampler(3D)],"                                 "\n"
"         gsamplerSh=[__sampler(float,1D,Depth),"                   "\n"
"                     __sampler(float,1D,Array,Depth),"             "\n"
"                     __sampler(float,2D,Depth),"                   "\n"
"                     __sampler(float,2D,Array,Depth),"             "\n"
"                     __sampler(float,Cube,Depth)],"                "\n"
"         gsamplerCAS=[__sampler(float,Cube,Array,Depth)]) {"       "\n"
"gvec4 texture(gsampler sampler, gtex_coord P, float bias)"         "\n"
	"= " SPV(OpImageSampleImplicitLod)                              "\n"
	"[sampler, P, =ImageOperands.Bias, bias];"                      "\n"
"gvec4 texture(gsampler sampler, gtex_coord P)"                     "\n"
	"= " SPV(OpImageSampleImplicitLod) "[sampler, P];"              "\n"
"float texture(gsamplerSh sampler, gshadow_coord P, float bias)"    "\n"
	"= " SPV(OpImageSampleDrefImplicitLod)                          "\n"
	"[sampler, [gsamplerSh shadow_coord(P)], [gsamplerSh comp(P)]," "\n"
	" =ImageOperands.Bias, bias];"                                  "\n"
"float texture(gsamplerSh sampler, gshadow_coord P)"                "\n"
	"= " SPV(OpImageSampleDrefImplicitLod)                          "\n"
	"[sampler, [gsamplerSh shadow_coord(P)], [gsamplerSh comp(P)]];""\n"
"float texture(gsamplerCAS sampler, vec4 P, float comp)"            "\n"
	"= " SPV(OpImageSampleDrefImplicitLod) "[sampler, P, comp];"    "\n"
"gvec4 textureProj(gsampler sampler, gproj_coord P, float bias)"    "\n"
	"= " SPV(OpImageSampleProjImplicitLod)                          "\n"
	"[sampler, P, =ImageOperands.Bias, bias];"                      "\n"
"gvec4 textureProj(gsampler sampler, gproj_coord P)"                "\n"
	"= " SPV(OpImageSampleProjImplicitLod) "[sampler, P];"          "\n"
"};"                                                                "\n"
"#undef __sampler"                                                  "\n"
"#undef _sampler"                                                   "\n"
"#undef gtex_coord"                                                 "\n"
"#undef gvec4"                                                      "\n"
;

static const char *glsl_common_texture_functions =
SRC_LINE
"#define gvec4 @vector(gsampler.sample_type, 4)"                    "\n"
"#define gvec4B @vector(gsamplerB.sample_type, 4)"                  "\n"
"#define gvec4MS @vector(gsamplerMS.sample_type, 4)"                "\n"
"#define gtex_coord gsampler.tex_coord"                             "\n"
"#define gshadow_coord gsamplerSh.shadow_coord"                     "\n"
"#define gproj_coord gsampler.proj_coord"                           "\n"
"#define __sampler(...) @sampler(@image(__VA_ARGS__))"              "\n"
"#define _sampler(...) __sampler(float __VA_OPT__(,) __VA_ARGS__), \\\n"
"                      __sampler(int __VA_OPT__(,) __VA_ARGS__),   \\\n"
"                      __sampler(uint __VA_OPT__(,) __VA_ARGS__)"   "\n"
"@generic(gsampler=[_sampler(1D),   _sampler(1D,Array),"            "\n"
"                   _sampler(2D),   _sampler(2D,Array),"            "\n"
"                   _sampler(Cube), _sampler(Cube,Array),"          "\n"
"                   _sampler(3D)],"                                 "\n"
"         gsamplerB=[_sampler(Buffer)],"                            "\n"
"         gsamplerMS=[_sampler(2D,MS), _sampler(2D,MS,Array)]) {"   "\n"
"gvec4 texelFetch(gsampler sampler, gtex_coord P, int lod)"         "\n"
	"= " SPV(OpImageFetch) "[sampler, P, =ImageOperands.Lod, lod];" "\n"
"gvec4B texelFetch(gsamplerB sampler, int P)"                       "\n"
	"= " SPV(OpImageFetch) "[sampler, P];"                          "\n"
"gvec4MS texelFetch(gsamplerMS sampler, ivec2 P, int sample)"       "\n"
	"= " SPV(OpImageFetch) "[sampler, P, Sample, sample];"          "\n"
"};"                                                                "\n"
"#undef __sampler"                                                  "\n"
"#undef _sampler"                                                   "\n"
"#undef gproj_coord"                                                "\n"
"#undef gshadow_coord"                                              "\n"
"#undef gtex_coord"                                                 "\n"
"#undef gvec4MS"                                                    "\n"
"#undef gvec4B"                                                     "\n"
"#undef gvec4"                                                      "\n"
"#define gvec4 @vector(gtexture.sample_type, 4)"                    "\n"
"#define gvec4B @vector(gtextureB.sample_type, 4)"                  "\n"
"#define gvec4MS @vector(gtextureMS.sample_type, 4)"                "\n"
"#define gtex_coord gtexture.tex_coord"                             "\n"
"#define gshadow_coord gtextureSh.shadow_coord"                     "\n"
"#define gproj_coord gtexture.proj_coord"                           "\n"
"#define __texture(...) @image(__VA_ARGS__ __VA_OPT__(,) Sampled)"  "\n"
"#define _texture(...) __texture(float __VA_OPT__(,) __VA_ARGS__), \\\n"
"                      __texture(int __VA_OPT__(,) __VA_ARGS__),   \\\n"
"                      __texture(uint __VA_OPT__(,) __VA_ARGS__)"   "\n"
"@generic(gtexture=[_texture(1D), _texture(1D,Array),"              "\n"
"                   _texture(2D), _texture(2D,Array),"              "\n"
"                   _texture(3D)],"                                 "\n"
"         gtextureB=[_texture(Buffer)],"                            "\n"
"         gtextureMS=[_texture(2D,MS), _texture(2D,MS,Array)]) {"   "\n"
"gvec4 texelFetch(gtexture texture, gtex_coord P, int lod)"         "\n"
	"= " SPV(OpImageFetch) "[texture, P, =ImageOperands.Lod, lod];" "\n"
"gvec4B texelFetch(gtextureB texture, int P)"                       "\n"
	"= " SPV(OpImageFetch) "[texture, P];"                          "\n"
"gvec4MS texelFetch(gtextureMS texture, ivec2 P, int sample)"       "\n"
	"= " SPV(OpImageFetch) "[texture, P, Sample, sample];"          "\n"
"};"                                                                "\n"
"#undef __texture"                                                  "\n"
"#undef _texture"                                                   "\n"
"#undef gproj_coord"                                                "\n"
"#undef gshadow_coord"                                              "\n"
"#undef gtex_coord"                                                 "\n"
"#undef gvec4MS"                                                    "\n"
"#undef gvec4B"                                                     "\n"
"#undef gvec4"                                                      "\n"
;
#if 0
"gvec4 textureLod(gsampler1D sampler, float P, float lod)"          "\n"
"gvec4 textureLod(gsampler2D sampler, vec2 P, float lod)"           "\n"
"gvec4 textureLod(gsampler3D sampler, vec3 P, float lod)"           "\n"
"gvec4 textureLod(gsamplerCube sampler, vec3 P, float lod)"         "\n"
"float textureLod(sampler2DShadow sampler, vec3 P, float lod)"      "\n"
"float textureLod(sampler1DShadow sampler, vec3 P, float lod)"      "\n"
"float textureLod(sampler1DArrayShadow sampler, vec3 P, float lod)" "\n"
"gvec4 textureLod(gsampler1DArray sampler, vec2 P, float lod)"      "\n"
"gvec4 textureLod(gsampler2DArray sampler, vec3 P, float lod)"      "\n"
"gvec4 textureLod(gsamplerCubeArray sampler, vec4 P, float lod)"    "\n"

"gvec4 textureOffset(gsampler1D sampler, float P, int offset [, float bias] )" "\n"
"gvec4 textureOffset(gsampler2D sampler, vec2 P, ivec2 offset [, float bias] )" "\n"
"gvec4 textureOffset(gsampler3D sampler, vec3 P, ivec3 offset [, float bias] )" "\n"
"gvec4 textureOffset(gsampler2DRect sampler, vec2 P, ivec2 offset)" "\n"
"float textureOffset(sampler2DShadow sampler, vec3 P, ivec2 offset [, float bias] )" "\n"
"float textureOffset(sampler2DRectShadow sampler, vec3 P, ivec2 offset)" "\n"
"float textureOffset(sampler1DShadow sampler, vec3 P, int offset [, float bias] )" "\n"
"float textureOffset(sampler1DArrayShadow sampler, vec3 P, int offset [, float bias] )" "\n"
"float textureOffset(sampler2DArrayShadow sampler, vec4 P, ivec2 offset)" "\n"
"gvec4 textureOffset(gsampler1DArray sampler, vec2 P, int offset [, float bias] )" "\n"
"gvec4 textureOffset(gsampler2DArray sampler, vec3 P, ivec2 offset [, float bias] )" "\n"


"gvec4 texelFetchOffset(gsampler1D sampler, int P, int lod, int offset)" "\n"
"gvec4 texelFetchOffset(gsampler2D sampler, ivec2 P, int lod, ivec2 offset)" "\n"
"gvec4 texelFetchOffset(gsampler3D sampler, ivec3 P, int lod, ivec3 offset)" "\n"
"gvec4 texelFetchOffset(gsampler2DRect sampler, ivec2 P, ivec2 offset)" "\n"
"gvec4 texelFetchOffset(gsampler1DArray sampler, ivec2 P, int lod, int offset)" "\n"
"gvec4 texelFetchOffset(gsampler2DArray sampler, ivec3 P, int lod, ivec2 offset)" "\n"

"gvec4 textureProjOffset(gsampler1D sampler, vec2 P, int offset [, float bias] )" "\n"
"gvec4 textureProjOffset(gsampler1D sampler, vec4 P, int offset [, float bias] )" "\n"
"gvec4 textureProjOffset(gsampler2D sampler, vec3 P, ivec2 offset [, float bias] )" "\n"
"gvec4 textureProjOffset(gsampler2D sampler, vec4 P, ivec2 offset [, float bias] )" "\n"
"gvec4 textureProjOffset(gsampler3D sampler, vec4 P, ivec3 offset [, float bias] )" "\n"
"gvec4 textureProjOffset(gsampler2DRect sampler, vec3 P, ivec2 offset)" "\n"
"gvec4 textureProjOffset(gsampler2DRect sampler, vec4 P, ivec2 offset)" "\n"
"float textureProjOffset(sampler2DRectShadow sampler, vec4 P, ivec2 offset)" "\n"
"float textureProjOffset(sampler1DShadow sampler, vec4 P, int offset [, float bias] )" "\n"
"float textureProjOffset(sampler2DShadow sampler, vec4 P, ivec2 offset [, float bias] )" "\n"

"gvec4 textureLodOffset(gsampler1D sampler, float P, float lod, int offset)" "\n"
"gvec4 textureLodOffset(gsampler2D sampler, vec2 P, float lod, ivec2 offset)" "\n"
"gvec4 textureLodOffset(gsampler3D sampler, vec3 P, float lod, ivec3 offset)" "\n"
"gvec4 textureLodOffset(gsampler1DArray sampler, vec2 P, float lod, int offset)" "\n"
"gvec4 textureLodOffset(gsampler2DArray sampler, vec3 P, float lod, ivec2 offset)" "\n"
"float textureLodOffset(sampler1DArrayShadow sampler, vec3 P, float lod, int offset)" "\n"
"float textureLodOffset(sampler1DShadow sampler, vec3 P, float lod, int offset)" "\n"
"float textureLodOffset(sampler2DShadow sampler, vec3 P, float lod, ivec2 offset)" "\n"

"gvec4 textureProjLod(gsampler1D sampler, vec2 P, float lod)"       "\n"
"gvec4 textureProjLod(gsampler1D sampler, vec4 P, float lod)"       "\n"
"gvec4 textureProjLod(gsampler2D sampler, vec3 P, float lod)"       "\n"
"gvec4 textureProjLod(gsampler2D sampler, vec4 P, float lod)"       "\n"
"gvec4 textureProjLod(gsampler3D sampler, vec4 P, float lod)"       "\n"
"float textureProjLod(sampler1DShadow sampler, vec4 P, float lod)"  "\n"
"float textureProjLod(sampler2DShadow sampler, vec4 P, float lod)"  "\n"

"gvec4 textureProjLodOffset(gsampler1D sampler, vec2 P, float lod, int offset)" "\n"
"gvec4 textureProjLodOffset(gsampler1D sampler, vec4 P, float lod, int offset)" "\n"
"gvec4 textureProjLodOffset(gsampler2D sampler, vec3 P, float lod, ivec2 offset)" "\n"
"gvec4 textureProjLodOffset(gsampler2D sampler, vec4 P, float lod, ivec2 offset)" "\n"
"gvec4 textureProjLodOffset(gsampler3D sampler, vec4 P, float lod, ivec3 offset)" "\n"
"float textureProjLodOffset(sampler1DShadow sampler, vec4 P, float lod, int offset)" "\n"
"float textureProjLodOffset(sampler2DShadow sampler, vec4 P, float lod, ivec2 offset)" "\n"

"gvec4 textureGrad(gsampler1D sampler, float _P, float dPdx, float dPdy)" "\n"
"gvec4 textureGrad(gsampler2D sampler, vec2 P, vec2 dPdx, vec2 dPdy)" "\n"
"gvec4 textureGrad(gsampler3D sampler, P, vec3 dPdx, vec3 dPdy)"    "\n"
"gvec4 textureGrad(gsamplerCube sampler, vec3 P, vec3 dPdx, vec3 dPdy)" "\n"
"gvec4 textureGrad(gsampler2DRect sampler, vec2 P, vec2 dPdx, vec2 dPdy)" "\n"
"gvec4 textureGrad(gsampler1DArray sampler, vec2 P, float dPdx, float dPdy)" "\n"
"gvec4 textureGrad(gsampler2DArray sampler, vec3 P, vec2 dPdx, vec2 dPdy)" "\n"
"gvec4 textureGrad(gsamplerCubeArray sampler, vec4 P, vec3 dPdx, vec3 dPdy)" "\n"
"float textureGrad(sampler2DRectShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy)" "\n"
"float textureGrad(sampler1DShadow sampler, vec3 P, float dPdx, float dPdy)" "\n"
"float textureGrad(sampler1DArrayShadow sampler, vec3 P, float dPdx, float dPdy)" "\n"
"float textureGrad(sampler2DShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy)" "\n"
"float textureGrad(samplerCubeShadow sampler, vec4 P, vec3 dPdx, vec3 dPdy)" "\n"
"float textureGrad(sampler2DArrayShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)" "\n"

"gvec4 textureGradOffset(gsampler1D sampler, float P, float dPdx, float dPdy, int offset)" "\n"
"gvec4 textureGradOffset(gsampler2D sampler, vec2 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"
"gvec4 textureGradOffset(gsampler3D sampler, vec3 P, vec3 dPdx, vec3 dPdy, ivec3 offset)" "\n"
"gvec4 textureGradOffset(gsampler2DRect sampler, vec2 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"
"gvec4 textureGradOffset(gsampler2DArray sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"
"gvec4 textureGradOffset(gsampler1DArray sampler, vec2 P, float dPdx, float dPdy, int offset)" "\n"

"float textureGradOffset(sampler1DArrayShadow sampler, vec3 P, float dPdx, float dPdy, int offset)" "\n"
"float textureGradOffset(sampler2DArrayShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"
"float textureGradOffset(sampler2DRectShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"
"float textureGradOffset(sampler1DShadow sampler, vec3 P, float dPdx, float dPdy, int offset)" "\n"
"float textureGradOffset(sampler2DShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"

"gvec4 textureProjGrad(gsampler1D sampler, vec2 P, float dPdx, float dPdy)" "\n"
"gvec4 textureProjGrad(gsampler1D sampler, vec4 P, float dPdx, float dPdy)" "\n"
"gvec4 textureProjGrad(gsampler2D sampler, vec3 P, vec2 dPdx, vec2 dPdy)" "\n"
"gvec4 textureProjGrad(gsampler2D sampler, vec4 P, vec2 dPdx, vec2 dPdy)" "\n"
"gvec4 textureProjGrad(gsampler3D sampler, vec4 P, vec3 dPdx, vec3 dPdy)" "\n"
"gvec4 textureProjGrad(gsampler2DRect sampler, vec3 P, vec2 dPdx, vec2 dPdy)" "\n"
"gvec4 textureProjGrad(gsampler2DRect sampler, vec4 P, vec2 dPdx, vec2 dPdy)" "\n"
"float textureProjGrad(sampler2DRectShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)" "\n"
"float textureProjGrad(sampler1DShadow sampler, vec4 P, float dPdx, float dPdy)" "\n"
"float textureProjGrad(sampler2DShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)" "\n"

"gvec4 textureProjGradOffset(gsampler1D sampler, vec2 P, float dPdx, float dPdy, int offset)" "\n"
"gvec4 textureProjGradOffset(gsampler1D sampler, vec4 P, float dPdx, float dPdy, int offset)" "\n"
"gvec4 textureProjGradOffset(gsampler2D sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"
"gvec4 textureProjGradOffset(gsampler2D sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"
"gvec4 textureProjGradOffset(gsampler3D sampler, vec4 P, vec3 dPdx, vec3 dPdy, ivec3 offset)" "\n"
"gvec4 textureProjGradOffset(gsampler2DRect sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"
"gvec4 textureProjGradOffset(gsampler2DRect sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"
"float textureProjGradOffset(sampler2DRectShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"
"float textureProjGradOffset(sampler1DShadow sampler, vec4 P, float dPdx, float dPdy, int offset)" "\n"
"float textureProjGradOffset(sampler2DShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)" "\n"

"//gather"                                                          "\n"
"gvec4 textureGather(gsampler2D sampler, vec2 P [, int comp])"      "\n"
"gvec4 textureGather(gsampler2DArray sampler, vec3 P [, int comp])" "\n"
"gvec4 textureGather(gsamplerCube sampler, vec3 P [, int comp])"    "\n"
"gvec4 textureGather(gsamplerCubeArray sampler, vec4 P[, int comp])""\n"
"gvec4 textureGather(gsampler2DRect sampler, vec2 P[, int comp])"   "\n"
"vec4 textureGather(sampler2DShadow sampler, vec2 P, float refZ)"   "\n"
"vec4 textureGather(sampler2DArrayShadow sampler, vec3 P, float refZ)" "\n"
"vec4 textureGather(samplerCubeShadow sampler, vec3 P, float refZ)" "\n"
"vec4 textureGather(samplerCubeArrayShadow sampler, vec4 P, float refZ)" "\n"
"vec4 textureGather(sampler2DRectShadow sampler, vec2 P, float refZ)" "\n"

"gvec4 textureGatherOffset(gsampler2D sampler, vec2 P, ivec2 offset, [ int comp])" "\n"
"gvec4 textureGatherOffset(gsampler2DArray sampler, vec3 P, ivec2 offset [ int comp])" "\n"
"gvec4 textureGatherOffset(gsampler2DRect sampler, vec2 P, ivec2 offset [ int comp])" "\n"
"vec4 textureGatherOffset(sampler2DShadow sampler, vec2 P, float refZ, ivec2 offset)" "\n"
"vec4 textureGatherOffset(sampler2DArrayShadow sampler, vec3 P, float refZ, ivec2 offset)" "\n"
"vec4 textureGatherOffset(sampler2DRectShadow sampler, vec2 P, float refZ, ivec2 offset)" "\n"

"gvec4 textureGatherOffsets(gsampler2D sampler, vec2 P, ivec2 offsets[4] [, int comp])" "\n"
"gvec4 textureGatherOffsets(gsampler2DArray sampler, vec3 P, ivec2 offsets[4] [, int comp])" "\n"
"gvec4 textureGatherOffsets(gsampler2DRect sampler, vec2 P, ivec2 offsets[4] [, int comp])" "\n"
"vec4 textureGatherOffsets(sampler2DShadow sampler, vec2 P, float refZ, ivec2 offsets[4])" "\n"
"vec4 textureGatherOffsets(sampler2DArrayShadow sampler, vec3 P, float refZ, ivec2 offsets[4])" "\n"
"vec4 textureGatherOffsets(sampler2DRectShadow sampler, vec2 P, float refZ, ivec2 offsets[4])" "\n"
"};"                                                                "\n"
#endif

static const char *glsl_atomic_functions =
SRC_LINE
"#define uintr @reference(uint)"                                        "\n"
"#define intr @reference(int)"                                          "\n"
"#define inout @inout"                                                  "\n"
"@overload uint atomicAdd(uintr mem, const uint data)"                  "\n"
	"= " SPV(OpAtomicIAdd) "[mem, Scope.Device,"                        "\n"
							"MemorySemantics.Relaxed, data];"           "\n"
"@overload int atomicAdd(intr mem, const int data)"                     "\n"
	"= " SPV(OpAtomicIAdd) "[mem, Scope.Device,"                        "\n"
							"MemorySemantics.Relaxed, data];"           "\n"
"@overload uint atomicMin(uintr mem, const uint data)"                  "\n"
	"= " SPV(OpAtomicUMin) "[mem, Scope.Device,"                        "\n"
							"MemorySemantics.Relaxed, data];"           "\n"
"@overload int atomicMin(intr mem, const int data)"                     "\n"
	"= " SPV(OpAtomicSMin) "[mem, Scope.Device,"                        "\n"
							"MemorySemantics.Relaxed, data];"           "\n"
"@overload uint atomicMax(uintr mem, const uint data)"                  "\n"
	"= " SPV(OpAtomicUMax) "[mem, Scope.Device,"                        "\n"
							"MemorySemantics.Relaxed, data];"           "\n"
"@overload int atomicMax(intr mem, const int data)"                     "\n"
	"= " SPV(OpAtomicUMax) "[mem, Scope.Device,"                        "\n"
							"MemorySemantics.Relaxed, data];"           "\n"
"@overload uint atomicAnd(uintr mem, const uint data)"                  "\n"
	"= " SPV(OpAtomicAnd) "[mem, Scope.Device,"                         "\n"
							"MemorySemantics.Relaxed, data];"           "\n"
"@overload int atomicAnd(intr mem, const int data)"                     "\n"
	"= " SPV(OpAtomicAnd) "[mem, Scope.Device,"                         "\n"
						   "MemorySemantics.Relaxed, data];"            "\n"
"@overload uint atomicOr(uintr mem, const uint data)"                   "\n"
	"= " SPV(OpAtomicOr) "[mem, Scope.Device,"                          "\n"
						  "MemorySemantics.Relaxed, data];"             "\n"
"@overload int atomicOr(intr mem, const int data)"                      "\n"
	"= " SPV(OpAtomicOr) "[mem, Scope.Device,"                          "\n"
						  "MemorySemantics.Relaxed, data];"             "\n"
"@overload uint atomicXor(uintr mem, const uint data)"                  "\n"
	"= " SPV(OpAtomicXor) "[mem, Scope.Device,"                         "\n"
						   "MemorySemantics.Relaxed, data];"            "\n"
"@overload int atomicXor(intr mem, const int data)"                     "\n"
	"= " SPV(OpAtomicXor) "[mem, Scope.Device,"                         "\n"
						   "MemorySemantics.Relaxed, data];"            "\n"
"@overload uint atomicExchange(uintr mem, const uint data)"             "\n"
	"= " SPV(OpAtomicExchange) "[mem, Scope.Device,"                    "\n"
								"MemorySemantics.Relaxed, data];"       "\n"
"@overload int atomicExchange(intr mem, const int data)"                "\n"
	"= " SPV(OpAtomicExchange) "[mem, Scope.Device,"                    "\n"
								"MemorySemantics.Relaxed, data];"       "\n"
"@overload uint atomicCompSwap(uintr mem, const uint compare,"          "\n"
							  "const uint data)"                        "\n"
	"= " SPV(OpAtomicCompareExchange) "[mem, Scope.Device,"             "\n"
									   "MemorySemantics.Relaxed, data];""\n"
"@overload int atomicCompSwap(intr mem, const int compare,"             "\n"
							 "const int data)"                          "\n"
	"= " SPV(OpAtomicCompareExchange) "[mem, Scope.Device,"             "\n"
									   "MemorySemantics.Relaxed, data];""\n"
"#undef intr"      "\n"
"#undef uintr"     "\n"
"#undef inout"     "\n";

static const char *glsl_image_functions =
SRC_LINE
"#define readonly"                                                      "\n"
"#define writeonly"                                                     "\n"
"#define __image(...) @image(__VA_ARGS__ __VA_OPT__(,) Storage)"        "\n"
"#define _image(...) __image(float __VA_OPT__(,) __VA_ARGS__), \\"      "\n"
"                    __image(int __VA_OPT__(,) __VA_ARGS__),   \\"      "\n"
"                    __image(uint __VA_OPT__(,) __VA_ARGS__)"           "\n"
"#define gvec4 @vector(gimage.sample_type, 4)"                          "\n"
"#define gvec4MS @vector(gimageMS.sample_type, 4)"                      "\n"
"#define IMAGE_PARAMS gimage image, gimage.image_coord P"               "\n"
"#define IMAGE_PARAMS_MS gimageMS image, gimageMS.image_coord P, int sample" "\n"
"#define PIMAGE_PARAMS @reference(gimage) image, gimage.image_coord P"               "\n"
"#define PIMAGE_PARAMS_MS @reference(gimageMS) image, gimageMS.image_coord P, int sample" "\n"
"@generic(gimage=[_image(1D),   _image(1D,Array),"                      "\n"
"                 _image(2D),   _image(2D,Array),"                      "\n"
"                 _image(Cube), _image(Cube,Array),"                    "\n"
"                 _image(3D),   _image(Rect), _image(Buffer)],"         "\n"
"         gimageMS=[_image(2D,MS), _image(2D,MS,Array)]) {"             "\n"
"gimage.size_type imageSize(readonly writeonly gimage image) = " SPV(OpImageQuerySize) ";" "\n"
"gimageMS.size_type imageSize(readonly writeonly gimageMS image) = " SPV(OpImageQuerySize) ";" "\n"
"int imageSamples(readonly writeonly gimageMS image) = " SPV(OpImageQuerySamples) ";" "\n"
"gvec4 imageLoad(readonly IMAGE_PARAMS) = " SPV(OpImageRead) ";" "\n"
"gvec4MS imageLoad(readonly IMAGE_PARAMS_MS) = " SPV(OpImageRead) ";" "\n"
"void imageStore(writeonly IMAGE_PARAMS, gvec4 data) = " SPV(OpImageWrite) ";" "\n"
"void imageStore(writeonly IMAGE_PARAMS_MS, gvec4MS data) = " SPV(OpImageWrite) ";" "\n"
"@reference(gimage.sample_type, StorageClass.Image) __imageTexel(PIMAGE_PARAMS, int sample) = " SPV(OpImageTexelPointer) ";" "\n"
"@reference(gimageMS.sample_type, StorageClass.Image) __imageTexel(PIMAGE_PARAMS_MS) = " SPV(OpImageTexelPointer) ";" "\n"
"#define __imageAtomic(op,type) \\"                                     "\n"
"type imageAtomic##op(PIMAGE_PARAMS, type data) \\"                     "\n"
"{ \\"                                                                  "\n"
"    return atomic##op(__imageTexel(image, P, 0), data); \\"            "\n"
"} \\"                                                                  "\n"
"type imageAtomic##op(PIMAGE_PARAMS_MS, type data) \\"                  "\n"
"{ \\"                                                                  "\n"
"    return atomic##op(__imageTexel(image, P, sample), data); \\"       "\n"
"}"                                                                     "\n"
"#define imageAtomic(op) \\"                                            "\n"
"__imageAtomic(op,uint) \\"                                             "\n"
"__imageAtomic(op,int)"                                                 "\n"
"imageAtomic(Add)"                                                      "\n"
"imageAtomic(Min)"                                                      "\n"
"imageAtomic(Max)"                                                      "\n"
"imageAtomic(And)"                                                      "\n"
"imageAtomic(Or)"                                                       "\n"
"imageAtomic(Xor)"                                                      "\n"
"__imageAtomic(Exchange, float)"                                        "\n"
"__imageAtomic(Exchange, uint)"                                         "\n"
"__imageAtomic(Exchange, int)"                                          "\n"
"#define __imageAtomicCompSwap(type) \\"                                "\n"
"type imageAtomicCompSwap(PIMAGE_PARAMS, type data) \\"                 "\n"
"{ \\"                                                                  "\n"
"    return atomicCompSwap(__imageTexel(image, P, 0), data); \\"        "\n"
"} \\"                                                                  "\n"
"type imageAtomicCompSwap(PIMAGE_PARAMS_MS, type data) \\"              "\n"
"{ \\"                                                                  "\n"
"    return atomicCompSwap(__imageTexel(image, P, sample), data); \\"   "\n"
"}"                                                                     "\n"
"__imageAtomicCompSwap(uint)"                                           "\n"
"__imageAtomicCompSwap(int)"                                            "\n"
"};" "\n"
"#undef gvec4"          "\n"
"#undef gvec4MS"        "\n"
"#undef IMAGE_PARAMS"   "\n"
"#undef IMAGE_PARAMS_MS""\n"
"#undef _image"         "\n"
"#undef __image"        "\n"
"#undef readonly"       "\n"
"#undef writeonly"      "\n";

//geometry shader functions
static const char *glsl_geometry_functions =
SRC_LINE
"void EmitStreamVertex(int stream) = " SPV(OpEmitStreamVertex) ";"      "\n"
"void EndStreamPrimitive(int stream) = " SPV(OpEndStreamPrimitive) ";"  "\n"
"void EmitVertex() = " SPV(OpEmitVertex) ";"                            "\n"
"void EndPrimitive() = " SPV(OpEndPrimitive) ";"                        "\n";

//fragment processing functions
static const char *glsl_fragment_functions =
SRC_LINE
"void __discard() = " SPV(OpKill) ";"                                   "\n"
"@generic(genFType=@vector(float)) {"                                   "\n"
"genFType dFdx(genFType p) = " SPV(OpDPdx) ";"                          "\n"
"genFType dFdy(genFType p) = " SPV(OpDPdy) ";"                          "\n"
"genFType dFdxFine(genFType p) = " SPV(OpDPdxFine) ";"                  "\n"
"genFType dFdyFine(genFType p) = " SPV(OpDPdyFine) ";"                  "\n"
"genFType dFdxCoarse(genFType p) = " SPV(OpDPdxCoarse) ";"              "\n"
"genFType dFdyCoarse(genFType p) = " SPV(OpDPdyCoarse) ";"              "\n"
"genFType fwidth(genFType p) = " SPV(OpFwidth) ";"                      "\n"
"genFType fwidthFine(genFType p) = " SPV(OpFwidthFine) ";"              "\n"
"genFType fwidthCoarse(genFType p) = " SPV(OpFwidthCoarse) ";"          "\n"
"};" "\n"

#if 0
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
#endif
//subpass-input functions
SRC_LINE
"#define readonly"                                                      "\n"
"#define writeonly"                                                     "\n"
"#define __spI(...) @image(__VA_ARGS__ __VA_OPT__(,) SubpassData)"      "\n"
"#define _subpassInput(...) __spI(float __VA_OPT__(,) __VA_ARGS__),    \\\n"
"                           __spI(int __VA_OPT__(,) __VA_ARGS__),      \\\n"
"                           __spI(uint __VA_OPT__(,) __VA_ARGS__)"      "\n"
"#define gvec4 @vector(gsubpassInput.sample_type, 4)"                   "\n"
"#define gvec4MS @vector(gsubpassInputMS.sample_type, 4)"               "\n"
"@generic(gsubpassInput=[_subpassInput()],"                             "\n"
"         gsubpassInputMS=[_subpassInput(MS)]) {"                       "\n"
"gvec4 subpassLoad(gsubpassInput subpass)"                              "\n"
	"= " SPV(OpImageRead)                                               "\n"
	"[subpass, @construct(gsubpassInput.image_coord, 0)];"              "\n"
"gvec4MS subpassLoad(gsubpassInputMS subpass)"                          "\n"
	"= " SPV(OpImageRead)                                               "\n"
	"[subpass, @construct(gsubpassInputMS.image_coord, 0), sample];"    "\n"
"};"                                                                    "\n"
"#undef readonly"       "\n"
"#undef writeonly"      "\n"
"#undef gvec4"          "\n"
"#undef gvec4MS"        "\n"
"#undef _subpassInput"  "\n"
"#undef __spI"          "\n"
;
#if 0
//shader invocation group functions
bool anyInvocation(bool value)
bool allInvocations(bool value)
bool allInvocationsEqual(bool value)
#endif

void
glsl_multiview (int behavior, void *scanner)
{
	if (behavior) {
		spirv_add_capability (pr.module, SpvCapabilityMultiView);
		rua_parse_define ("GL_EXT_multiview 1\n");
	} else {
		rua_undefine ("GL_EXT_multiview", scanner);
	}
}

static int glsl_include_state = 0;

bool
glsl_on_include (const char *name, rua_ctx_t *ctx)
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
glsl_parse_vars (const char *var_src, rua_ctx_t *ctx)
{
	glsl_parse_string (var_src, ctx);
}

static void
glsl_init_common (rua_ctx_t *ctx)
{
	glsl_sublang = *(glsl_sublang_t *) ctx->language->sublanguage;

	current_target.create_entry_point ("main", glsl_sublang.model_name);

	image_init_types ();

	ctx->language->initialized = true;
	block_clear ();
	rua_ctx_t rua_ctx = { .language = &lang_ruamoko, .sub_parse = true };
	qc_parse_string (glsl_general_functions, &rua_ctx);
	qc_parse_string (glsl_texture_size_functions, &rua_ctx);
	qc_parse_string (glsl_texture_lod_functions, &rua_ctx);
	qc_parse_string (glsl_texture_levels_functions, &rua_ctx);
	qc_parse_string (glsl_common_texture_functions, &rua_ctx);
	qc_parse_string (glsl_atomic_functions, &rua_ctx);
	qc_parse_string (glsl_image_functions, &rua_ctx);
	glsl_parse_vars (glsl_system_constants, ctx);
}

void
glsl_init_comp (rua_ctx_t *ctx)
{
	glsl_init_common (ctx);
	glsl_parse_vars (glsl_compute_vars, ctx);
}

void
glsl_init_vert (rua_ctx_t *ctx)
{
	glsl_init_common (ctx);
	glsl_parse_vars (glsl_Vulkan_vertex_vars, ctx);
	rua_ctx_t rua_ctx = { .language = &lang_ruamoko };
	qc_parse_string (glsl_other_texture_functions, &rua_ctx);
}

void
glsl_init_tesc (rua_ctx_t *ctx)
{
	glsl_init_common (ctx);
	glsl_parse_vars (glsl_tesselation_control_vars, ctx);
	rua_ctx_t rua_ctx = { .language = &lang_ruamoko };
	qc_parse_string (glsl_other_texture_functions, &rua_ctx);

	spirv_add_capability (pr.module, SpvCapabilityTessellation);
}

void
glsl_init_tese (rua_ctx_t *ctx)
{
	glsl_init_common (ctx);
	glsl_parse_vars (glsl_tesselation_evaluation_vars, ctx);
	rua_ctx_t rua_ctx = { .language = &lang_ruamoko };
	qc_parse_string (glsl_other_texture_functions, &rua_ctx);

	spirv_add_capability (pr.module, SpvCapabilityTessellation);
}

void
glsl_init_geom (rua_ctx_t *ctx)
{
	glsl_init_common (ctx);
	glsl_parse_vars (glsl_geometry_vars, ctx);
	rua_ctx_t rua_ctx = { .language = &lang_ruamoko };
	qc_parse_string (glsl_geometry_functions, &rua_ctx);
	qc_parse_string (glsl_other_texture_functions, &rua_ctx);

	spirv_add_capability (pr.module, SpvCapabilityGeometry);
}

void
glsl_init_frag (rua_ctx_t *ctx)
{
	glsl_init_common (ctx);
	glsl_parse_vars (glsl_fragment_vars, ctx);
	rua_ctx_t rua_ctx = { .language = &lang_ruamoko };
	qc_parse_string (glsl_fragment_functions, &rua_ctx);
	qc_parse_string (glsl_frag_texture_functions, &rua_ctx);
}
