void (integer x, integer y, string text) Menu_Begin = #0;
void (integer x, integer y, string name) Menu_Pic = #0;
void (integer x, integer y, string name) Menu_CenterPic = #0;
void (integer x, integer y, string text, void (string text, integer key) func) Menu_Item = #0;
void (void (integer x, integer y) func) Menu_Cursor = #0;
void () Menu_End = #0;

void (integer x, integer y, string name) Draw_Pic = #0;

float time;
entity self;
.float nextthink;
.float frame;
.void () think;

string [6] dot = {
	"gfx/menudot0.lpm",
	"gfx/menudot1.lpm",
	"gfx/menudot2.lpm",
	"gfx/menudot3.lpm",
	"gfx/menudot4.lpm",
	"gfx/menudot5.lpm",
};

void (integer x, integer y) spinner =
{
	Draw_Pic (x, y, dot[integer(time * 10) % 6]);
};

void (string text, integer key) quit =
{
};

void () main_menu =
{
	Menu_Begin (0, 0, "");
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/ttl_main.lmp");
	Menu_Pic (71,32, "gfx/mainmenu.lmp");
	Menu_Cursor (spinner);
	//if (do_single_player)
	//	single_player_menu ();
	//multi_player_menu ();
	//options_menu ();
	//help_menu ();
	Menu_Item (54, 132, "", quit);
	Menu_End ();
};

void () menu_init =
{
	main_menu ();
};
