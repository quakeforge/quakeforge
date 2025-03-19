#pragma bug die
void printf (string fmt, ...) = #0;
float (string s) stof = #0;
@overload float (float x) sqrt = #0;

float
heron (float a, float b, float c)
{
	float s = (a + b + c) / 2;
	return sqrt (s*(s-a)*(s-b)*(s-c));
}

float
kahan (float a, float b, float c)
{
	float t;
	if (a < b) { t = a; a = b; b = t; }
	if (b < c) { t = b; b = c; c = t; }
	if (a < b) { t = a; a = b; b = t; }
	return sqrt ((a + (b + c))*(c - (a - b))*(c + (a - b))*(a + (b - c)))/4;
}

int
main (int argc, string *argv)
{
	float a = stof (argv[1]);
	float b = stof (argv[2]);
	float c = stof (argv[3]);
	float expt = stof (argv[4]);
	float h = heron (a, b, c);
	float k = kahan (a, b, c);
	printf ("%.9g %.9g %d\n", h, k, k == expt);
	return k != expt;
}
