qpic_t p_load_pic;
qpic_t p_save_pic;
qpic_t [6] dot;
qpic_t box_tl_pic;
qpic_t box_ml_pic;
qpic_t box_bl_pic;
qpic_t box_tm_pic;
qpic_t box_mm_pic;
qpic_t box_mm2_pic;
qpic_t box_bm_pic;
qpic_t box_tr_pic;
qpic_t box_mr_pic;
qpic_t box_br_pic;
qpic_t qplaque_pic;
qpic_t p_option_pic;
qpic_t p_multi_pic;

string [6] dot_name = {
    "gfx/menudot1.lmp",
    "gfx/menudot2.lmp",
    "gfx/menudot3.lmp",
    "gfx/menudot4.lmp",
    "gfx/menudot5.lmp",
    "gfx/menudot6.lmp",
};

void ()
load_menu_pics =
{
	local integer i;

	for (i = 0; i < 6; i++)
		dot[i] = Draw_CachePic (dot_name[i], 1);

	box_tl_pic = Draw_CachePic ("gfx/box_tl.lmp", 1);
	box_ml_pic = Draw_CachePic ("gfx/box_ml.lmp", 1);
	box_bl_pic = Draw_CachePic ("gfx/box_bl.lmp", 1);
	box_tm_pic = Draw_CachePic ("gfx/box_tm.lmp", 1);
	box_mm_pic = Draw_CachePic ("gfx/box_mm.lmp", 1);
	box_mm2_pic = Draw_CachePic ("gfx/box_mm2.lmp", 1);
	box_bm_pic = Draw_CachePic ("gfx/box_bm.lmp", 1);
	box_tr_pic = Draw_CachePic ("gfx/box_tr.lmp", 1);
	box_mr_pic = Draw_CachePic ("gfx/box_mr.lmp", 1);
	box_br_pic = Draw_CachePic ("gfx/box_br.lmp", 1);

	qplaque_pic = Draw_CachePic ("gfx/qplaque.lmp", 1);
	p_option_pic = Draw_CachePic ("gfx/p_option.lmp", 1);
	p_option_pic = Draw_CachePic ("gfx/p_multi.lmp", 1);
};
