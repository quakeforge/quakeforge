#ifndef __input_attach_h
#define __input_attach_h
#include "GLSL/fragment.h"

typedef @image(float, SubpassData) subpassInput;

#ifndef INPUT_ATTACH_SET
#define INPUT_ATTACH_SET 0
#endif

#define INPUT_ATTACH(ind) \
	[uniform, \
	 input_attachment_index(ind), \
	 set(INPUT_ATTACH_SET), \
	 binding(ind)]

#endif//__input_attach_h
