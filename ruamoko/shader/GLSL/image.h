#ifndef __qfcc_shader_glsl_image_h
#define __qfcc_shader_glsl_image_h

#ifndef __GLSL__
#include "_defines.h"
#endif

//FIXME these shouldn't exist (use param attributes
#define __readonly
#define __writeonly

// image functions
@generic(gimage=[_image(1D),   _image(1D,Array),
                 _image(2D),   _image(2D,Array),
                 _image(Cube), _image(Cube,Array),
                 _image(3D),   _image(Rect), _image(Buffer)],
         gimageMS=[_image(2D,MS), _image(2D,MS,Array)]) {

gimage.size_type imageSize(__readonly __writeonly gimage image)
	= SPV(OpImageQuerySize);
gimageMS.size_type imageSize(__readonly __writeonly gimageMS image)
	= SPV(OpImageQuerySize);
int imageSamples(__readonly __writeonly gimageMS image) = SPV(OpImageQuerySamples);
@vector(gimage.sample_type, 4)
	imageLoad(__readonly IMAGE_PARAMS) = SPV(OpImageRead);
@vector(gimageMS.sample_type, 4)
	imageLoad(__readonly IMAGE_PARAMS_MS) = SPV(OpImageRead);
void imageStore(__writeonly IMAGE_PARAMS, @vector(gimage.sample_type, 4) data)
	= SPV(OpImageWrite);
void imageStore(__writeonly IMAGE_PARAMS_MS,
				@vector(gimageMS.sample_type, 4) data)
	= SPV(OpImageWrite);
@reference(gimage.sample_type, StorageClass.Image)
	__imageTexel(PIMAGE_PARAMS, int sample) = SPV(OpImageTexelPointer);
@reference(gimageMS.sample_type, StorageClass.Image)
	__imageTexel(PIMAGE_PARAMS_MS) = SPV(OpImageTexelPointer);
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

#endif//__qfcc_shader_glsl_image_h
