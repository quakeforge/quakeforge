#ifndef __ruamoko_math_h
#define __ruamoko_math_h

@extern vector v_forward, v_up, v_right;

@extern void (vector ang) makevectors;
@extern float () random;
@extern integer (float f) ftoi;
@extern float (integer i) itof;
@extern vector (vector v) normalize;
@extern float (vector v) vlen;
@extern float (vector v) vectoyaw;
@extern float (float v) rint;
@extern float (float v) floor;
@extern float (float v) ceil;
@extern float (float f) fabs;
@extern vector (vector v) vectoangles;

#endif//__ruamoko_math_h
