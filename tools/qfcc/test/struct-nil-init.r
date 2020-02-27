#pragma bug die

struct foo {
	quaternion x;
	double y;
};

int main()
{
	struct foo bar = { };
	return bar.y;	// to survive and prevail
}
