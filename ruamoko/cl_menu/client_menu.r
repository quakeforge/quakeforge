#include "AutoreleasePool.h"
#include "menu.h"
#include "cmd.h"
#include "gib.h"
#include "draw.h"
#include "key.h"
#include "string.h"
#include "cbuf.h"
#include "options.h"
#include "servlist.h"
#include "system.h"
#include "qfs.h"
#include "debug.h"
#include "HUD.h"
#include "client_menu.h"
#include "PropertyList.h"
#include "sound.h"

#include "gui/InputLine.h"
#include "gui/Rect.h"

float () random = #7;

string dot_name[6] = {
	"gfx/menudot1.lmp",
	"gfx/menudot2.lmp",
	"gfx/menudot3.lmp",
	"gfx/menudot4.lmp",
	"gfx/menudot5.lmp",
	"gfx/menudot6.lmp",
};

void ()
menu_enter_sound =
{
	S_LocalSound ("misc/menu2.wav");
}

void ()
menu_leave_sound =
{
	S_LocalSound ("misc/menu2.wav");
}

int (int key, int unicode, int down)
menu_key_sound =
{
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			S_LocalSound ("misc/menu1.wav");
			break;
		case QFK_UP:
		case QFM_WHEEL_UP:
			S_LocalSound ("misc/menu1.wav");
			break;
	}
	return 0;
}

void (int x, int y) spinner =
{
	local int	i = (int) (time * 10) % 6;
	local qpic_t	p = Draw_CachePic (dot_name[i], 1);

	Draw_Pic (x, y, p);
};

int do_single_player;

string quitMessage[32] = {
/*   .........1.........2.... */
	"  Are you gonna quit    ",
	"  this game just like   ",
	"   everything else?     ",
	"                        ",

	" Milord, methinks that  ",
	"   thou art a lowly     ",
	" quitter. Is this true? ",
	"                        ",

	" Do I need to bust your ",
	"  face open for trying  ",
	"        to quit?        ",
	"                        ",

	" Man, I oughta smack you",
	"   for trying to quit!  ",
	"     Press Y to get     ",
	"      smacked out.      ",

	" Press Y to quit like a ",
	"   big loser in life.   ",
	"  Press N to stay proud ",
	"    and successful!     ",

	"   If you press Y to    ",
	"  quit, I will summon   ",
	"  Satan all over your   ",
	"      hard drive!       ",

	"  Um, Asmodeus dislikes ",
	" his children trying to ",
	" quit. Press Y to return",
	"   to your Tinkertoys.  ",

	"  If you quit now, I'll ",
	"  throw a blanket-party ",
	"   for you next time!   ",
	"                        "
};
int quit_index;

// ********* LOAD / SAVE

#define MAX_SAVEGAMES 12
#define MAX_QUICK 5
string filenames[MAX_SAVEGAMES];
int loadable[MAX_SAVEGAMES];
int load_cursor;
int save_cursor;

string (QFile f) get_comment =
{
	local string line;
	local PLItem *plist;

	plist = [PLItem fromFile:f];
	line = [(PLString*) [(PLDictionary*) plist getObjectForKey:"comment"] string];
	return line;
}

void (int quick) scan_saves =
{
	local int	i;
	local QFile		f;
	local string    line;
	local int max = MAX_SAVEGAMES;
	string basename = "s";
	if (quick) {
		max = MAX_QUICK;
		basename = "quick";
	}
	for (i = 0; i < max; i++) {
		if (!filenames[i])
			filenames[i] = str_new ();
		loadable[i] = 0;
		string path = sprintf ("%s%i.sav", basename, i);
		dprint(path + "\n");
		f = QFS_OpenFile (path);
		if (!f) {
			str_copy (filenames[i], "--- UNUSED SLOT ---");
			continue;
		}
		line = Qgetline (f);
		if (line == "QuakeForge\n") {
			str_copy (filenames[i], get_comment (f));
		} else {
			str_copy (filenames[i], Qgetline (f));
		}
		loadable[i] = 1;
		Qclose (f);
	}
	for (i = max; i < MAX_SAVEGAMES; i++) {
		loadable[i] = 0;
	}
};

