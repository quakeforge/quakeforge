typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x0a) bivec_t;
typedef PGA.tvec point_t;

@overload float sqrt (float x) = #0;
void printf (string fmt, ...) = #0;

typedef struct {
	float view_dist;
} obj_t;

vec2 foo (obj_t *self, point_t a, point_t b)
{
	auto dir = a ∨ b;
	self.view_dist = sqrt (dir • ~dir);
	dir /= self.view_dist;
	return ((vec3)dir.bvect).xz;
}

int main ()
{
	obj_t obj = { 0 };
	auto xy = foo (&obj, (point_t)'3 0 0 1'f, (point_t)'0 0 4 1'f);
	printf ("%g %g %g\n", obj.view_dist, xy.x, xy.y);
	return obj.view_dist != 5 || xy.x != -0.6 || xy.y != 0.8;
}
