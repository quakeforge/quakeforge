/*
	keys.h

	Key definitions and prototypes

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/


#ifndef _KEYS_H
#define _KEYS_H

#ifndef __QFCC__
# include "QF/qtypes.h"
# include "QF/quakeio.h"
#endif

/** \defgroup input Input Sub-system */

/** \defgroup input_keybinding Key Binding Sub-system
	\ingroup input
*/
///@{

/// these are the key numbers that should be passed to Key_Event
typedef enum {
	/* The keyboard syms have been cleverly chosen to map to ASCII */
	QFK_UNKNOWN		= 0,
	QFK_FIRST			= 0,
	QFK_BACKSPACE		= 8,
	QFK_TAB			= 9,
	QFK_CLEAR			= 12,
	QFK_RETURN		= 13,
	QFK_PAUSE			= 19,
	QFK_ESCAPE		= 27,
	QFK_SPACE			= 32,
	QFK_EXCLAIM		= 33,
	QFK_QUOTEDBL		= 34,
	QFK_HASH			= 35,
	QFK_DOLLAR		= 36,
	QFK_PERCENT		= 37,
	QFK_AMPERSAND		= 38,
	QFK_QUOTE			= 39,
	QFK_LEFTPAREN		= 40,
	QFK_RIGHTPAREN	= 41,
	QFK_ASTERISK		= 42,
	QFK_PLUS			= 43,
	QFK_COMMA			= 44,
	QFK_MINUS			= 45,
	QFK_PERIOD		= 46,
	QFK_SLASH			= 47,
	QFK_0				= 48,
	QFK_1				= 49,
	QFK_2				= 50,
	QFK_3				= 51,
	QFK_4				= 52,
	QFK_5				= 53,
	QFK_6				= 54,
	QFK_7				= 55,
	QFK_8				= 56,
	QFK_9				= 57,
	QFK_COLON			= 58,
	QFK_SEMICOLON		= 59,
	QFK_LESS			= 60,
	QFK_EQUALS		= 61,
	QFK_GREATER		= 62,
	QFK_QUESTION		= 63,
	QFK_AT			= 64,
	/*
	   Skip uppercase letters
	 */
	QFK_LEFTBRACKET	= 91,
	QFK_BACKSLASH		= 92,
	QFK_RIGHTBRACKET	= 93,
	QFK_CARET			= 94,
	QFK_UNDERSCORE	= 95,
	QFK_BACKQUOTE		= 96,
	QFK_a				= 97,
	QFK_b				= 98,
	QFK_c				= 99,
	QFK_d				= 100,
	QFK_e				= 101,
	QFK_f				= 102,
	QFK_g				= 103,
	QFK_h				= 104,
	QFK_i				= 105,
	QFK_j				= 106,
	QFK_k				= 107,
	QFK_l				= 108,
	QFK_m				= 109,
	QFK_n				= 110,
	QFK_o				= 111,
	QFK_p				= 112,
	QFK_q				= 113,
	QFK_r				= 114,
	QFK_s				= 115,
	QFK_t				= 116,
	QFK_u				= 117,
	QFK_v				= 118,
	QFK_w				= 119,
	QFK_x				= 120,
	QFK_y				= 121,
	QFK_z				= 122,
	QFK_BRACELEFT		= 123,
	QFK_BAR				= 124,
	QFK_BRACERIGHT		= 125,
	QFK_ASCIITILDE		= 126,
	QFK_DELETE			= 127,
	/* End of ASCII mapped keysyms */

	/* International keyboard syms */
	QFK_WORLD_0		= 160,		/* 0xA0 */
	QFK_WORLD_1		= 161,
	QFK_WORLD_2		= 162,
	QFK_WORLD_3		= 163,
	QFK_WORLD_4		= 164,
	QFK_WORLD_5		= 165,
	QFK_WORLD_6		= 166,
	QFK_WORLD_7		= 167,
	QFK_WORLD_8		= 168,
	QFK_WORLD_9		= 169,
	QFK_WORLD_10		= 170,
	QFK_WORLD_11		= 171,
	QFK_WORLD_12		= 172,
	QFK_WORLD_13		= 173,
	QFK_WORLD_14		= 174,
	QFK_WORLD_15		= 175,
	QFK_WORLD_16		= 176,
	QFK_WORLD_17		= 177,
	QFK_WORLD_18		= 178,
	QFK_WORLD_19		= 179,
	QFK_WORLD_20		= 180,
	QFK_WORLD_21		= 181,
	QFK_WORLD_22		= 182,
	QFK_WORLD_23		= 183,
	QFK_WORLD_24		= 184,
	QFK_WORLD_25		= 185,
	QFK_WORLD_26		= 186,
	QFK_WORLD_27		= 187,
	QFK_WORLD_28		= 188,
	QFK_WORLD_29		= 189,
	QFK_WORLD_30		= 190,
	QFK_WORLD_31		= 191,
	QFK_WORLD_32		= 192,
	QFK_WORLD_33		= 193,
	QFK_WORLD_34		= 194,
	QFK_WORLD_35		= 195,
	QFK_WORLD_36		= 196,
	QFK_WORLD_37		= 197,
	QFK_WORLD_38		= 198,
	QFK_WORLD_39		= 199,
	QFK_WORLD_40		= 200,
	QFK_WORLD_41		= 201,
	QFK_WORLD_42		= 202,
	QFK_WORLD_43		= 203,
	QFK_WORLD_44		= 204,
	QFK_WORLD_45		= 205,
	QFK_WORLD_46		= 206,
	QFK_WORLD_47		= 207,
	QFK_WORLD_48		= 208,
	QFK_WORLD_49		= 209,
	QFK_WORLD_50		= 210,
	QFK_WORLD_51		= 211,
	QFK_WORLD_52		= 212,
	QFK_WORLD_53		= 213,
	QFK_WORLD_54		= 214,
	QFK_WORLD_55		= 215,
	QFK_WORLD_56		= 216,
	QFK_WORLD_57		= 217,
	QFK_WORLD_58		= 218,
	QFK_WORLD_59		= 219,
	QFK_WORLD_60		= 220,
	QFK_WORLD_61		= 221,
	QFK_WORLD_62		= 222,
	QFK_WORLD_63		= 223,
	QFK_WORLD_64		= 224,
	QFK_WORLD_65		= 225,
	QFK_WORLD_66		= 226,
	QFK_WORLD_67		= 227,
	QFK_WORLD_68		= 228,
	QFK_WORLD_69		= 229,
	QFK_WORLD_70		= 230,
	QFK_WORLD_71		= 231,
	QFK_WORLD_72		= 232,
	QFK_WORLD_73		= 233,
	QFK_WORLD_74		= 234,
	QFK_WORLD_75		= 235,
	QFK_WORLD_76		= 236,
	QFK_WORLD_77		= 237,
	QFK_WORLD_78		= 238,
	QFK_WORLD_79		= 239,
	QFK_WORLD_80		= 240,
	QFK_WORLD_81		= 241,
	QFK_WORLD_82		= 242,
	QFK_WORLD_83		= 243,
	QFK_WORLD_84		= 244,
	QFK_WORLD_85		= 245,
	QFK_WORLD_86		= 246,
	QFK_WORLD_87		= 247,
	QFK_WORLD_88		= 248,
	QFK_WORLD_89		= 249,
	QFK_WORLD_90		= 250,
	QFK_WORLD_91		= 251,
	QFK_WORLD_92		= 252,
	QFK_WORLD_93		= 253,
	QFK_WORLD_94		= 254,
	QFK_WORLD_95		= 255,		/* 0xFF */

	/* Numeric keypad */
	QFK_KP0			= 256,
	QFK_KP1			= 257,
	QFK_KP2			= 258,
	QFK_KP3			= 259,
	QFK_KP4			= 260,
	QFK_KP5			= 261,
	QFK_KP6			= 262,
	QFK_KP7			= 263,
	QFK_KP8			= 264,
	QFK_KP9			= 265,
	QFK_KP_PERIOD		= 266,
	QFK_KP_DIVIDE		= 267,
	QFK_KP_MULTIPLY	= 268,
	QFK_KP_MINUS		= 269,
	QFK_KP_PLUS		= 270,
	QFK_KP_ENTER		= 271,
	QFK_KP_EQUALS		= 272,

	/* Arrows + Home/End pad */
	QFK_UP			= 273,
	QFK_DOWN			= 274,
	QFK_RIGHT			= 275,
	QFK_LEFT			= 276,
	QFK_INSERT		= 277,
	QFK_HOME			= 278,
	QFK_END			= 279,
	QFK_PAGEUP		= 280,
	QFK_PAGEDOWN		= 281,

	/* Function keys */
	QFK_F1			= 282,
	QFK_F2			= 283,
	QFK_F3			= 284,
	QFK_F4			= 285,
	QFK_F5			= 286,
	QFK_F6			= 287,
	QFK_F7			= 288,
	QFK_F8			= 289,
	QFK_F9			= 290,
	QFK_F10			= 291,
	QFK_F11			= 292,
	QFK_F12			= 293,
	QFK_F13			= 294,
	QFK_F14			= 295,
	QFK_F15			= 296,

	/* Key state modifier keys */
	QFK_NUMLOCK		= 300,
	QFK_CAPSLOCK		= 301,
	QFK_SCROLLOCK		= 302,
	QFK_RSHIFT		= 303,
	QFK_LSHIFT		= 304,
	QFK_RCTRL			= 305,
	QFK_LCTRL			= 306,
	QFK_RALT			= 307,
	QFK_LALT			= 308,
	QFK_RMETA			= 309,
	QFK_LMETA			= 310,
	QFK_LSUPER		= 311,		/* Left "Windows" key */
	QFK_RSUPER		= 312,		/* Right "Windows" key */
	QFK_MODE			= 313,		/* "Alt Gr" key */
	QFK_COMPOSE		= 314,		/* Multi-key compose key */

	/* Miscellaneous function keys */
	QFK_HELP			= 315,
	QFK_PRINT			= 316,
	QFK_SYSREQ		= 317,
	QFK_BREAK			= 318,
	QFK_MENU			= 319,
	QFK_POWER			= 320,		/* Power Macintosh power key */
	QFK_EURO			= 321,		/* Some european keyboards */
	QFK_UNDO			= 322,

	/* Japanese keys */
	QFK_KANJI,						/* Kanji, Kanji convert */
	QFK_MUHENKAN,					/* Cancel Conversion */
	QFK_HENKAN,						/* Alias for Henkan_Mode */
	QFK_ROMAJI,						/* to Romaji */
	QFK_HIRAGANA,					/* to Hiragana */
	QFK_KATAKANA,					/* to Katakana */
	QFK_HIRAGANA_KATAKANA,			/* Hiragana/Katakana toggle */
	QFK_ZENKAKU,					/* to Zenkaku */
	QFK_HANKAKU,					/* to Hankaku */
	QFK_ZENKAKU_HANKAKU,			/* Zenkaku/Hankaku toggle */
	QFK_TOUROKU,					/* Add to Dictionary */
	QFK_MASSYO,						/* Delete from Dictionary */
	QFK_KANA_LOCK,					/* Kana Lock */
	QFK_KANA_SHIFT,					/* Kana Shift */
	QFK_EISU_SHIFT,					/* Alphanumeric Shift */
	QFK_EISU_TOGGLE,				/* Alphanumeric toggle */
	QFK_KANJI_BANGOU,				/* Codeinput */
	QFK_ZEN_KOHO,					/* Multiple/All Candidate(s) */
	QFK_MAE_KOHO,					/* Previous Candidate */

	/* some multi-media/browser keys */
	QFK_HOMEPAGE,
	QFK_SEARCH,
	QFK_MAIL,
	QFK_FAVORITES,
	QFK_AUDIOMUTE,
	QFK_AUDIOLOWERVOLUME,
	QFK_AUDIORAISEVOLUME,
	QFK_AUDIOPLAY,
	QFK_CALCULATOR,
	QFK_REDO,
	QFK_NEW,
	QFK_RELOAD,
	QFK_OPEN,
	QFK_CLOSE,
	QFK_REPLY,
	QFK_MAILFORWARD,
	QFK_SEND,
	QFK_SAVE,
	QFK_BACK,
	QFK_FORWARD,

	/* Add any other keys here */

//
// mouse buttons generate virtual keys
//
	QFM_BUTTON1,
	QFM_BUTTON2,
	QFM_BUTTON3,
	QFM_WHEEL_UP,
	QFM_WHEEL_DOWN,
	QFM_BUTTON6,
	QFM_BUTTON7,
	QFM_BUTTON8,
	QFM_BUTTON9,
	QFM_BUTTON10,
	QFM_BUTTON11,
	QFM_BUTTON12,
	QFM_BUTTON13,
	QFM_BUTTON14,
	QFM_BUTTON15,
	QFM_BUTTON16,
	QFM_BUTTON17,
	QFM_BUTTON18,
	QFM_BUTTON19,
	QFM_BUTTON20,
	QFM_BUTTON21,
	QFM_BUTTON22,
	QFM_BUTTON23,
	QFM_BUTTON24,
	QFM_BUTTON25,
	QFM_BUTTON26,
	QFM_BUTTON27,
	QFM_BUTTON28,
	QFM_BUTTON29,
	QFM_BUTTON30,
	QFM_BUTTON31,
	QFM_BUTTON32,

//
// joystick buttons
//
	QFJ_BUTTON1,
	QFJ_BUTTON2,
	QFJ_BUTTON3,
	QFJ_BUTTON4,
	QFJ_BUTTON5,
	QFJ_BUTTON6,
	QFJ_BUTTON7,
	QFJ_BUTTON8,
	QFJ_BUTTON9,
	QFJ_BUTTON10,
	QFJ_BUTTON11,
	QFJ_BUTTON12,
	QFJ_BUTTON13,
	QFJ_BUTTON14,
	QFJ_BUTTON15,
	QFJ_BUTTON16,
	QFJ_BUTTON17,
	QFJ_BUTTON18,
	QFJ_BUTTON19,
	QFJ_BUTTON20,
	QFJ_BUTTON21,
	QFJ_BUTTON22,
	QFJ_BUTTON23,
	QFJ_BUTTON24,
	QFJ_BUTTON25,
	QFJ_BUTTON26,
	QFJ_BUTTON27,
	QFJ_BUTTON28,
	QFJ_BUTTON29,
	QFJ_BUTTON30,
	QFJ_BUTTON31,
	QFJ_BUTTON32,
	QFJ_BUTTON33,
	QFJ_BUTTON34,
	QFJ_BUTTON35,
	QFJ_BUTTON36,
	QFJ_BUTTON37,
	QFJ_BUTTON38,
	QFJ_BUTTON39,
	QFJ_BUTTON40,
	QFJ_BUTTON41,
	QFJ_BUTTON42,
	QFJ_BUTTON43,
	QFJ_BUTTON44,
	QFJ_BUTTON45,
	QFJ_BUTTON46,
	QFJ_BUTTON47,
	QFJ_BUTTON48,
	QFJ_BUTTON49,
	QFJ_BUTTON50,
	QFJ_BUTTON51,
	QFJ_BUTTON52,
	QFJ_BUTTON53,
	QFJ_BUTTON54,
	QFJ_BUTTON55,
	QFJ_BUTTON56,
	QFJ_BUTTON57,
	QFJ_BUTTON58,
	QFJ_BUTTON59,
	QFJ_BUTTON60,
	QFJ_BUTTON61,
	QFJ_BUTTON62,
	QFJ_BUTTON63,
	QFJ_BUTTON64,

//
// joystick axes (for button emulation without consuming buttons)
//
	QFJ_AXIS1,
	QFJ_AXIS2,
	QFJ_AXIS3,
	QFJ_AXIS4,
	QFJ_AXIS5,
	QFJ_AXIS6,
	QFJ_AXIS7,
	QFJ_AXIS8,
	QFJ_AXIS9,
	QFJ_AXIS10,
	QFJ_AXIS11,
	QFJ_AXIS12,
	QFJ_AXIS13,
	QFJ_AXIS14,
	QFJ_AXIS15,
	QFJ_AXIS16,
	QFJ_AXIS17,
	QFJ_AXIS18,
	QFJ_AXIS19,
	QFJ_AXIS20,
	QFJ_AXIS21,
	QFJ_AXIS22,
	QFJ_AXIS23,
	QFJ_AXIS24,
	QFJ_AXIS25,
	QFJ_AXIS26,
	QFJ_AXIS27,
	QFJ_AXIS28,
	QFJ_AXIS29,
	QFJ_AXIS30,
	QFJ_AXIS31,
	QFJ_AXIS32,

	QFK_LAST
} knum_t;

