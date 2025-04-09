#ifndef __pga3d_h
#define __pga3d_h

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;

@overload motor_t normalize (motor_t m);
@overload motor_t sqrt (motor_t m);
@overload motor_t exp (bivector_t b);
@overload bivector_t log (motor_t m);
motor_t make_motor (vec4 translation, vec4 rotation);
void set_transform (motor_t m, transform_t transform, string p);

typedef struct {
	motor_t     M;
	bivector_t  B;
} state_t;

@overload state_t dState (state_t s);
@overload state_t dState (state_t s, bivector_t f);

void draw_3dline (transform_t camera, vec4 p1, vec4 p2, int color);

#endif//__pga3d_h
