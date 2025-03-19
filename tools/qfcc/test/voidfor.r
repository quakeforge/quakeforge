#pragma bug die
void bar ()
{
}

void foo ()
{
	int i;

	for (i = 0; i < 11; i++)
		bar ();
}

int main ()
{
	foo ();
	return 0;
}