int (string text, int key) load_f =
{
	scan_saves (0);
	Menu_SelectMenu ("load");
	return 0;
};

int (string text, int key) save_f =
{
	scan_saves (0);
	Menu_SelectMenu ("save");
	return 0;
};

void () load_save_f =
{
	scan_saves (0);
	if (Cmd_Argv (0) == "menu_load")
		Menu_SelectMenu ("load");
	else
		Menu_SelectMenu ("save");
};

int (int x, int y) load_quickbup_draw =
{
	local int i;

	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_load.lmp", 1));
	for (i=0 ; i< MAX_QUICK; i++)
		Draw_String (x + 16, y + 32 + 8 * i, filenames[i]);
	Draw_Character (x + 8, y + 32 + load_cursor * 8,
					12 + ((int) (time * 4) & 1));
	return 1;
};

int (int x, int y) load_draw =
{
	local int i;

	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_load.lmp", 1));
	for (i=0 ; i< MAX_SAVEGAMES; i++)
		Draw_String (x + 16, y + 32 + 8 * i, filenames[i]);
	Draw_String (x + 16, y + 32 + 8 * i, "Quick save backups");
	Draw_Character (x + 8, y + 32 + load_cursor * 8,
					12 + ((int) (time * 4) & 1));
	return 1;
};

int (int x, int y) save_draw =
{
	local int i;

	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_save.lmp", 1));
	for (i=0 ; i< MAX_SAVEGAMES; i++)
		Draw_String (x + 16, y + 32 + 8 * i, filenames[i]);
	Draw_Character (x + 8, y + 32 + save_cursor * 8,
					12 + ((int) (time * 4) & 1));
	return 1;
};

int (int key, int unicode, int down) load_quickbup_keyevent =
{
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			S_LocalSound ("misc/menu1.wav");
			load_cursor++;
			load_cursor %= MAX_QUICK;
			return 1;
		case QFK_UP:
		case QFM_WHEEL_UP:
			S_LocalSound ("misc/menu1.wav");
			load_cursor += MAX_QUICK - 1;
			load_cursor %= MAX_QUICK;
			return 1;
		case QFK_RETURN:
		case QFM_BUTTON1:
			if (loadable[load_cursor]) {
				S_LocalSound ("misc/menu2.wav");
				Menu_SelectMenu (nil);
				Cbuf_AddText (sprintf ("load quick%i.sav\n", load_cursor));
				load_cursor = MAX_SAVEGAMES;
			}
			return 1;
	}
	return 0;
};

int (int key, int unicode, int down) load_keyevent =
{
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			S_LocalSound ("misc/menu1.wav");
			load_cursor++;
			load_cursor %= MAX_SAVEGAMES + 1;
			return 1;
		case QFK_UP:
		case QFM_WHEEL_UP:
			S_LocalSound ("misc/menu1.wav");
			load_cursor += MAX_SAVEGAMES;
			load_cursor %= MAX_SAVEGAMES + 1;
			return 1;
		case QFK_RETURN:
		case QFM_BUTTON1:
			if (load_cursor == MAX_SAVEGAMES) {
				load_cursor = 0;
				scan_saves (1);
				Menu_SelectMenu ("load_quickbup");
			} else if (loadable[load_cursor]) {
				S_LocalSound ("misc/menu2.wav");
				Menu_SelectMenu (nil);
				Cbuf_AddText (sprintf ("load s%i.sav\n", load_cursor));
			}
			return 1;
	}
	return 0;
};

