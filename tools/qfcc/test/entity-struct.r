typedef struct foo_s {
	int a, b;
} foo_t;

entity self;
foo_t a, b;
vector x, y;
int i, j;

.vector v;
.foo_t foo;

void bar (entity other)
{
	self.v = x;
	self.foo.b = i;
	j = self.foo.b;
	self.foo.a = self.foo.b;
	self.foo.b = self.foo.a;
}

int
main (void)
{
	return 0;	// to survive and prevail :)
}
