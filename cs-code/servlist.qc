#include "menu.h"
#include "draw.h"
#include "inputline.h"
#include "options_util.h"
#include "controls_o.h"
#include "client_menu.h"

integer () servlist_favorates_draw =
{
	Draw_Pic        (16, 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (160, 4, Draw_CachePic ("gfx/p_multi.lmp", 1));
	Draw_String     (54,  40, "Under Construction");
	return 1;
};

integer () servlist_all_draw =
{
	Draw_Pic        (16, 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (160, 4, Draw_CachePic ("gfx/p_multi.lmp", 1));
	Draw_String     (54,  40, "Under Construction");
	return 1;
};

integer serv_nfull; 
integer serv_nempty;
inputline_t serv_maxping;
inputline_t serv_game;

integer () servlist_filter_draw =
{
    Draw_Pic (16, 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (160, 4, Draw_CachePic ("gfx/p_multi.lmp", 1));
	Draw_String	(62, 40, "Max Ping........:");
	text_box (206, 32, 4, 1);
	InputLine_Draw (serv_maxping, 206, 40, 1);
	Draw_String (62, 56, "Game Contains...:");
	text_box (206, 48, 8, 1);
	Draw_String (62, 72, "Server Not Full.:");
	Draw_String (206, 72, ((serv_nfull == 0)?"No":"Yes"));
	Draw_String (62, 88, "Server Not Empty:");
	Draw_String (206, 88, ((serv_nempty == 0)?"No":"Yes"));
	Draw_String (62, 96, "Under Construction");
	opt_cursor  (54, (Menu_GetIndex() * 16) + 40);
	return 1;
};

void () servlist_favorates_menu =
{
	Menu_Begin (54, 40, "favorates");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	Menu_Draw (servlist_favorates_draw);			
	Menu_End ();
};

void () servlist_all_menu =
{
	Menu_Begin (54, 48, "all");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	Menu_Draw (servlist_all_draw);
	Menu_End ();					
};

integer (string text, integer key) sl_filter_in =
{
	load_keybindings();
	switch (text) {
	case "isnfull":
		serv_nfull ^= 1;
		break;
	case "isnempty":
		serv_nempty ^= 1;
		break;
	default:
		break;
	}
	return 0;
};

void () servlist_filter_menu =
{
	Menu_Begin (54, 56, "filter");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	Menu_Item (62, 40, "ping", sl_filter_in, 0);
	Menu_Item (62, 48, "gametext", sl_filter_in, 0);
	Menu_Item (62, 56, "isnfull", sl_filter_in, 0);
	Menu_Item (62, 64, "isnempty", sl_filter_in, 0);
	Menu_Draw (servlist_filter_draw);
	Menu_End ();
};

void () server_list_menu =
{
	serv_maxping = InputLine_Create (4, 8, ' ');
	InputLine_SetWidth (serv_maxping, 5);
	
	Menu_Begin (54, 52, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	servlist_favorates_menu ();
	servlist_all_menu ();
	servlist_filter_menu ();
	Menu_End ();
};

