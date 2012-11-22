entity self;
.vector velocity;
.vector oldorigin;
.float speed;
@extern .vector avel;
@extern vector vel;

void (vector tdest, float speed, void () func) SUB_CalcMove = {}
vector normalize (vector x) = #0;
float vlen (vector x) = #0;

void oof (void)
{
}

void foo (void)
{
	float forward = vlen (self.velocity);
	forward *= 0.8;
	self.velocity = forward * normalize (self.velocity);
	SUB_CalcMove(self.oldorigin, self.speed, oof);
	self.avel_y = vel_z;
}
