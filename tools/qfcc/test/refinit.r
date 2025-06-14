#pragma optimize on
#pragma bug die
#pragma warn error

typedef union imui_color_s {
	struct {
		unsigned    normal;
		unsigned    hot;
		unsigned    active;
	};
	unsigned    color[3];
} imui_color_t;

typedef struct imui_style_s {
	imui_color_t background;
	imui_color_t foreground;
	imui_color_t text;
} imui_style_t;

void get_style (imui_style_t *s) = #0;

int do_something (bool x)
{
	int y;
	imui_style_t style;
	get_style (&style);
	y = style.background.normal;
	if (x) {
		return style.foreground.normal;
	}
	return y;
}

int main()
{
	return 0;
}
