#ifndef __pga3d_h
#define __pga3d_h

#include <scene.h>

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.group_mask(0x6) rotor_t;
typedef PGA.group_mask(0xc) translator_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;

@overload motor_t normalize (motor_t m);
@overload motor_t sqrt (motor_t m);
@overload motor_t exp (bivector_t b);
@overload rotor_t exp (PGA.bvect b);
@overload translator_t exp (PGA.bvecp b);
@overload bivector_t log (motor_t m);
motor_t make_motor (vec4 translation, vec4 rotation);
void set_transform (motor_t m, transform_t transform);

typedef struct state_s {
	motor_t     M;
	bivector_t  B;
} state_t;

typedef struct body_s {
	motor_t     R;
	bivector_t  I;
	bivector_t  Ii;
} body_t;

@overload state_t dState (state_t s);
@overload state_t dState (state_t s, bivector_t f);
@overload state_t dState (state_t s, body_t *body);

void draw_3dline (transform_t camera, vec4 p1, vec4 p2, int color);
void create_cube ();
void update_cube (float dt);
void draw_cube ();

#endif//__pga3d_h
