float time;

void (integer x, integer y) spinner =
{
	local integer i = integer(time * 10) % 6;
	local qpic_t p = Draw_CachePic (dot_name[i], 1);

	Draw_Pic (x, y, p);
};

void (integer x, integer y, integer width, integer lines) text_box =
{
	local integer cx, cy, n;
	local qpic_t p;

	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp", 1);
	Draw_Pic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp", 1);
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp", 1);
	Draw_Pic (cx, cy + 8, p);

	cx += 8;
	while (width > 0) {
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp", 1);
		Draw_Pic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp", 1);
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp", 1);
			Draw_Pic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp", 1);
		Draw_Pic (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp", 1);
	Draw_Pic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp", 1);
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp", 1);
	Draw_Pic (cx, cy + 8, p);
};
