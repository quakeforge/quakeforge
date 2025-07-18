#ifndef __general_h
#define __general_h

#define GLSL(op) @intrinsic(OpExtInst, "GLSL.std.450", op)
#define SPV(op) @intrinsic(op)
@generic (genFType = @vector (float),
		  genDType = @vector (double),
		  genIType = @vector (int),
		  genUType = @vector (uint),
		  genBType = @vector (bool)) {
float length(genFType x) = GLSL(Length);
double length(genDType x) = GLSL(Length);
genFType normalize(genFType x) = GLSL(Normalize);
genFType clamp(genFType x, genFType minVal, genFType maxVal) = GLSL(FClamp);

genFType abs(genFType x) = GLSL(FAbs);
genIType abs(genIType x) = GLSL(SAbs);
genDType abs(genDType x) = GLSL(FAbs);

genFType sign(genFType x) = GLSL(FSign);
genIType sign(genIType x) = GLSL(SSign);
genDType sign(genDType x) = GLSL(FSign);


genFType mix(genFType x, genFType y, genFType a) = GLSL(FMix);
genFType mix(genFType x, genFType y, float a) = GLSL(FMix)
	[x, y, @construct (genFType, a)];
genDType mix(genDType x, genDType y, genDType a) = GLSL(FMix);
genDType mix(genDType x, genDType y, double a) = GLSL(FMix)
	[x, y, @construct (genDType, a)];
genFType mix(genFType x, genFType y, genBType a) = SPV(OpSelect) [a,y,x];
genDType mix(genDType x, genDType y, genBType a) = SPV(OpSelect) [a,y,x];
genIType mix(genIType x, genIType y, genBType a) = SPV(OpSelect) [a,y,x];
genUType mix(genUType x, genUType y, genBType a) = SPV(OpSelect) [a,y,x];
genBType mix(genBType x, genBType y, genBType a) = SPV(OpSelect) [a,y,x];

genFType pow(genFType x, genFType y) = GLSL(Pow);
genFType exp(genFType x) = GLSL(Exp);
genFType log(genFType x) = GLSL(Log);
genFType exp2(genFType x) = GLSL(Exp2);
genFType log2(genFType x) = GLSL(Log2);
genFType sqrt(genFType x) = GLSL(Sqrt);
genDType sqrt(genDType x) = GLSL(Sqrt);
genFType inversesqrt(genFType x) = GLSL(InverseSqrt);
genDType inversesqrt(genDType x) = GLSL(InverseSqrt);

genFType min(genFType x, genFType y) = GLSL(FMin);
genFType min(genFType x, float y)    = GLSL(FMin) [x, @construct (genFType, y)];
genDType min(genDType x, genDType y) = GLSL(FMin) ;
genDType min(genDType x, double y)   = GLSL(FMin) [x, @construct (genDType, y)];
genIType min(genIType x, genIType y) = GLSL(SMin);
genIType min(genIType x, int y)      = GLSL(SMin) [x, @construct (genIType, y)];
genUType min(genUType x, genUType y) = GLSL(UMin);
genUType min(genUType x, uint y)     = GLSL(UMin) [x, @construct (genUType, y)];
genFType max(genFType x, genFType y) = GLSL(FMax);
genFType max(genFType x, float y)    = GLSL(FMax) [x, @construct (genFType, y)];
genDType max(genDType x, genDType y) = GLSL(FMax);
genDType max(genDType x, double y)   = GLSL(FMax) [x, @construct (genDType, y)];
genIType max(genIType x, genIType y) = GLSL(SMax);
genIType max(genIType x, int y)      = GLSL(SMax) [x, @construct (genIType, y)];
genUType max(genUType x, genUType y) = GLSL(UMax);
genUType max(genUType x, uint y)     = GLSL(UMax) [x, @construct (genUType, y)];

genFType smoothstep(genFType edge0, genFType edge1, genFType x)
	= GLSL(SmoothStep);
genFType smoothstep(float edge0, float edge1, genFType x)
	= GLSL(SmoothStep) [@construct(genFType, edge0),
						@construct(genFType, edge1), x];
genDType smoothstep(genDType edge0, genDType edge1, genDType x)
	= GLSL(SmoothStep) ;
genDType smoothstep(double edge0, double edge1, genDType x)
	= GLSL(SmoothStep) [@construct(genDType, edge0),
						 @construct(genDType, edge1), x];
};

#undef SPV
#undef GLSL

#endif//__general_h
