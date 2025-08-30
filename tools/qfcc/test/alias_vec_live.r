ivec2 vec_func1();
void vec_func2(ivec2 x);

bool ok = false;

void draw ()
{
	int count = 2;
	ivec2 len = vec_func1();
	int height = vec_func1().y;
	//vec_func2 (len);
	len.y = count * height;
	vec_func2 (len);
}
#if 0
ivec2 vec_func1() = #0;
void vec_func2(ivec2 x) = #0;
#else
ivec2 vec_func1 ()
{
	return { 42, 69 };
}

void vec_func2 (ivec2 x)
{
	ok = x.x == 42 && x.y == 138;
}

int main ()
{
	draw();
	return !ok;
}
#endif
