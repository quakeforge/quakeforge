float time;

void (integer x, integer y) spinner =
{
	Draw_Pic (x, y, dot[integer(time * 10) % 6]);
};

void (integer x, integer y, integer width, integer lines) text_box =
{
	local integer cx, cy, n;
	local qpic_t p;

	cx = x;
	cy = y;
	Draw_Pic (cx, cy, box_tl_pic);
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, box_ml_pic);
	}
	Draw_Pic (cx, cy + 8, box_bl_pic);

	cx += 8;
	while (width > 0) {
		cy = y;
		Draw_Pic (cx, cy, box_tm_pic);
		p = box_mm_pic;
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = box_mm2_pic;
			Draw_Pic (cx, cy, p);
		}
		Draw_Pic (cx, cy + 8, box_bm_pic);
		width -= 2;
		cx += 16;
	}

	cy = y;
	Draw_Pic (cx, cy, box_tr_pic);
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, box_mr_pic);
	}
	Draw_Pic (cx, cy + 8, box_br_pic);
};
