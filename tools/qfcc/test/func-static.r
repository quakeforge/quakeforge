#pragma bug die
int count;

void foo (void)
{
	@static int x;

	if (!x)
		count++;
	x = 1;
}

int main(void)
{
	foo ();
	foo ();
	return count != 1;
}
