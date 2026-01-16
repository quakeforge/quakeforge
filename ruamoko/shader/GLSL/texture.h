#ifndef __qfcc_shader_glsl_texture_h
#define __qfcc_shader_glsl_texture_h

#ifndef __GLSL__
#include "_defines.h"
#endif

// texture size functions
@generic(gsamplerLod=[_sampler(1D),   _sampler(1D,Array),
                      _sampler(2D),   _sampler(2D,Array),
                      _sampler(Cube), _sampler(Cube,Array),
                      _sampler(3D),
                      _sampler(1D,Depth),   _sampler(1D,Array,Depth),
                      _sampler(2D,Depth),   _sampler(2D,Array,Depth),
                      _sampler(Cube,Depth), _sampler(Cube,Array,Depth)],
         gsampler=[_sampler(2D,MS), _sampler(2D,MS,Array),
                   _sampler(Rect),  _sampler(Buffer),
                   _sampler(Rect,Depth)]) {
gsamplerLod.size_type textureSize(gsamplerLod sampler, int lod)
	= SPV(OpImageQuerySizeLod);
gsampler.size_type textureSize(gsampler sampler) = SPV(OpImageQuerySize);
};

// texture lod functions
@generic(gsamplerLod=[_sampler(1D), _sampler(1D,Array),
                      _sampler(2D), _sampler(2D,Array),
                      _sampler(Cube), _sampler(Cube,Array),
                      _sampler(3D),
                      _sampler(1D,Depth),   _sampler(1D,Array,Depth),
                      _sampler(2D,Depth),   _sampler(2D,Array,Depth),
                      _sampler(Cube,Depth), _sampler(Cube,Array,Depth)]) {
vec2 textureQueryLod(gsamplerLod sampler, gsamplerLod.lod_coord P) = SPV(OpImageQueryLod);
};

// texture levels functions
@generic(gsamplerLod=[_sampler(1D), _sampler(1D,Array),
                      _sampler(2D), _sampler(2D,Array),
                      _sampler(Cube), _sampler(Cube,Array),
                      _sampler(3D),
                      _sampler(1D,Depth),   _sampler(1D,Array,Depth),
                      _sampler(2D,Depth),   _sampler(2D,Array,Depth),
                      _sampler(Cube,Depth), _sampler(Cube,Array,Depth)],
         gsamplerMS=[_sampler(2D,MS), _sampler(2D,MS,Array)]) {
int textureQueryLevels(gsamplerLod sampler) = SPV(OpImageQueryLevels);
int textureSamples(gsamplerMS sampler) = SPV(OpImageQuerySamples);
};

// other texture functions
#ifdef __GLSL_FRAGMENT__
// frag texture functions
@generic(gsampler=[_sampler(1D),   _sampler(1D,Array),
                   _sampler(2D),   _sampler(2D,Array),
                   _sampler(Cube), _sampler(Cube,Array),
                   _sampler(3D)],
         gsamplerSh=[__sampler(float,1D,Depth),
                     __sampler(float,1D,Array,Depth),
                     __sampler(float,2D,Depth),
                     __sampler(float,2D,Array,Depth),
                     __sampler(float,Cube,Depth)],
         gsamplerCAS=[__sampler(float,Cube,Array,Depth)]) {
@vector(gsampler.sample_type, 4)
	texture(gsampler sampler, gsampler.tex_coord P, float bias)
		= SPV(OpImageSampleImplicitLod)
		[sampler, P, =ImageOperands.Bias, bias];
@vector(gsampler.sample_type, 4)
	texture(gsampler sampler, gsampler.tex_coord P)
		= SPV(OpImageSampleImplicitLod) [sampler, P];
float texture(gsamplerSh sampler, gsamplerSh.shadow_coord P, float bias)
	= SPV(OpImageSampleDrefImplicitLod)
	[sampler, [gsamplerSh shadow_coord(P)], [gsamplerSh comp(P)],
	 =ImageOperands.Bias, bias];
float texture(gsamplerSh sampler, gsamplerSh.shadow_coord P)
	= SPV(OpImageSampleDrefImplicitLod)
	[sampler, [gsamplerSh shadow_coord(P)], [gsamplerSh comp(P)]];
float texture(gsamplerCAS sampler, vec4 P, float comp)
	= SPV(OpImageSampleDrefImplicitLod) [sampler, P, comp];
@vector(gsampler.sample_type, 4)
	textureProj(gsampler sampler, gsampler.proj_coord P, float bias)
		= SPV(OpImageSampleProjImplicitLod)
		[sampler, P, =ImageOperands.Bias, bias];
@vector(gsampler.sample_type, 4)
	textureProj(gsampler sampler, gsampler.proj_coord P)
		= SPV(OpImageSampleProjImplicitLod) [sampler, P];
};
#else
@generic(gsampler=[_sampler(1D),   _sampler(1D,Array),
                   _sampler(2D),   _sampler(2D,Array),
                   _sampler(Cube), _sampler(Cube,Array),
                   _sampler(3D)],
         gsamplerSh=[__sampler(float,1D,Depth),
                     __sampler(float,1D,Array,Depth),
                     __sampler(float,2D,Depth),
                     __sampler(float,2D,Array,Depth),
                     __sampler(float,Cube,Depth)],
         gsamplerCAS=[__sampler(float,Cube,Array,Depth)]) {
@vector(gsampler.sample_type, 4)
	texture(gsampler sampler, gsampler.tex_coord P)
		= SPV(OpImageSampleExplicitLod)
		[sampler, P, =ImageOperands.Lod, 0f];
float texture(gsamplerSh sampler, gsamplerSh.shadow_coord P)
	= SPV(OpImageSampleDrefExplicitLod)
	[sampler, [gsamplerSh shadow_coord(P)], [gsamplerSh comp(P)],
	 =ImageOperands.Lod, 0f];
float texture(gsamplerCAS sampler, vec4 P, float comp)
	= SPV(OpImageSampleDrefExplicitLod) [sampler, P, comp,
	 =ImageOperands.Lod, 0f];
@vector(gsampler.sample_type, 4)
	textureProj(gsampler sampler, gsampler.proj_coord P)
		= SPV(OpImageSampleProjExplicitLod)
		[sampler, P, =ImageOperands.Lod, 0f];
};
#endif

