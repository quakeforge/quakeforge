#ifndef MAT_TYPE
#define MAT_TYPE(type_name, base_type, align_as)
#endif

MAT_TYPE(mat2x2, float, vec2)
MAT_TYPE(mat2x3, float, vec3)
MAT_TYPE(mat2x4, float, vec4)
MAT_TYPE(mat3x2, float, vec2)
MAT_TYPE(mat3x3, float, vec3)
MAT_TYPE(mat3x4, float, vec4)
MAT_TYPE(mat4x2, float, vec2)
MAT_TYPE(mat4x3, float, vec3)
MAT_TYPE(mat4x4, float, vec4)
MAT_TYPE(dmat2x2, double, dvec2)
MAT_TYPE(dmat2x3, double, dvec3)
MAT_TYPE(dmat2x4, double, dvec4)
MAT_TYPE(dmat3x2, double, dvec2)
MAT_TYPE(dmat3x3, double, dvec3)
MAT_TYPE(dmat3x4, double, dvec4)
MAT_TYPE(dmat4x2, double, dvec2)
MAT_TYPE(dmat4x3, double, dvec3)
MAT_TYPE(dmat4x4, double, dvec4)

#undef MAT_TYPE
