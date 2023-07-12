typedef struct bar {
	float x, y, z, w;
} bar;

bar set_return();

@param foo()
{
	set_return();
	return nil;
}

bar set_return()
{
	return {1, 2, 3, 4};
}
void printf (string fmt, ...) = #0;
int main()
{
	@param r = foo();
	printf("%q\n", r);
	return !(r.quaternion_val.x == 0
			 && r.quaternion_val.y == 0
			 && r.quaternion_val.z == 0
			 && r.quaternion_val.w == 0);
}
