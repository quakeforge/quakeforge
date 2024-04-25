#ifndef MAT_TYPE
#define MAT_TYPE(type_name, base_type, cols, align_as)
#endif

MAT_TYPE(mat2x2, float, 2, vec2)
MAT_TYPE(mat2x3, float, 2, vec3)
MAT_TYPE(mat2x4, float, 2, vec4)
MAT_TYPE(mat3x2, float, 3, vec2)
MAT_TYPE(mat3x3, float, 3, vec3)
MAT_TYPE(mat3x4, float, 3, vec4)
MAT_TYPE(mat4x2, float, 4, vec2)
MAT_TYPE(mat4x3, float, 4, vec3)
MAT_TYPE(mat4x4, float, 4, vec4)
MAT_TYPE(dmat2x2, double, 2, dvec2)
MAT_TYPE(dmat2x3, double, 2, dvec3)
MAT_TYPE(dmat2x4, double, 2, dvec4)
MAT_TYPE(dmat3x2, double, 3, dvec2)
MAT_TYPE(dmat3x3, double, 3, dvec3)
MAT_TYPE(dmat3x4, double, 3, dvec4)
MAT_TYPE(dmat4x2, double, 4, dvec2)
MAT_TYPE(dmat4x3, double, 4, dvec3)
MAT_TYPE(dmat4x4, double, 4, dvec4)

#undef MAT_TYPE
