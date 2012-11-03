void
ptrderef (int *to, int *from)
{
	int x;

	x = *from;
	x = *from++;
	x = *++from;
	//x = ++*from; FIXME syntax error (valid C)
	to = from++;
	to = ++from;
	*to = *from++;
	*to = *++from;
}
