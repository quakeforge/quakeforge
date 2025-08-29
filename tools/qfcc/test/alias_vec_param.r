ivec2 vec_func1();
void vec_func2(ivec2 x);

bool ok = false;

void draw ()
{
	int count = 1;
	ivec2 len = vec_func1();
	len.y = count * 2;
	vec_func2 (len);
}

ivec2 vec_func1 ()
{
	return { 42, 69 };
}

void vec_func2 (ivec2 x)
{
	ok = x.x == 42 && x.y == 2;
}

int main ()
{
	draw();
	return !ok;
}