int (int key, int unicode, int down) save_keyevent =
{
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			S_LocalSound ("misc/menu1.wav");
			save_cursor++;
			save_cursor %= MAX_SAVEGAMES;
			return 1;
		case QFK_UP:
		case QFM_WHEEL_UP:
			S_LocalSound ("misc/menu1.wav");
			save_cursor += MAX_SAVEGAMES - 1;
			save_cursor %= MAX_SAVEGAMES;
			return 1;
		case QFK_RETURN:
		case QFM_BUTTON1:
			Menu_SelectMenu (nil);
			Cbuf_AddText (sprintf ("save s%i.sav\n", save_cursor));
			return 1;
	}
	return 0;
};

void () load_quickbup_menu =
{
	Menu_Begin (0, 0, "load_quickbup");
	Menu_EnterHook (menu_enter_sound);
	Menu_LeaveHook (menu_leave_sound);
	Menu_FadeScreen (1);
	Menu_KeyEvent (load_quickbup_keyevent);
	Menu_Draw (load_quickbup_draw);
	Menu_End ();
};

void () load_menu =
{
	Menu_Begin (0, 0, "load");
	Menu_EnterHook (menu_enter_sound);
	Menu_LeaveHook (menu_leave_sound);
	Menu_FadeScreen (1);
	Menu_KeyEvent (load_keyevent);
	Menu_Draw (load_draw);
	Menu_End ();
	Cmd_AddCommand ("menu_load", load_save_f);
};

void () save_menu =
{
	Menu_Begin (0, 0, "save");
	Menu_EnterHook (menu_enter_sound);
	Menu_LeaveHook (menu_leave_sound);
	Menu_FadeScreen (1);
	Menu_KeyEvent (save_keyevent);
	Menu_Draw (save_draw);
	Menu_End ();
	Cmd_AddCommand ("menu_save", load_save_f);
};

// ********* QUIT

int () quit =
{
	Menu_SelectMenu ("quit");
	quit_index = (int) (random () * 8);
	quit_index &= 7;
	return 0;
};

int (string text, int key) quit_f =
{
	quit ();
	return 0;
};

int (int key, int unicode, int down) quit_keyevent =
{
	if (key == 'y') {
		Menu_Quit ();
		return 1;
	}
	if (key == 'n') {
		Menu_SelectMenu (nil);
		return 1;
	}
	return 0;
};

int (int x, int y) quit_draw =
{
	text_box (x + 56, y + 76, 24, 4);
	Draw_String (x + 64, y + 84, quitMessage[quit_index * 4 + 0]);
	Draw_String (x + 64, y + 92, quitMessage[quit_index * 4 + 1]);
	Draw_String (x + 64, y + 100, quitMessage[quit_index * 4 + 2]);
	Draw_String (x + 64, y + 108, quitMessage[quit_index * 4 + 3]);
	return 1;
};

void () quit_menu =
{
	Menu_Begin (0, 0, "quit");
	Menu_EnterHook (menu_enter_sound);
	Menu_LeaveHook (menu_leave_sound);
	Menu_FadeScreen (1);
	Menu_KeyEvent (quit_keyevent);
	Menu_Draw (quit_draw);
	Menu_End ();
};

int (string text, int key) sp_start =
{
	Menu_SelectMenu (nil);
	Cbuf_AddText ("disconnect\n");
	Cbuf_AddText ("maxplayers 1\n");
	Cbuf_AddText ("coop 0\n");
	Cbuf_AddText ("deathmatch 0\n");
	Cbuf_AddText ("teamplay 0\n");
	Cbuf_AddText ("listen 0\n");
	Cbuf_AddText ("noexit 0\n");
	Cbuf_AddText ("samelevel 0\n");
	Cbuf_AddText ("map start\n");
	return 0;
};

void () single_player_menu =
{
	Menu_Begin (54, 32, "");
	Menu_EnterHook (menu_enter_sound);
	Menu_LeaveHook (menu_leave_sound);
	Menu_KeyEvent (menu_key_sound);
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/ttl_sgl.lmp");
	Menu_Pic (72, 32, "gfx/sp_menu.lmp");
	Menu_Cursor (spinner);
	Menu_Item (54, 32, "", sp_start, 0);
	Menu_Item (54, 52, "", load_f, 0);
	Menu_Item (54, 72, "", save_f, 0);
	Menu_End ();
};

