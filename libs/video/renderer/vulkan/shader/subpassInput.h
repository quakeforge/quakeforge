#ifndef __subpassInput_h
#define __subpassInput_h

#define SPV(op) @intrinsic(op)

#define __spI(...) @image(__VA_ARGS__ __VA_OPT__(,) SubpassData)
#define _subpassInput(...)	__spI(float __VA_OPT__(,) __VA_ARGS__),	\
							__spI(int __VA_OPT__(,) __VA_ARGS__),	\
							__spI(uint __VA_OPT__(,) __VA_ARGS__)
#define gvec4 @vector(gsubpassInput.sample_type, 4)
#define gvec4MS @vector(gsubpassInputMS.sample_type, 4)
@generic (gsubpassInput   = [_subpassInput()],
		  gsubpassInputMS = [_subpassInput(MS)]) {
gvec4 subpassLoad(gsubpassInput subpass)
	= SPV(OpImageRead) [subpass, @construct(gsubpassInput.image_coord, 0)];
gvec4MS subpassLoad(gsubpassInputMS subpass)
	= SPV(OpImageRead) [subpass, @construct(gsubpassInputMS.image_coord, 0),
						sample];
};
#undef gvec4MS
#undef gvec4
#undef _subpassInput
#undef __spI
#undef SPV

#endif
