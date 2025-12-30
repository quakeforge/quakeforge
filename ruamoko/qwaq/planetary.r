#define GLSL(op) @intrinsic(OpExtInst, "GLSL.std.450", op)
#define SPV(op) @intrinsic(op)

@generic(genFType=@vector(float),
		 genDType=@vector(double),
		 genIType=@vector(int),
		 genUType=@vector(uint),
		 genBType=@vector(bool),
		 mat=@matrix(float),
		 vec=[vec2,vec3,vec4,dvec2,dvec3,dvec4],
		 ivec=[ivec2,ivec3,ivec4],
		 uvec=[uvec2,uvec3,uvec4],
		 bvec=[bvec2,bvec3,bvec4]) {
genFType atan(genFType y, genFType x) = GLSL(Atan2);
genFType atan(genFType y_over_x) = GLSL(Atan);
float length(genFType x) = GLSL(Length);
genFType sign(genFType x) = GLSL(FSign);
genIType sign(genIType x) = GLSL(SSign);
genDType sign(genDType x) = GLSL(FSign);
genFType dFdx(genFType p) = SPV(OpDPdx);
genFType dFdy(genFType p) = SPV(OpDPdy);
genFType dFdxFine(genFType p) = SPV(OpDPdxFine);
genFType dFdyFine(genFType p) = SPV(OpDPdyFine);
genFType dFdxCoarse(genFType p) = SPV(OpDPdxCoarse);
genFType dFdyCoarse(genFType p) = SPV(OpDPdyCoarse);
genFType fwidth(genFType p) = SPV(OpFwidth);
genFType normalize(genFType x) =  GLSL(Normalize);
genDType normalize(genDType x) =  GLSL(Normalize);
};

#define highp
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
vec4 unpackUnorm4x8(highp uint p) = GLSL(UnpackUnorm4x8);
vec4 unpackSnorm4x8(highp uint p) = GLSL(UnpackSnorm4x8);
#undef highp
#undef __sampler
#undef _sampler
#undef gtex_coord
#undef gvec4
#undef SPV

[uniform, set(1), binding(0)] @sampler(@image(float,2D)) Palette;
[uniform, set(3), binding(0)] @sampler(@image(float,2D,Array)) SurfMap;

[push_constant] @block PushConstants {
	mat4  Model;
	uint  enabled_mask;
	float blend;
	uint  debug_bone;
	uint  colors;
	vec4  base_color;
	vec4  fog;
};

vec4
surf_map (vec3 dir, int layer)
{
	const float pi = 3.14159265358979;
	// equirectangular images go from left to right, top to bottom, but
	// the computed angles go right to left, bottom to top, so need to flip
	// vertical for the maps to be the right way round.
	const vec2 conv = vec2(1/(2*pi), -1/pi);

	vec3 rid = -dir;
	float x1 = atan (dir.y, dir.x);
	float x2 = atan (rid.y, rid.x);
	float y = atan (dir.z, length(dir.xy));
	vec2 uv1 = vec2 (x1, y) * conv + vec2(0.5, 0.5);
	vec2 uv2 = vec2 (x2, y) * conv + vec2(0.0, 0.5);
	//FIXME mipmaps mess this up
	vec2 uv = fwidth(uv1.x) > 0.5 ? uv2: uv1;
	return texture (SurfMap, vec3(uv, layer));
}

[in(0)] vec2 uv;
[in(1)] vec4 position;
[in(2)] vec3 normal;
[in(3)] vec4 color;

[out(0)] vec4 frag_color;
[out(1)] vec4 frag_emission;
[out(2)] vec4 frag_normal;

[shader("Fragment")]
[capability("MultiView")]
void
main ()
{
	auto dir = (position - Model[3]).xyz;
	auto local_dir = vec3 (
		dir • normalize(Model[0].xyz),
		dir • normalize(Model[1].xyz),
		dir • normalize(Model[2].xyz)
	);
	vec4 c, e;
	c = surf_map(local_dir, 0);
	e = surf_map(local_dir, 1) * 2;
	vec4 rows = unpackUnorm4x8(colors);
	vec4        cmap = surf_map (dir, 2);
	c += texture (Palette, vec2 (cmap.x, rows.x)) * cmap.y;
	c += texture (Palette, vec2 (cmap.z, rows.y)) * cmap.w;
	frag_color = c * color;
	frag_emission = e;
	frag_normal = vec4(normalize(dir), 1);
}