// common texture functions
@generic(gsampler=[_sampler(1D),   _sampler(1D,Array),
                   _sampler(2D),   _sampler(2D,Array),
                   _sampler(Cube), _sampler(Cube,Array),
                   _sampler(3D)],
         gsamplerB=[_sampler(Buffer)],
         gsamplerMS=[_sampler(2D,MS), _sampler(2D,MS,Array)]) {
@vector(gsampler.sample_type, 4)
	texelFetch(gsampler sampler, gsampler.tex_coord P, int lod)
		= SPV(OpImageFetch) [sampler, P, =ImageOperands.Lod, lod];
@vector(gsamplerB.sample_type, 4)
	texelFetch(gsamplerB sampler, int P)
		= SPV(OpImageFetch) [sampler, P];
@vector(gsamplerMS.sample_type, 4)
	texelFetch(gsamplerMS sampler, ivec2 P, int sample)
		= SPV(OpImageFetch) [sampler, P, Sample, sample];
};

@generic(gtexture=[_texture(1D), _texture(1D,Array),
                   _texture(2D), _texture(2D,Array),
                   _texture(3D)],
         gtextureB=[_texture(Buffer)],
         gtextureMS=[_texture(2D,MS), _texture(2D,MS,Array)]) {
@vector(gtexture.sample_type, 4)
	texelFetch(gtexture texture, gtexture.tex_coord P, int lod)
		= SPV(OpImageFetch) [texture, P, =ImageOperands.Lod, lod];
@vector(gtextureB.sample_type, 4)
	texelFetch(gtextureB texture, int P)
		= SPV(OpImageFetch) [texture, P];
@vector(gtextureMS.sample_type, 4)
	texelFetch(gtextureMS texture, ivec2 P, int sample)
		= SPV(OpImageFetch) [texture, P, Sample, sample];
};

#if 0
gvec4 textureLod(gsampler1D sampler, float P, float lod)
gvec4 textureLod(gsampler2D sampler, vec2 P, float lod)
gvec4 textureLod(gsampler3D sampler, vec3 P, float lod)
gvec4 textureLod(gsamplerCube sampler, vec3 P, float lod)
float textureLod(sampler2DShadow sampler, vec3 P, float lod)
float textureLod(sampler1DShadow sampler, vec3 P, float lod)
float textureLod(sampler1DArrayShadow sampler, vec3 P, float lod)
gvec4 textureLod(gsampler1DArray sampler, vec2 P, float lod)
gvec4 textureLod(gsampler2DArray sampler, vec3 P, float lod)
gvec4 textureLod(gsamplerCubeArray sampler, vec4 P, float lod)

