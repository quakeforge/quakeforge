#ifndef __oit_h
#define __oit_h

#ifndef OIT_SET
#define OIT_SET 2
#endif

#ifdef __RUAMOKO__

typedef struct FragData {
	vec4        color;
	float       depth;
	int         next;
} FragData;

[buffer, set(OIT_SET), binding(0), coherent]
@block FragCount {
	int         numFragments;
	int         maxFragments;
};

[buffer, set(OIT_SET), binding(1)]
@block Fragments {
	FragData    fragments[];
};

[uniform, set(OIT_SET), binding(2), coherent]
@image(int, 2D, Array, Storage, R32i) heads;

#else

struct FragData {
	vec4        color;
	float       depth;
	int         next;
};

layout (set = OIT_SET, binding = 0) coherent buffer FragCount {
	int         numFragments;
	int         maxFragments;
};

layout (set = OIT_SET, binding = 1) buffer Fragments {
	FragData    fragments[];
};

layout (set = OIT_SET, binding = 2, r32i) coherent uniform iimage2DArray heads;
#endif

#endif//__oit_h
