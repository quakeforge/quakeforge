#include "test-harness.h"

.void() think;
.float nextthink;
.float frame;
entity self;
float time;

$frame frame0 frame1 frame2 frame3

void
state0 (void)
[$frame1, state1]
{
	if (self.frame != $frame1 || self.think != state1
		|| self.nextthink != 0.1f) {
		printf ("state0: %g %x %g\n", self.frame, self.think, self.nextthink);
		exit (1);
	}
}

void
state1 (void)
[$frame2, state2, 0.2f]
{
	if (self.frame != $frame2 || self.think != state2
		|| self.nextthink != 0.2f) {
		printf ("state0: %g %x %g\n", self.frame, self.think, self.nextthink);
		exit (1);
	}
}

void
state2 (void)
[$frame0, state0, 0.5f]
{
	if (self.frame != $frame0 || self.think != state0
		|| self.nextthink != 0.5f) {
		printf ("state0: %g %x %g\n", self.frame, self.think, self.nextthink);
		exit (1);
	}
}

int
main ()
{
	self = spawn ();
	state0();
	while (self.frame != $frame0) {
		self.think();
	}
	return 0;
}
