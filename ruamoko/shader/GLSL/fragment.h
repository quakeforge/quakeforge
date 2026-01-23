#ifndef __qfcc_shader_glsl_fragment_h
#define __qfcc_shader_glsl_fragment_h

#ifndef __GLSL__
#include "_defines.h"
#endif

//fragment processing functions
void __discard() =  SPV(OpKill);
@generic(genFType=@vector(float)) {
genFType dFdx(genFType p) =  SPV(OpDPdx);
genFType dFdy(genFType p) =  SPV(OpDPdy);
genFType dFdxFine(genFType p) =  SPV(OpDPdxFine);
genFType dFdyFine(genFType p) =  SPV(OpDPdyFine);
genFType dFdxCoarse(genFType p) =  SPV(OpDPdxCoarse);
genFType dFdyCoarse(genFType p) =  SPV(OpDPdyCoarse);
genFType fwidth(genFType p) =  SPV(OpFwidth);
genFType fwidthFine(genFType p) =  SPV(OpFwidthFine);
genFType fwidthCoarse(genFType p) =  SPV(OpFwidthCoarse);
};

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
#endif

//subpass-input functions
@generic(gsubpassInput=[_subpassInput()],
         gsubpassInputMS=[_subpassInput(MS)]) {
@vector(gsubpassInput.sample_type, 4)
	subpassLoad(gsubpassInput subpass)
		=  SPV(OpImageRead)
		[subpass, @construct(gsubpassInput.image_coord, 0)];
@vector(gsubpassInputMS.sample_type, 4)
	subpassLoad(gsubpassInputMS subpass)
		=  SPV(OpImageRead)
		[subpass, @construct(gsubpassInputMS.image_coord, 0), sample];
};

#endif//__qfcc_shader_glsl_fragment_h
