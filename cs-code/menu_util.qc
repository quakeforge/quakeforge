float time;

string [6] dot = {
    "gfx/menudot1.lmp",
    "gfx/menudot2.lmp",
    "gfx/menudot3.lmp",
    "gfx/menudot4.lmp",
    "gfx/menudot5.lmp",
    "gfx/menudot6.lmp",
};

void (integer x, integer y) spinner =
{
	Draw_Pic (x, y, dot[integer(time * 10) % 6]);
};

void (integer x, integer y, integer width, integer lines) text_box =
{
	local integer cx, cy, n;
	local string p;

	cx = x;
	cy = y;
	Draw_Pic (cx, cy, "gfx/box_tl.lmp");
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, "gfx/box_ml.lmp");
	}
	Draw_Pic (cx, cy + 8, "gfx/box_bl.lmp");

	cx += 8;
	while (width > 0) {
		cy = y;
		Draw_Pic (cx, cy, "gfx/box_tm.lmp");
		p = "gfx/box_mm.lmp";
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = "gfx/box_mm2.lmp";
			Draw_Pic (cx, cy, p);
		}
		Draw_Pic (cx, cy + 8, "gfx/box_bm.lmp");
		width -= 2;
		cx += 16;
	}

	cy = y;
	Draw_Pic (cx, cy, "gfx/box_tr.lmp");
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, "gfx/box_mr.lmp");
	}
	Draw_Pic (cx, cy + 8, "gfx/box_br.lmp");
};