// ********* MULTIPLAYER

int JoiningGame;
int lanConfig_cursor;
string my_tcpip_address;
string lanConfig_portname;
string lanConfig_joinname;
#define NUM_LANCONFIG_CMDS 3
int lanConfig_cursor_table[NUM_LANCONFIG_CMDS] = { 72, 92, 124 };
InputLine *lanConfig_port_il;
InputLine *lanConfig_join_il;
InputLine *input_active;

int (int x, int y) join_draw =
{
	local int f = x + (320 - 26 * 8) / 2;
	text_box (f, y + 134, 24, 4);
	Draw_String (f, y + 142, " Commonly used to play  ");
	Draw_String (f, y + 150, " over the Internet, but ");
	Draw_String (f, y + 158, " also used on a Local   ");
	Draw_String (f, y + 166, " Area Network.          ");
	return 0;
};

int (int x, int y) lanconfig_draw =
{
	local int basex = 54 + x;
	local string startJoin = JoiningGame ? "Join Game" : "New Game";
	local string protocol = "UDP";

	Draw_String (basex, y + 32, sprintf ("%s - %s", startJoin, protocol));
	basex += 8;
	Draw_String (basex, y + 52, "Address:");
	Draw_String (basex + 9 * 8, y + 52, "127.0.0.1");
	Draw_String (basex, y + lanConfig_cursor_table[0], "Port");
	[lanConfig_port_il setBasePos:x y:y];
	[lanConfig_port_il cursor:lanConfig_cursor == 0 && input_active];
	[lanConfig_port_il draw];
	Draw_String (basex + 9 * 8, y + lanConfig_cursor_table[0],
				 lanConfig_portname);

	if (JoiningGame) {
		Draw_String (basex, y + lanConfig_cursor_table[1], "Search for local "
					 "games...");
		Draw_String (basex, y + 108, "Join game at:");
		[lanConfig_join_il setBasePos:x y:y];
		[lanConfig_join_il cursor:lanConfig_cursor == 2 && input_active];
		[lanConfig_join_il draw];
		Draw_String (basex + 16, y + lanConfig_cursor_table[2],
					 lanConfig_joinname);
	} else {
		text_box (basex, y + lanConfig_cursor_table[1] - 8, 2, 1);
		Draw_String (basex + 8, y + lanConfig_cursor_table[1], "OK");
	}
	if (!input_active)
		Draw_Character (basex - 8, y + lanConfig_cursor_table[lanConfig_cursor],
						12 + ((int) (time * 4) & 1));

	return 0;
};

int (int key, int unicode, int down) lanconfig_keyevent =
{
	if (input_active)
		[input_active processInput:(key >= 256 ? key : unicode)];
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			if (!input_active) {
				S_LocalSound ("misc/menu2.wav");
				lanConfig_cursor ++;
				lanConfig_cursor %= NUM_LANCONFIG_CMDS;
			}
			return 1;
		case QFK_UP:
		case QFM_WHEEL_UP:
			if (!input_active) {
				S_LocalSound ("misc/menu2.wav");
				lanConfig_cursor += NUM_LANCONFIG_CMDS - 1;
				lanConfig_cursor %= NUM_LANCONFIG_CMDS;
			}
			return 1;
		case QFK_RETURN:
			if (input_active) {
				input_active = nil;
			} else {
				if (lanConfig_cursor == 0) {
					input_active = lanConfig_port_il;
				} else if (JoiningGame) {
					if (lanConfig_cursor == 2) {
						input_active = lanConfig_join_il;
					}
				}
			}
			return 1;
	}
	return 0;
};

