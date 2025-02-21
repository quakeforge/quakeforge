#pragma bug die
int strlen (string s) = #0;

int foo (string bar)
{
	int len;
	len = -strlen (bar);
	return len;
}

int main (void)
{
	return 0;
}
