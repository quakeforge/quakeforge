typedef enum {
	NO = 0,
	YES
} BOOL;

@extern BOOL sel_is_mapped (SEL aSel);
BOOL (SEL aSel) sel_is_mapped = #0;

@overload int foo(int x)
{
	return 1;
}

@overload int foo(float x)
{
	return 2;
}

int main()
{
	//FIXME fails on implicit cast of double to float
	return !(foo(5) == 1 && foo (5.0f) == 2);
}