typedef enum {
	key_unfocused,			///< engine has lost input focus
	key_game,				///< Normal in-game key bindings
	key_demo,				///< Demo playback key bindings
	key_console,			///< Command console key bindings
	key_message,			///< Message input line key bindings
	key_menu,				///< Menu key bindings.

	key_last				///< enum size
} keydest_t;

#ifndef __QFCC__
typedef struct {
	int     down[2];        // key nums holding it down
	int     state;          // low bit is down state
} kbutton_t;

extern knum_t       key_togglemenu;
extern knum_t       key_toggleconsole;

typedef struct keybind_s {
	char *str;
} keybind_t;

/**	Input Mapping Table
*/
typedef struct imt_s {
	struct imt_s *next;				///< list of tables attached to key_dest
	struct imt_s *chain;			///< fallback table if key not bound
	const char *name;				///< for user interaction
	keybind_t   bindings[QFK_LAST];
	int         written;			///< avoid duplicate config file writes
} imt_t;

/**	Chain of input mapping tables ascociated with a keydest sub-system (game,
	menu, etc).
*/
typedef struct keytarget_s {
	imt_t      *imts;				///< list of tables attached to this target
	imt_t      *active;				///< currently active table in this target
} keytarget_t;

extern int		keydown[QFK_LAST];

