typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.group_mask(0x6) rotor_t;
typedef PGA.group_mask(0xc) translator_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;

typedef struct body_s {
	motor_t     R;
	bivector_t  I;
	bivector_t  Ii;
} body_t;

body_t box (vec3 extent, float invDensity)
{
	body_t body = {};//infinite mass
	body.R = 1;
	vec3 e = extent;
	if (!invDensity || @horiz(| e == '0 0 0')) {
		return body;
	}
	float vol = 8 * e.x * e.y * e.z;
	e = e * e;
	float I = 1.0f / 3;
	invDensity /= vol;
	body.Ii.bvect = '1 1 1';
	body.Ii.bvecp = (PGA.bvecp) [e.y + e.z, e.z + e.x, e.x + e.y];
	//body.Ii.bvecp = (PGA.bvecp) (e.yzx + e.zxy);
	body.I.bvect = body.Ii.bvect / invDensity;
	body.I.bvecp = body.Ii.bvecp * I / invDensity;
	body.Ii.bvect *= invDensity;
	body.Ii.bvecp *= invDensity / I;
	return body;
}

#ifdef VULKAN
[out("Position")] vec4 pos;
[shader(Vertex)]
void main ()
{
	vec3 ext = '1 4 9';
	auto I = box(ext, 36*3*8);
	pos = vec4(I[1], 1);
}
#else
void printf (string fmt, ...) = #0;

int main ()
{
	vec3 ext = '1 4 9';
	auto body = box(ext, 36*3*8);
	printf ("%.9v %.9v\n", body.Ii.bvect, body.Ii.bvecp);
	return ((vec3)body.Ii.bvect != '3 3 3'
			|| (vec3)body.Ii.bvecp != '873 738 153') & 1;
}
#endif
