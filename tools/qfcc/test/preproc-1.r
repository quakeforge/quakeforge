#define LPAREN() (
#define G(Q) 42
#define F(R, X, ...) __VA_OPT__(G R X) )
int x = F(LPAREN(), 0, <:-); // replaced by int x = 42;

int
main ()
{
	return x != 42;
}