struct cbuf_s;

void Key_Init (struct cbuf_s *cb);
void Key_Init_Cvars (void);

/**	Find an Input Mapping Table by name.

	Searches through all keydest targets for the named imt. The search is case
	insensitive.

	\param imt_name	The name of the imt to find. Case insensitive.
	\return			The named imt, or null if not found.
*/
imt_t *Key_FindIMT (const char *imt_name) __attribute__((pure));

/**	Create a new imt and attach it to the specified keydest target.

	The name of the new imt must be unique (case insensitive) across all
	keydest targets. This is to simplify the in_bind command.

	If \a chain_imt_name is not null, then it species the fallback imt for when
	the key is not bound in the new imt. It must be an already existing imt in
	the specified keydest target. This is to prevent loops and other weird
	behavior.

	\param kd		The keydest target to which the new imt will be attached.
	\param imt_name	The name for the new imt. Must be unique (case
					insensitive).
	\param chain_imt_name	The name of the fallback imt if not null. Must
					already exist on the specified keydest target.
*/
void Key_CreateIMT (keydest_t kd, const char *imt_name,
					const char *chain_imt_name);

/**	Handle a key press/release event.

	\param key		The key that was pressed or released for this event.
	\param unicode	The unicode value of the key.
	\param down		True if a press event, false if a release event.
*/
void Key_Event (knum_t key, short unicode, qboolean down);

