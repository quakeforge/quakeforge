entity spawn (void) { return nil; }
.float f;
float time;

void foo (void)
{
	local entity ent;

	return;
	if (!time) {
		ent = spawn ();
		ent.f = time + 0.1;
	}
}

int main ()
{
	return 0;	// if the test compiles, it passes.
}
