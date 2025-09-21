int ret_val;

void draw_brush (int ind, vec3 v, vec3 eye, @inout vec4 color)
{
	if (ind != 0 || eye != '0 0 1' || v != '-1 1 0' || color != '1 1 1 1') {
		ret_val = 1;
		return;
	}
	ret_val = 0;
}

vec4 x;

int main()
{
	auto eye = vec3(-0, 0, 1);
	auto v = vec3(-1, 1, 0);
	auto color = vec4 (1, 1, 1, 1);

	draw_brush (0, v, eye, color);
	return ret_val;
}
