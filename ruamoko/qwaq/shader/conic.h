#ifndef __qwaq_shader_conic_h
#define __qwaq_shader_conic_h

#include "../pga3d.h"

typedef struct conic_s {
	point_t point;
	point_t s, t;
	float slr;
	float ecc;
	float width;
	uint  color;
} conic_t;

#endif//__qwaq_shader_conic_h