gvec4 textureOffset(gsampler1D sampler, float P, int offset [, float bias] )
gvec4 textureOffset(gsampler2D sampler, vec2 P, ivec2 offset [, float bias] )
gvec4 textureOffset(gsampler3D sampler, vec3 P, ivec3 offset [, float bias] )
gvec4 textureOffset(gsampler2DRect sampler, vec2 P, ivec2 offset)
float textureOffset(sampler2DShadow sampler, vec3 P, ivec2 offset [, float bias] )
float textureOffset(sampler2DRectShadow sampler, vec3 P, ivec2 offset)
float textureOffset(sampler1DShadow sampler, vec3 P, int offset [, float bias] )
float textureOffset(sampler1DArrayShadow sampler, vec3 P, int offset [, float bias] )
float textureOffset(sampler2DArrayShadow sampler, vec4 P, ivec2 offset)
gvec4 textureOffset(gsampler1DArray sampler, vec2 P, int offset [, float bias] )
gvec4 textureOffset(gsampler2DArray sampler, vec3 P, ivec2 offset [, float bias] )


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
gvec4 textureLodOffset(gsampler1DArray sampler, vec2 P, float lod, int offset)
gvec4 textureLodOffset(gsampler2DArray sampler, vec3 P, float lod, ivec2 offset)
float textureLodOffset(sampler1DArrayShadow sampler, vec3 P, float lod, int offset)
float textureLodOffset(sampler1DShadow sampler, vec3 P, float lod, int offset)
float textureLodOffset(sampler2DShadow sampler, vec3 P, float lod, ivec2 offset)

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
gvec4 textureGrad(gsampler1DArray sampler, vec2 P, float dPdx, float dPdy)
gvec4 textureGrad(gsampler2DArray sampler, vec3 P, vec2 dPdx, vec2 dPdy)
gvec4 textureGrad(gsamplerCubeArray sampler, vec4 P, vec3 dPdx, vec3 dPdy)
float textureGrad(sampler2DRectShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy)
float textureGrad(sampler1DShadow sampler, vec3 P, float dPdx, float dPdy)
float textureGrad(sampler1DArrayShadow sampler, vec3 P, float dPdx, float dPdy)
float textureGrad(sampler2DShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy)
float textureGrad(samplerCubeShadow sampler, vec4 P, vec3 dPdx, vec3 dPdy)
float textureGrad(sampler2DArrayShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)

gvec4 textureGradOffset(gsampler1D sampler, float P, float dPdx, float dPdy, int offset)
gvec4 textureGradOffset(gsampler2D sampler, vec2 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
gvec4 textureGradOffset(gsampler3D sampler, vec3 P, vec3 dPdx, vec3 dPdy, ivec3 offset)
gvec4 textureGradOffset(gsampler2DRect sampler, vec2 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
gvec4 textureGradOffset(gsampler2DArray sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
gvec4 textureGradOffset(gsampler1DArray sampler, vec2 P, float dPdx, float dPdy, int offset)

float textureGradOffset(sampler1DArrayShadow sampler, vec3 P, float dPdx, float dPdy, int offset)
float textureGradOffset(sampler2DArrayShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
float textureGradOffset(sampler2DRectShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)
float textureGradOffset(sampler1DShadow sampler, vec3 P, float dPdx, float dPdy, int offset)
float textureGradOffset(sampler2DShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)

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
gvec4 textureGatherOffset(gsampler2DRect sampler, vec2 P, ivec2 offset [ int comp])
vec4 textureGatherOffset(sampler2DShadow sampler, vec2 P, float refZ, ivec2 offset)
vec4 textureGatherOffset(sampler2DArrayShadow sampler, vec3 P, float refZ, ivec2 offset)
vec4 textureGatherOffset(sampler2DRectShadow sampler, vec2 P, float refZ, ivec2 offset)

gvec4 textureGatherOffsets(gsampler2D sampler, vec2 P, ivec2 offsets[4] [, int comp])
gvec4 textureGatherOffsets(gsampler2DArray sampler, vec3 P, ivec2 offsets[4] [, int comp])
gvec4 textureGatherOffsets(gsampler2DRect sampler, vec2 P, ivec2 offsets[4] [, int comp])
vec4 textureGatherOffsets(sampler2DShadow sampler, vec2 P, float refZ, ivec2 offsets[4])
vec4 textureGatherOffsets(sampler2DArrayShadow sampler, vec3 P, float refZ, ivec2 offsets[4])
vec4 textureGatherOffsets(sampler2DRectShadow sampler, vec2 P, float refZ, ivec2 offsets[4])
};
#endif

#endif//__qfcc_shader_glsl_texture_h
