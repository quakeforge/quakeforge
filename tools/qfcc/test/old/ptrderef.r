int y;
void
ptrderef (int *to, int *from)
{
	int x;

	x = *from;
	x = *from++;
	x = *++from;
	x = ++*from;
	to = from++;
	to = ++from;
	*to = *from++;
	*to = *++from;
	*to = x;
	*to++ = *from++;
	y = *to++;
}