void () lanconfig_menu =
{
	Menu_Begin (54, 92, "");
	Menu_EnterHook (menu_enter_sound);
	Menu_LeaveHook (menu_leave_sound);
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	Menu_Draw (lanconfig_draw);
	Menu_KeyEvent (lanconfig_keyevent);
	Menu_End ();
};

void () join_menu =
{
	Menu_Begin (54, 32, "");
	Menu_EnterHook (menu_enter_sound);
	Menu_LeaveHook (menu_leave_sound);
	Menu_KeyEvent (menu_key_sound);
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	Menu_Pic (72, 32, "gfx/dim_modm.lmp");
	Menu_Pic (72, 51, "gfx/dim_drct.lmp");
	Menu_Pic (72, 70, "gfx/dim_ipx.lmp");
	Menu_Pic (72, 89, "gfx/netmen4.lmp");
	lanconfig_menu ();
	Menu_Draw (join_draw);
	Menu_Cursor (spinner);
	Menu_End ();
};

int (int key, int unicode, int down) multi_player_keyevent =
{
	if (key == QFK_RETURN) {
		JoiningGame = (Menu_GetIndex () == 0);
	}
	return 0;
};

void () multi_player_menu =
{
	Menu_Begin (54, 52, "");
	Menu_EnterHook (menu_enter_sound);
	Menu_LeaveHook (menu_leave_sound);
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_multi.lmp");
	Menu_Pic (72, 32, "gfx/mp_menu.lmp");
	Menu_KeyEvent (multi_player_keyevent);
	join_menu ();
	if (do_single_player)
		Menu_Item (54, 52, "", quit_f, 0);
	Menu_Item (54, 72, "", quit_f, 0);
	Menu_Cursor (spinner);
	Menu_End ();
};



void () help_menu =
{
	Menu_Item (54, 92, "", quit_f, 0);
};

void () main_menu =
{
	Menu_Begin (0, 0, "main");
	Menu_EnterHook (menu_enter_sound);
	Menu_LeaveHook (menu_leave_sound);
	Menu_KeyEvent (menu_key_sound);
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/ttl_main.lmp");
	if (do_single_player)
		Menu_Pic (71,32, "gfx/mainmenu.lmp");
	else
		Menu_SubPic (71,52, "gfx/mainmenu.lmp", 0, 20, 240, 92);
	Menu_Cursor (spinner);
	if (do_single_player)
		single_player_menu ();
	switch (gametype ()) {
	case "netquake":
		multi_player_menu ();
		break;
	case "quakeworld":
		server_list_menu ();
		break;
	default:
		break;
	}
	MENU_options ();
	help_menu ();
	Menu_Item (54, 112, "", quit_f, 0);
	Menu_End ();
};

void () menu_init =
{
	lanConfig_port_il = [[InputLineBox alloc] initWithBounds:makeRect (126, lanConfig_cursor_table[0] - 8, 8, 4) promptCharacter:' '];
	[lanConfig_port_il setWidth:10];
	lanConfig_join_il = [[InputLineBox alloc] initWithBounds:makeRect (70, lanConfig_cursor_table[2] - 8, 24, 4) promptCharacter:' '];
	[lanConfig_join_il setWidth:26];
	switch (gametype ()) {
	case "netquake":
		do_single_player = 1;
		break;
	case "quakeworld":
		do_single_player = 0;
		break;
	default:
		break;
	}
	main_menu ();
	quit_menu ();
	load_menu ();
	load_quickbup_menu ();
	save_menu ();

	Menu_TopMenu ("main");
	Menu_SetQuit (quit);

};

void () menu_draw_hud =
{
};

@static AutoreleasePool *autorelease_pool;

void menu_pre ()
{
	autorelease_pool = [[AutoreleasePool alloc] init];
}

void menu_post ()
{
	//FIXME called too often by the engine?
	AutoreleasePool *ar;

	ar = autorelease_pool;
	autorelease_pool = nil;
	[ar release];
}
