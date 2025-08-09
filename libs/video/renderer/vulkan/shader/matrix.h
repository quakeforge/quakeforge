#ifndef __matrix_h
#define __matrix_h

#define SPV(op) @intrinsic(op)
#define GLSL(op) @intrinsic(OpExtInst, "GLSL.std.450", op)

@generic(mat=@matrix(float)) {
mat matrixCompMult(mat x, mat y) = SPV(SpvOpMatrixTimesMatrix);
};
@overload mat2 outerProduct(vec2 c, vec2 r) = SPV(OpOuterProduct);
@overload mat3 outerProduct(vec3 c, vec3 r) = SPV(OpOuterProduct);
@overload mat4 outerProduct(vec4 c, vec4 r) = SPV(OpOuterProduct);
@overload mat2x3 outerProduct(vec3 c, vec2 r) = SPV(OpOuterProduct);
@overload mat3x2 outerProduct(vec2 c, vec3 r) = SPV(OpOuterProduct);
@overload mat2x4 outerProduct(vec4 c, vec2 r) = SPV(OpOuterProduct);
@overload mat4x2 outerProduct(vec2 c, vec4 r) = SPV(OpOuterProduct);
@overload mat3x4 outerProduct(vec4 c, vec3 r) = SPV(OpOuterProduct);
@overload mat4x3 outerProduct(vec3 c, vec4 r) = SPV(OpOuterProduct);
@overload mat2 transpose(mat2 m) = SPV(OpTranspose);
@overload mat3 transpose(mat3 m) = SPV(OpTranspose);
@overload mat4 transpose(mat4 m) = SPV(OpTranspose);
@overload mat2x3 transpose(mat3x2 m) = SPV(OpTranspose);
@overload mat3x2 transpose(mat2x3 m) = SPV(OpTranspose);
@overload mat2x4 transpose(mat4x2 m) = SPV(OpTranspose);
@overload mat4x2 transpose(mat2x4 m) = SPV(OpTranspose);
@overload mat3x4 transpose(mat4x3 m) = SPV(OpTranspose);
@overload mat4x3 transpose(mat3x4 m) = SPV(OpTranspose);
@overload float determinant(mat2 m) = GLSL(Determinant);
@overload float determinant(mat3 m) = GLSL(Determinant);
@overload float determinant(mat4 m) = GLSL(Determinant);
@overload mat2 inverse(mat2 m) = GLSL(MatrixInverse);
@overload mat3 inverse(mat3 m) = GLSL(MatrixInverse);
@overload mat4 inverse(mat4 m) = GLSL(MatrixInverse);

#undef SPV
#undef GLSL

#endif//__matrix_h
