#ifndef __qfcc_shader_glsl_general_h
#define __qfcc_shader_glsl_general_h

#ifndef __GLSL__
#include "_defines.h"
#endif

@generic(genFType=@vector(float),
         genDType=@vector(double),
         genIType=@vector(int),
         genUType=@vector(uint),
         genBType=@vector(bool),
         mat=@matrix(float),
         vec=[vec2,vec3,vec4,dvec2,dvec3,dvec4],
         ivec=[ivec2,ivec3,ivec4],
         uvec=[uvec2,uvec3,uvec4],
         bvec=[bvec2,bvec3,bvec4]) {

genFType radians(genFType degrees) = GLSL(Radians);
genFType degrees(genFType radians) = GLSL(Degrees);
genFType sin(genFType angle) = GLSL(Sin);
genFType cos(genFType angle) = GLSL(Cos);
genFType tan(genFType angle) = GLSL(Tan);
genFType asin(genFType x) = GLSL(Asin);
genFType acos(genFType x) = GLSL(Acos);
genFType atan(genFType y, genFType x) = GLSL(Atan2);
genFType atan(genFType y_over_x) = GLSL(Atan);
genFType sinh(genFType x) = GLSL(Sinh);
genFType cosh(genFType x) = GLSL(Cosh);
genFType tanh(genFType x) = GLSL(Tanh);
genFType asinh(genFType x) = GLSL(Asinh);
genFType acosh(genFType x) = GLSL(Acosh);
genFType atanh(genFType x) = GLSL(Atanh);

//exponential functions
genFType pow(genFType x, genFType y) = GLSL(Pow);
genFType exp(genFType x) = GLSL(Exp);
genFType log(genFType x) = GLSL(Log);
genFType exp2(genFType x) = GLSL(Exp2);
genFType log2(genFType x) = GLSL(Log2);
genFType sqrt(genFType x) = GLSL(Sqrt);
genDType sqrt(genDType x) = GLSL(Sqrt);
genFType inversesqrt(genFType x) = GLSL(InverseSqrt);
genDType inversesqrt(genDType x) = GLSL(InverseSqrt);

//common functions
genFType abs(genFType x) = GLSL(FAbs);
genIType abs(genIType x) = GLSL(SAbs);
genDType abs(genDType x) = GLSL(FAbs);
genFType sign(genFType x) = GLSL(FSign);
genIType sign(genIType x) = GLSL(SSign);
genDType sign(genDType x) = GLSL(FSign);
genFType floor(genFType x) = GLSL(Floor);
genDType floor(genDType x) = GLSL(Floor);
genFType trunc(genFType x) = GLSL(Trunc);
genDType trunc(genDType x) = GLSL(Trunc);
genFType round(genFType x) = GLSL(Round);
genDType round(genDType x) = GLSL(Round);
genFType roundEven(genFType x) = GLSL(RoundEven);
genDType roundEven(genDType x) = GLSL(RoundEven);
genFType ceil(genFType x) = GLSL(Ceil);
genDType ceil(genDType x) = GLSL(Ceil);
genFType fract(genFType x) = GLSL(Fract);
genDType fract(genDType x) = GLSL(Fract);
genFType mod(genFType x, float y) = SPV(OpFMod) [x, @construct (genFType, y)];
genFType mod(genFType x, genFType y) = SPV(OpFMod);
genDType mod(genDType x, double y) = SPV(OpFMod) [x, @construct (genDType, y)];
genDType mod(genDType x, genDType y) = SPV(OpFMod);
genFType modf(genFType x, @out genFType i);
genDType modf(genDType x, @out genDType i);
genFType min(genFType x, genFType y) = GLSL(FMin);
genFType min(genFType x, float y) = GLSL(FMin) [x, @construct (genFType, y)];
genDType min(genDType x, genDType y) = GLSL(FMin);
genDType min(genDType x, double y) = GLSL(FMin) [x, @construct (genDType, y)];
genIType min(genIType x, genIType y) = GLSL(SMin);
genIType min(genIType x, int y) = GLSL(SMin) [x, @construct (genIType, y)];
genUType min(genUType x, genUType y) = GLSL(UMin);
genUType min(genUType x, uint y) = GLSL(UMin) [x, @construct (genUType, y)];
genFType max(genFType x, genFType y) = GLSL(FMax);
genFType max(genFType x, float y) = GLSL(FMax) [x, @construct (genFType, y)];
genDType max(genDType x, genDType y) = GLSL(FMax);
genDType max(genDType x, double y) = GLSL(FMax) [x, @construct (genDType, y)];
genIType max(genIType x, genIType y) = GLSL(SMax);
genIType max(genIType x, int y) = GLSL(SMax) [x, @construct (genIType, y)];
genUType max(genUType x, genUType y) = GLSL(UMax);
genUType max(genUType x, uint y) = GLSL(UMax) [x, @construct (genUType, y)];
genFType clamp(genFType x, genFType minVal, genFType maxVal)
	= GLSL(FClamp);
genFType clamp(genFType x, float minVal, float maxVal)
	= GLSL(FClamp) [x, @construct (genFType, minVal),
						@construct (genFType, maxVal)];
genDType clamp(genDType x, genDType minVal, genDType maxVal)
	= GLSL(FClamp);
genDType clamp(genDType x, double minVal, double maxVal)
	= GLSL(FClamp) [x, @construct (genDType, minVal),
						@construct (genDType, maxVal)];
genIType clamp(genIType x, genIType minVal, genIType maxVal)
	= GLSL(SClamp);
genIType clamp(genIType x, int minVal, int maxVal)
	= GLSL(SClamp) [x, @construct (genIType, minVal),
						@construct (genIType, maxVal)];
genUType clamp(genUType x, genUType minVal, genUType maxVal)
	= GLSL(UClamp);
genUType clamp(genUType x, uint minVal, uint maxVal)
	= GLSL(UClamp) [x, @construct (genUType, minVal),
						@construct (genUType, maxVal)];
genFType mix(genFType x, genFType y, genFType a)
	= GLSL(FMix);
genFType mix(genFType x, genFType y, float a)
	= GLSL(FMix) [x, y, @construct (genFType, a)];
genDType mix(genDType x, genDType y, genDType a)
	= GLSL(FMix);
genDType mix(genDType x, genDType y, double a)
	= GLSL(FMix) [x, y, @construct (genDType, a)];
genFType mix(genFType x, genFType y, genBType a)
	= SPV(OpSelect) [a,y,x];
genDType mix(genDType x, genDType y, genBType a)
	= SPV(OpSelect) [a,y,x];
genIType mix(genIType x, genIType y, genBType a)
	= SPV(OpSelect) [a,y,x];
genUType mix(genUType x, genUType y, genBType a)
	= SPV(OpSelect) [a,y,x];
genBType mix(genBType x, genBType y, genBType a)
	= SPV(OpSelect) [a,y,x];
genFType step(genFType edge, genFType x)
	= GLSL(Step);
genFType step(float edge, genFType x)
	= GLSL(Step) [@construct(genFType, edge), x];
genDType step(genDType edge, genDType x)
	= GLSL(Step);
genDType step(double edge, genDType x)
	= GLSL(Step) [@construct(genDType, edge), x];
genFType smoothstep(genFType edge0, genFType edge1, genFType x)
	= GLSL(SmoothStep);
genFType smoothstep(float edge0, float edge1, genFType x)
	= GLSL(SmoothStep) [@construct(genFType, edge0),
							@construct(genFType, edge1), x];
genDType smoothstep(genDType edge0, genDType edge1, genDType x)
	= GLSL(SmoothStep);
genDType smoothstep(double edge0, double edge1, genDType x)
	= GLSL(SmoothStep) [@construct(genDType, edge0),
							@construct(genDType, edge1), x];
gbvec(genFType) isnan(genFType x) = SPV(OpIsNan);
gbvec(genDType) isnan(genDType x) = SPV(OpIsNan);
gbvec(genFType) isinf(genFType x) = SPV(OpIsInf);
gbvec(genDType) isinf(genDType x) = SPV(OpIsInf);
givec(genFType) floatBitsToInt(highp genFType value) = SPV(OpBitcast);
guvec(genFType) floatBitsToUint(highp genFType value) = SPV(OpBitcast);
gvec(genIType) intBitsToFloat(highp genIType value) = SPV(OpBitcast);
gvec(genUType) uintBitsToFloat(highp genUType value) = SPV(OpBitcast);
genFType fma(genFType a, genFType b, genFType c) = GLSL(Fma);
genDType fma(genDType a, genDType b, genDType c) = GLSL(Fma);
genFType frexp(highp genFType x, @out highp genIType exp) = GLSL(Frexp);
genDType frexp(genDType x, @out genIType exp) = GLSL(Frexp);
genFType ldexp(highp genFType x, highp genIType exp) = GLSL(Ldexp);
genDType ldexp(genDType x, genIType exp) = GLSL(Ldexp);

//floating-point pack and unpack functions
highp uint packUnorm2x16(vec2 v) = GLSL(PackUnorm2x16);
highp uint packSnorm2x16(vec2 v) = GLSL(PackSnorm2x16);
uint packUnorm4x8(vec4 v) = GLSL(PackUnorm4x8);
uint packSnorm4x8(vec4 v) = GLSL(PackSnorm4x8);
vec2 unpackUnorm2x16(highp uint p) = GLSL(UnpackUnorm2x16);
vec2 unpackSnorm2x16(highp uint p) = GLSL(UnpackSnorm2x16);
vec4 unpackUnorm4x8(highp uint p) = GLSL(UnpackUnorm4x8);
vec4 unpackSnorm4x8(highp uint p) = GLSL(UnpackSnorm4x8);
uint packHalf2x16(vec2 v) = GLSL(PackHalf2x16);
vec2 unpackHalf2x16(uint v) = GLSL(UnpackHalf2x16);
double packDouble2x32(uvec2 v) = GLSL(PackDouble2x32);
uvec2 unpackDouble2x32(double v) = GLSL(UnpackDouble2x32);

//geometric functions
float length(genFType x) = GLSL(Length);
double length(genDType x) = GLSL(Length);
float distance(genFType p0, genFType p1) = GLSL(Distance);
double distance(genDType p0, genDType p1) = GLSL(Distance);
float dot(genFType x, genFType y) = SPV(OpDot);
double dot(genDType x, genDType y) = SPV(OpDot);
@overload vec3 cross(vec3 x, vec3 y) = GLSL(Cross);
@overload dvec3 cross(dvec3 x, dvec3 y) = GLSL(Cross);
genFType normalize(genFType x) = GLSL(Normalize);
genDType normalize(genDType x) = GLSL(Normalize);
genFType faceforward(genFType N, genFType I, genFType Nref);
genDType faceforward(genDType N, genDType I, genDType Nref);
genFType reflect(genFType I, genFType N);
genDType reflect(genDType I, genDType N);
genFType refract(genFType I, genFType N, float eta);
genDType refract(genDType I, genDType N, double eta);

//matrix functions
mat matrixCompMult(mat x, mat y) = SPV(SpvOpMatrixTimesMatrix);
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

//vector relational functions
gbvec(vec) lessThan(vec x, vec y) = SPV(OpFOrdLessThan);
gbvec(ivec) lessThan(ivec x, ivec y) = SPV(OpSLessThan);
gbvec(uvec) lessThan(uvec x, uvec y) = SPV(OpULessThan);
gbvec(vec) lessThanEqual(vec x, vec y) = SPV(OpFOrdLessThanEqual);
gbvec(ivec) lessThanEqual(ivec x, ivec y) = SPV(OpSLessThanEqual);
gbvec(uvec) lessThanEqual(uvec x, uvec y) = SPV(OpULessThanEqual);
gbvec(vec) greaterThan(vec x, vec y) = SPV(OpFOrdGreaterThan);
gbvec(ivec) greaterThan(ivec x, ivec y) = SPV(OpSGreaterThan);
gbvec(uvec) greaterThan(uvec x, uvec y) = SPV(OpUGreaterThan);
gbvec(vec) greaterThanEqual(vec x, vec y) = SPV(OpFOrdGreaterThanEqual);
gbvec(ivec) greaterThanEqual(ivec x, ivec y) = SPV(OpUGreaterThanEqual);
gbvec(uvec) greaterThanEqual(uvec x, uvec y) = SPV(OpSGreaterThanEqual);
gbvec(vec) equal(vec x, vec y) = SPV(OpFOrdEqual);
gbvec(ivec) equal(ivec x, ivec y) = SPV(OpIEqual);
gbvec(uvec) equal(uvec x, uvec y) = SPV(OpIEqual);
gbvec(bvec) equal(bvec x, bvec y) = SPV(OpLogicalEqual);
gbvec(vec) notEqual(vec x, vec y) = SPV(OpFOrdNotEqual);
gbvec(ivec) notEqual(ivec x, ivec y) = SPV(OpINotEqual);
gbvec(uvec) notEqual(uvec x, uvec y) = SPV(OpINotEqual);
gbvec(bvec) notEqual(bvec x, bvec y) = SPV(OpLogicalNotEqual);
bool any(bvec x) = SPV(OpAny);
bool all(bvec x) = SPV(OpAll);
bvec not(bvec x) = SPV(OpLogicalNot);

//integer functions
genUType uaddCarry(highp genUType x, highp genUType y,
                   @out lowp genUType carry);
genUType usubBorrow(highp genUType x, highp genUType y,
                    @out lowp genUType borrow);
void umulExtended(highp genUType x, highp genUType y,
                  @out highp genUType msb,
                  @out highp genUType lsb);
void imulExtended(highp genIType x, highp genIType y,
                  @out highp genIType msb,
                  @out highp genIType lsb);
genIType bitfieldExtract(genIType value, int offset, int bits)
	= SPV(OpBitFieldSExtract);
genUType bitfieldExtract(genUType value, int offset, int bits)
	= SPV(OpBitFieldUExtract);
genIType bitfieldInsert(genIType base, genIType insert, int offset, int bits)
	= SPV(OpBitFieldInsert);
genUType bitfieldInsert(genUType base, genUType insert, int offset, int bits)
	= SPV(OpBitFieldInsert);
genIType bitfieldReverse(highp genIType value) = SPV(OpBitReverse);
givec(genUType) bitfieldReverse(highp genUType value) = SPV(OpBitReverse);
genIType bitCount(genIType value) = SPV(OpBitCount);
givec(genUType) bitCount(genUType value) = SPV(OpBitCount);
genIType findLSB(genIType value) = GLSL(FindSLsb);
givec(genUType) findLSB(genUType value) = GLSL(FindULsb);
genIType findMSB(highp genIType value) = GLSL(FindSMsb);
givec(genUType) findMSB(highp genUType value) = GLSL(FindUMsb);

};

#endif//__qfcc_shader_glsl_general_h
