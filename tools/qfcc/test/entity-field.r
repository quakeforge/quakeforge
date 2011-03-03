.float x;
.vector v;
.float z = nil;
.float w;
.void (float y) func;
//.void (float y) bi = #0;
.void (float y) ni = nil;
.float u;

//.void (float y) co =
//{
//}

void (entity ent) foo =
{
	ent.x = ent.v * ent.v;
	ent.func (ent.v * ent.v);
};
