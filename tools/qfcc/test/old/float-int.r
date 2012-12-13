float (integer i) foo = #0;
integer (integer a) bar = #0;

integer func (integer size)
{
	return bar (foo (size) * 8);
}
