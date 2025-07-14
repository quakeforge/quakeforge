#ifndef __image_h
#define __image_h

#define SPV(op) @intrinsic(op)

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
#undef SPV

#endif//__image_h
