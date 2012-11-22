typedef struct foo_s {
	integer a, b;
} foo_t;

entity self;
foo_t a, b;
vector x, y;
integer i, j;

.vector v;
.foo_t foo;

void bar (entity other)
{
#if 0
	self.foo = a;
	b = self.foo;
	self.foo = other.foo;
	a = b;

	self.v = x;
	y = self.v;
	self.v = other.v;
	x = y;
#endif
	self.foo.b = i;
	j = self.foo.b;
	self.foo.a = self.foo.b;
	self.foo.b = self.foo.a;
}
