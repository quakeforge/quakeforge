#include <math.h>

void printf (string fnt, ...) = #0;

#define SAMPLES 4
float output[SAMPLES];
float input[SAMPLES];

float T=0.1;
float w=2;
float a=0.5;

float A[3], B[3];

void
z_transform (float *y, float *x, float *A, float *B, int n, int zind)
{
	zind %= SAMPLES;
	float c = x[zind] * A[0];
	for (int i = 1; i < n; i++) {
		int z = (SAMPLES + zind - i) % SAMPLES;
		c += x[z] * A[i] - y[z] * B[i];
	}
	y[zind] = c / B[0];
}

int
main ()
{
	float e = exp (-a*T);
	float c = cos (w*T);
	float s = sin (w*T);

	B[0] = 1;
	B[1] = -2 * e * c;
	B[2] = e * e;

	A[0] = 0;
	A[1] = 1 - e*(c + (a/w)*s);
	A[2] = e*e + e*((a/w)*s - c);

	for (int i = 0; i < 200; i++) {
		int ind = i % SAMPLES;
		input[ind] = 1;//i ? 0 : 1;
		z_transform (output, input, A, B, 3, i);
		printf ("%2d %7.4f %7.4f\n", i, input[ind], output[ind]);
	}

	return 0;
}
