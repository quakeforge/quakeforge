[in("FragCoord")] vec4 gl_FragCoord;

#define SPV(op) @intrinsic(op)
#define GLSL(op) @intrinsic(OpExtInst,"GLSL.std.450",op)

@generic(genFType = @vector(float)) {
genFType min(genFType x, genFType y) = GLSL(FMin);
genFType max(genFType x, genFType y) = GLSL(FMax);
float length(genFType x) = GLSL(Length);
genFType lerp(genFType x, genFType y, genFType a) = GLSL(FMix);
genFType lerp(genFType x, genFType y, float a)
	= GLSL(FMix) [x, y, @construct (genFType, a)];

genFType clamp(genFType x, genFType y, genFType a) = GLSL(FMix);
genFType clamp(genFType x, float minVal, float maxVal)
    = GLSL(FClamp) [x, @construct (genFType, minVal),
					   @construct (genFType, maxVal)];
};

#define readonly
#define writeonly
#define __image(...) @image(__VA_ARGS__ __VA_OPT__(,) Storage)
#define _image(...) __image(float __VA_OPT__(,) __VA_ARGS__), \
                    __image(int __VA_OPT__(,) __VA_ARGS__),   \
                    __image(uint __VA_OPT__(,) __VA_ARGS__)
#define gvec4 @vector(gimage.sample_type, 4)
#define gvec4MS @vector(gimageMS.sample_type, 4)
#define IMAGE_PARAMS gimage image, gimage.image_coord P
#define IMAGE_PARAMS_MS gimageMS image, gimageMS.image_coord P, int sample
#define PIMAGE_PARAMS @reference(gimage) image, gimage.image_coord P
#define PIMAGE_PARAMS_MS @reference(gimageMS) image, gimageMS.image_coord P, int sample
@generic(gimage=[_image(1D),   _image(1D,Array),
                 _image(2D),   _image(2D,Array),
                 _image(Cube), _image(Cube,Array),
                 _image(3D),   _image(Rect), _image(Buffer)],
         gimageMS=[_image(2D,MS), _image(2D,MS,Array)]) {
gimage.size_type imageSize(readonly writeonly gimage image) = SPV(OpImageQuerySize);
gimageMS.size_type imageSize(readonly writeonly gimageMS image) = SPV(OpImageQuerySize);
int imageSamples(readonly writeonly gimageMS image) = SPV(OpImageQuerySamples);
gvec4 imageLoad(readonly IMAGE_PARAMS) = SPV(OpImageRead);
gvec4MS imageLoad(readonly IMAGE_PARAMS_MS) = SPV(OpImageRead);
void imageStore(writeonly IMAGE_PARAMS, gvec4 data) = SPV(OpImageWrite);
void imageStore(writeonly IMAGE_PARAMS_MS, gvec4MS data) = SPV(OpImageWrite);
@reference(gimage.sample_type, StorageClass.Image) __imageTexel(PIMAGE_PARAMS, int sample) = SPV(OpImageTexelPointer);
@reference(gimageMS.sample_type, StorageClass.Image) __imageTexel(PIMAGE_PARAMS_MS) = SPV(OpImageTexelPointer);
#define __imageAtomic(op,type) \
type imageAtomic##op(PIMAGE_PARAMS, type data) \
{ \
    return atomic##op(__imageTexel(image, P, 0), data); \
} \
type imageAtomic##op(PIMAGE_PARAMS_MS, type data) \
{ \
    return atomic##op(__imageTexel(image, P, sample), data); \
}
#define imageAtomic(op) \
__imageAtomic(op,uint) \
__imageAtomic(op,int)
imageAtomic(Add)
imageAtomic(Min)
imageAtomic(Max)
imageAtomic(And)
imageAtomic(Or)
imageAtomic(Xor)
__imageAtomic(Exchange, float)
__imageAtomic(Exchange, uint)
__imageAtomic(Exchange, int)
#define __imageAtomicCompSwap(type) \
type imageAtomicCompSwap(PIMAGE_PARAMS, type data) \
{ \
    return atomicCompSwap(__imageTexel(image, P, 0), data); \
} \
type imageAtomicCompSwap(PIMAGE_PARAMS_MS, type data) \
{ \
    return atomicCompSwap(__imageTexel(image, P, sample), data); \
}
__imageAtomicCompSwap(uint)
__imageAtomicCompSwap(int)
};
#undef gvec4
#undef gvec4MS
#undef IMAGE_PARAMS
#undef IMAGE_PARAMS_MS
#undef _image
#undef __image
#undef readonly
#undef writeonly

[uniform, set(0), binding(0)] @image(uint, 2D, Storage, R32i) cmd_heads;
[buffer, set(0), binding(1)] @block Commands {
	uint cmd_queue[];
};

float asfloat (uint x) = SPV(OpBitcast);
vec4 asrgba (uint x) = GLSL(UnpackUnorm4x8);

void
draw_line (uint ind, vec2 p, @inout vec4 color)
{
	auto a = vec2 (asfloat (cmd_queue[ind + 0]),
				   asfloat (cmd_queue[ind + 1]));
	auto b = vec2 (asfloat (cmd_queue[ind + 2]),
				   asfloat (cmd_queue[ind + 3]));
	auto r = asfloat (cmd_queue[ind + 5]);
	auto col = asrgba (cmd_queue[ind + 5]);
	float h = min (1, max (0, (p - a) • (b - a) / (b - a) • (b - a)));
	float d = length (p - a - h * (b - a)) - r;
	color = lerp (color, col, clamp (1f - d, 0f, 1f));
}

void
draw_circle (uint ind, vec2 p, @inout vec4 color)
{
	auto c = vec2 (asfloat (cmd_queue[ind + 0]),
				   asfloat (cmd_queue[ind + 1]));
	auto r = asfloat (cmd_queue[ind + 2]);
	auto col = asrgba (cmd_queue[ind + 3]);
	float d = length (p - c) - r;
	color = lerp (color, col, clamp (1 - d, 0, 1));
}

[out(0)] vec4 frag_color;

[shader("Fragment")]
void
main (void)
{
	// use 8x8 grid for screen grid
	auto cmd_coord = ivec2 (gl_FragCoord.xy) / 8;
	uint cmd_ind = imageLoad (cmd_heads, cmd_coord).r;

	auto color = vec4 (0, 0, 0, 1);
	while (cmd_ind != ~0u) {
		//FIXME might be a good time to look into BDA
		uint cmd = cmd_queue[cmd_ind + 0];
		uint next = cmd_queue[cmd_ind + 1];
		switch (cmd) {
		case 0:
			draw_line (cmd_ind + 2, gl_FragCoord.xy, color);
			break;
		case 1:
			draw_circle (cmd_ind + 2, gl_FragCoord.xy, color);
			break;
		}
		cmd_ind = next;
	}
	frag_color = color;
}
