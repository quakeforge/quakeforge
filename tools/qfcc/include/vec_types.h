#ifndef VEC_TYPE
#define VEC_TYPE(type_name, base_type)
#endif

VEC_TYPE(ivec2, int)
VEC_TYPE(ivec3, int)
VEC_TYPE(ivec4, int)
VEC_TYPE(vec2, float)
VEC_TYPE(vec3, float)
VEC_TYPE(vec4, float)
VEC_TYPE(lvec2, long)
VEC_TYPE(lvec3, long)
VEC_TYPE(lvec4, long)
VEC_TYPE(dvec2, double)
VEC_TYPE(dvec3, double)
VEC_TYPE(dvec4, double)
VEC_TYPE(uivec2, uint)
VEC_TYPE(uivec3, uint)
VEC_TYPE(uivec4, uint)
VEC_TYPE(ulvec2, ulong)
VEC_TYPE(ulvec3, ulong)
VEC_TYPE(ulvec4, ulong)

#undef VEC_TYPE
