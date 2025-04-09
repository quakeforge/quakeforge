#ifndef OIT_SET
#define OIT_SET 2
#endif

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
