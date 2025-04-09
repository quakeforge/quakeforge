#pragma bug die
float func (int x)
{
	float x = 0;
	return x;
}

int main ()
{
	return 1;	// test fails if compile succeeds
}