/**	Handle loss or gain of input focus (usually in windowed enviroments).

	Sets the keydest target to key_unfocuses when input focus is lost.

	Triggers keydest callbacks.

	\bug			Always sets the target to key_game when focus is gained.

	\param gain		True if focus is gained, false if focus is lost.
*/
void Key_FocusEvent (int gain);

void Key_WriteBindings (QFile *f);

/**	Force all key states to unpressed.

	Sends a key release event for any keys that are seen as down.
*/
void Key_ClearStates (void);

/**	Return a key binding in the specified input mapping table.

	\param imt		The input mapping table from which to get the binding.
	\param key		The key for which to get the binding.
	\return			The command string bound to the key, or null if unbound.
*/
const char *Key_GetBinding (imt_t *imt, knum_t key) __attribute__((pure));

/** Bind a command string to a key in the specified input mapping table.

	Only one command string can be bound to a key, but the command string may
	contain multiple commands.

	\param imt		The input mapping table in which the key will be bound.
	\param keynum	The key to which the command string will be bound.
	\param binding	The command string that will be bound.
*/
void Key_SetBinding (imt_t *imt, knum_t keynum, const char *binding);

/**	Set the current keydest target.

	Triggers keydest callbacks.

	\param kd		The keydest target to make current.
*/
void Key_SetKeyDest(keydest_t kd);

/** keydest callback signature.

	\param kd		The new current keydest target.
*/
typedef void keydest_callback_t (keydest_t kd);

/**	Add a callback for when the keydest target changes.

	\param callback	The callback to be added.
*/
void Key_KeydestCallback (keydest_callback_t *callback);

/**	Get the string representation of a key.

	Returns a string (a QFK_* name) for the given keynum.

	\param keynum	The key for which to get the string.
	\return			The string representation of the key.
*/
const char *Key_KeynumToString (knum_t keynum) __attribute__((pure));

/**	Get the keynum for the named key.

	Returns a key number to be used to index keybindings[] by looking at
	the given string.  Single ascii characters return themselves, while
	the QFK_* names are matched up.

	\param str		The name of the key.
	\return			The named key if valid, otherwise -1
*/
int Key_StringToKeynum (const char *str) __attribute__((pure));

struct progs_s;

/**	Add the Key builtins to the specified progs instance.
*/
void Key_Progs_Init (struct progs_s *pr);
#endif

///@}

#endif // _KEYS_H
