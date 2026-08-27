void printf (string fmt, ...) = #0;

mat2x3 box (vec3 e, float invDensity)
{
	mat2x3 I = {};
	float vol = 8 * e.x * e.y * e.z;
	e = e * e;
	float i = 1.0f / 3;
	invDensity /= vol;
	I[0] = '1 1 1';
	I[1] = [e.y + e.z, e.z + e.x, e.x + e.y];// * i * invDensity;
	I[0] = I[0] * invDensity;
	I[1] = I[1] * invDensity / i;
	return I;
}

int main ()
{
	vec3 ext = '1 4 9';
	auto I = box(ext, 36*3*8);
	printf ("%.9v %.9v\n", I[0], I[1]);
	return (I[0] != '3 3 3' || I[1] != '873 738 153') & 1;
}
