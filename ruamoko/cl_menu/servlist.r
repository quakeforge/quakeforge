#include "menu.h"
#include "draw.h"
#include "options_util.h"
#include "controls_o.h"
#include "client_menu.h"

#include "gui/InputLine.h"
#include "gui/Rect.h"

int (int x, int y) servlist_favorates_draw =
{
	Draw_Pic (x + 16, y + 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_multi.lmp", 1));
	Draw_String (x + 54, y + 40, "Under Construction");
	return 1;
};

int (int x, int y) servlist_all_draw =
{
	Draw_Pic (x + 16, y + 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_multi.lmp", 1));
	Draw_String (x + 54, y + 40, "Under Construction");
	return 1;
};

int serv_nfull;
int serv_nempty;
InputLine *serv_maxping;
InputLine *serv_game;

int (int x, int y) servlist_filter_draw =
{
    Draw_Pic (x + 16, y + 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_multi.lmp", 1));
	Draw_String	(x + 62, y + 40, "Max Ping........:");
	text_box (x + 206, y + 32, 4, 1);
	[serv_maxping setBasePos:x y:y];
	[serv_maxping cursor:1];
	[serv_maxping draw];
	Draw_String (x + 62, y + 56, "Game Contains...:");
	text_box (x + 206, y + 48, 8, 1);
	Draw_String (x + 62, y + 72, "Server Not Full.:");
	Draw_String (x + 206, y + 72, ((serv_nfull == 0) ? "No" : "Yes"));
	Draw_String (x + 62, y + 88, "Server Not Empty:");
	Draw_String (x + 206, y + 88, ((serv_nempty == 0) ? "No" : "Yes"));
	Draw_String (x + 62, y + 96, "Under Construction");
	opt_cursor (x + 54, y + (Menu_GetIndex () * 16) + 40);
	return 1;
};

void () servlist_favorates_menu =
{
	Menu_Begin (54, 40, "favorites");
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

int (string text, int key) sl_filter_in =
{
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
	serv_maxping = [[InputLine alloc] initWithBounds:makeRect (206, 40, 8, 4) promptCharacter:' '];
	[serv_maxping setWidth:5];
	
	Menu_Begin (54, 52, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	servlist_favorates_menu ();
	servlist_all_menu ();
	servlist_filter_menu ();
	Menu_End ();
};
