.float x;
.vector v;
.void (float y) func;

void (entity ent) foo =
{
	ent.x = ent.v * ent.v;
	ent.func (ent.v * ent.v);
};
