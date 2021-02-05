void printf (string fmt, ...) = #0;

float
swap (float a, float b)
{
	float t;
	if (a < b) { t = a; a = b; b = t; }
	return a - b;
}

int
main ()
{
	return swap (1, 2) > 0 ? 0 : 1;
}
