#define INPUT_ATTACH_SET 0
#include "input_attach.h"

[in("ViewIndex"), flat] int gl_ViewIndex;
[in("FragCoord")] vec4 gl_FragCoord;

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
genFType mix(genFType x, genFType y, genFType a) = GLSL(FMix);
genFType mix(genFType x, genFType y, float a) = GLSL(FMix)
    [x, y, @construct (genFType, a)];
genFType mix(genFType x, genFType y, genBType a) = SPV(OpSelect) [a, y, x];

genFType pow(genFType x, genFType y) = GLSL(Pow);
genFType exp(genFType x) = GLSL(Exp);

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

};


#define OIT_SET 1
#include "oit_blend.finc"

#include "fog.finc"

[push_constant] @block PushConstants {
	vec4        fog;
	vec4        camera;
};

[out(0)] vec4 frag_color;

[shader("Fragment")]
[capability("MultiView")]
void
main (void)
{
	vec3        c = subpassLoad (color).rgb;
	vec3        l = subpassLoad (light).rgb;
	vec3        e = subpassLoad (emission).rgb;
	vec4        p = subpassLoad (position);
	l = l / (l + 1);
	vec3        o = max(BlendFrags (vec4 (c * l + e, 1)).xyz, vec3(0));

	float d = p.w > 0 ? length (p.xyz - camera.xyz) : 1e36;
	o = FogBlend (vec4(o,1), fog, d).xyz;
	o = pow (o, vec3(0.83));//FIXME make gamma correction configurable
	frag_color = vec4 (o, 1);
}
