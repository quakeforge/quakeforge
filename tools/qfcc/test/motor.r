typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;

motor_t
make_motor (vec4 translation, vec4 rotation)
{
	auto dt = (PGA.group_mask(0x18)) translation;
	auto q = (PGA.group_mask(0x06)) rotation;
	motor_t t = { .scalar = 1, .bvecp = -dt.bvecp };
	motor_t r = { .scalar = q.scalar, .bvect = q.bvect };
	motor_t m = t * r;
	return m;
}
