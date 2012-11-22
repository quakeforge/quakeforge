//integer foo;
//typedef integer foo;
//integer (void) foo = #0;
//integer (bag);
//.integer (sack);
//.integer (float sack)x;
//integer *bar, baz;
//integer snafu(void)
//{
//}
//integer [8]blah;

@overload void func (integer a, integer b, integer c);
@overload void func (integer a, integer b);
@overload void func (integer a, ...);
//@overload void func (integer a, integer b, ...);
@overload void func (string y);
//@overload void func (...);
void func (integer x)
{
//	func ("");
}
void func (string y)
{
//	func (0.0);
//	func (0, 0.0);
//	func (0, 0, 0.0);
	func (0.0, "");
//	func (0, 0, 0, 0, 0.0);
}
