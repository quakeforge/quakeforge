#ifndef __qfcc_shader_glsl__defines_h
#define __qfcc_shader_glsl__defines_h

#define SPV(op) @intrinsic(op)
#define GLSL(op) @intrinsic(OpExtInst, "GLSL.std.450", op)

#define IMAGE_PARAMS gimage image, gimage.image_coord P
#define IMAGE_PARAMS_MS gimageMS image, gimageMS.image_coord P, int sample
#define PIMAGE_PARAMS @reference(gimage) image, gimage.image_coord P
#define PIMAGE_PARAMS_MS @reference(gimageMS) image, gimageMS.image_coord P, int sample
#define __image(...) @image(__VA_ARGS__ __VA_OPT__(,) Storage)
#define __sampler(...) @sampler(@image(__VA_ARGS__))
#define __spI(...) @image(__VA_ARGS__ __VA_OPT__(,) SubpassData)
#define __texture(...) @image(__VA_ARGS__ __VA_OPT__(,) Sampled)
#define _image(...) __image(float __VA_OPT__(,) __VA_ARGS__), __image(int __VA_OPT__(,) __VA_ARGS__), __image(uint __VA_OPT__(,) __VA_ARGS__)
#define _sampler(...) __sampler(float __VA_OPT__(,) __VA_ARGS__), __sampler(int __VA_OPT__(,) __VA_ARGS__), __sampler(uint __VA_OPT__(,) __VA_ARGS__)
#define _subpassInput(...) __spI(float __VA_OPT__(,) __VA_ARGS__), __spI(int __VA_OPT__(,) __VA_ARGS__), __spI(uint __VA_OPT__(,) __VA_ARGS__)
#define _texture(...) __texture(float __VA_OPT__(,) __VA_ARGS__), __texture(int __VA_OPT__(,) __VA_ARGS__), __texture(uint __VA_OPT__(,) __VA_ARGS__)
#define gbvec(base) @vector(bool, @width(base))
#define gdvec(base) @vector(double, @width(base))
#define givec(base) @vector(int, @width(base))
#define guvec(base) @vector(uint, @width(base))
#define gvec(base) @vector(float, @width(base))
#define intr @reference(int)
#define uintr @reference(uint)

//FIXME these shouldn't exist (use param attributes
#define highp
#define lowp

#endif//__qfcc_shader_glsl_defines_h
