#ifndef __input_attach_h
#define __input_attach_h
#include "subpassInput.h"

typedef @image(float, SubpassData) subpassInput;

#ifndef INPUT_ATTACH_SET
#define INPUT_ATTACH_SET 0
#endif

#define INPUT_ATTACH(ind) \
	[uniform, \
	 input_attachment_index(ind), \
	 set(INPUT_ATTACH_SET), \
	 binding(ind)]

INPUT_ATTACH(0) subpassInput color;
#ifndef COLOR_ONLY
INPUT_ATTACH(1) subpassInput emission;
INPUT_ATTACH(2) subpassInput normal;
INPUT_ATTACH(3) subpassInput position;
INPUT_ATTACH(4) subpassInput light;
#endif

#endif//__input_attach_h
