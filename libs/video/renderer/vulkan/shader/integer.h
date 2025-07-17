#ifndef __integer_h
#define __integer_h

#define GLSL(op) @intrinsic(OpExtInst, "GLSL.std.450", op)
#define SPV(op) @intrinsic(op)
#define highp
#define lowp
#define givec(base) @vector(int, @width(base))
@generic (genFType = @vector (float),
		  genDType = @vector (double),
		  genIType = @vector (int),
		  genUType = @vector (uint),
		  genBType = @vector (bool)) {
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
genIType bitfieldExtract(genIType value, int offset, int bits) = SPV(OpBitFieldSExtract);
genUType bitfieldExtract(genUType value, int offset, int bits) = SPV(OpBitFieldUExtract);
genIType bitfieldInsert(genIType base, genIType insert,
                        int offset, int bits) = SPV(OpBitFieldInsert);
genUType bitfieldInsert(genUType base, genUType insert,
                        int offset, int bits) = SPV(OpBitFieldInsert);
genIType bitfieldReverse(highp genIType value) = SPV(OpBitReverse);
givec(genUType) bitfieldReverse(highp genUType value) = SPV(OpBitReverse);
genIType bitCount(genIType value) = SPV(OpBitCount);
givec(genUType) bitCount(genUType value) = SPV(OpBitCount);
genIType findLSB(genIType value) = GLSL(FindSLsb);
givec(genUType) findLSB(genUType value) = GLSL(FindULsb);
genIType findMSB(highp genIType value) = GLSL(FindSMsb);
givec(genUType) findMSB(highp genUType value) = GLSL(FindUMsb);

};

#undef highp
#undef SPV
#undef GLSL

#endif//__integer_h
