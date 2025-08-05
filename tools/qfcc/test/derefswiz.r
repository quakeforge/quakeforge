#include "test-harness.h"

typedef struct {
	vec2 pitch;
	vec2 yaw;
} obj_t;

void
Player_cam (obj_t *self, float delta)
{
	vec2 dp = {-self.pitch.y, self.pitch.x };
	self.pitch += dp * delta;
}

int main ()
{
	obj_t obj = { .pitch = { 4, 3 } };
	Player_cam (&obj, -0.5);
	printf ("%g %g\n", obj.pitch.x, obj.pitch.y);
	return obj.pitch.x != 5.5 || obj.pitch.y != 1;
}
