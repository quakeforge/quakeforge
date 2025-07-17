#ifndef __texture_h
#define __texture_h

#define SPV(op) @intrinsic(op)

#define gvec4 @vector(gsampler.sample_type, 4)
#define gtex_coord gsampler.tex_coord
#define gshadow_coord gsamplerSh.shadow_coord
#define gproj_coord gsampler.proj_coord
#define __sampler(...) @sampler(@image(__VA_ARGS__))
#define _sampler(...) __sampler(float __VA_OPT__(,) __VA_ARGS__), \
                      __sampler(int __VA_OPT__(,) __VA_ARGS__),   \
                      __sampler(uint __VA_OPT__(,) __VA_ARGS__)
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
gvec4 texture(gsampler sampler, gtex_coord P, float bias)
	= SPV(OpImageSampleImplicitLod) [sampler, P, =ImageOperands.Bias, bias];
gvec4 texture(gsampler sampler, gtex_coord P)
	= SPV(OpImageSampleImplicitLod)[sampler, P];
float texture(gsamplerSh sampler, gshadow_coord P, float bias)
	= SPV(OpImageSampleDrefImplicitLod)
		[sampler, [gsamplerSh shadow_coord(P)], [gsamplerSh comp(P)],
		 =ImageOperands.Bias, bias];
float texture(gsamplerSh sampler, gshadow_coord P)
	= SPV(OpImageSampleDrefImplicitLod)
		[sampler, [gsamplerSh shadow_coord(P)], [gsamplerSh comp(P)]];
float texture(gsamplerCAS sampler, vec4 P, float comp)
	= SPV(OpImageSampleDrefImplicitLod)[sampler, P, comp];
gvec4 textureProj(gsampler sampler, gproj_coord P, float bias)
	= SPV(OpImageSampleProjImplicitLod) [sampler, P, =ImageOperands.Bias, bias];
gvec4 textureProj(gsampler sampler, gproj_coord P)
	= SPV(OpImageSampleProjImplicitLod)[sampler, P];
};
#undef __sampler
#undef _sampler
#undef gtex_coord
#undef gvec4
#undef SPV

#endif//__texture_h
