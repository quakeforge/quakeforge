#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include "cvar.h"
#include "console.h"
#include "qargs.h"
#include "cmd.h"
#include "zone.h"
#include "quakefs.h"
#include "gib.h"
#include "gib_instructions.h"
#include "gib_interpret.h"
#include "gib_modules.h"
#include "gib_parse.h"
#include "gib_vars.h"
#include "gib_error.h"

int
GIB_Get_Inst (char *start)
{
	int         i;
	int         len = 0;

	for (i = 0; start[i] != ';'; i++) {
		if (start[i] == '\'') {
			if ((len = GIB_End_Quote (start + i)) < 0)
				return len;
			else
				i += len;
		}
		if (start[i] == '\"') {
			if ((len = GIB_End_DQuote (start + i)) < 0)
				return len;
			else
				i += len;
		}
		if (start[i] == '{') {
			if ((len = GIB_End_Bracket (start + i)) < 0)
				return len;
			else
				i += len;
		}
		if (start[i] == 0)
			return 0;
	}
	return i;
}
int
GIB_Get_Arg (char *start)
{
	int         i;
	int         ret = -2;

	if (*start == '\'') {
		ret = GIB_End_Quote (start) + 1;
	}
	if (*start == '\"') {
		ret = GIB_End_DQuote (start) + 1;
	}
	if (*start == '{') {
		ret = GIB_End_Bracket (start) + 1;
	}

	if (ret == -1)
		return -1;
	if (ret >= 0)
		return ret;

	for (i = 1;
		 (start[i] != ' ' && start[i] != 0 && start[i] != '\''
		  && start[i] != '\"' && start[i] != '{') || start[i - 1] == '\\'; i++);
	return i;
}
int
GIB_End_Quote (char *start)
{
	int         i;
	int         len = 0;

	for (i = 1; start[i] != '\''; i++) {
		if (start[i - 1] != '\\') {
			if (start[i] == '\"') {
				if ((len = GIB_End_DQuote (start + i)) < 0)
					return len;
				else
					i += len;
			}

			if (start[i] == '{') {
				if ((len = GIB_End_Bracket (start + i)) < 0)
					return len;
				else
					i += len;
			}
			if (start[i] == 0)
				return -1;
		}
	}
	return i;
}
int
GIB_End_DQuote (char *start)
{
	int         i, ret;

	for (i = 1; start[i] != '\"'; i++) {
		if (start[i - 1] != '\\') {
			if (start[i] == '\'') {
				if ((ret = GIB_End_Quote (start + i)) < 0)
					return ret;
				else
					i += ret;
			}

			if (start[i] == '{') {
				if ((ret = GIB_End_Bracket (start + i)) < 0)
					return ret;
				else
					i += ret;
			}
			if (start[i] == 0)
				return -1;
		}
	}
	return i;
}

int
GIB_End_Bracket (char *start)
{
	int         i, ret;

	for (i = 1; start[i] != '}'; i++) {
		if (start[i - 1] != '\\') {
			if (start[i] == '\'') {
				if ((ret = GIB_End_Quote (start + i)) < 0)
					return ret;
				else
					i += ret;
			}

			if (start[i] == '\"') {
				if ((ret = GIB_End_DQuote (start + i)) < 0)
					return ret;
				else
					i += ret;
			}

			if (start[i] == '{') {
				if ((ret = GIB_End_Bracket (start + i)) < 0)
					return ret;
				else
					i += ret;
			}

			if (start[i] == 0)
				return -1;
		}
	}
	return i;
}

gib_sub_t  *
GIB_Get_ModSub_Sub (char *modsub)
{
	gib_module_t *mod;
	gib_sub_t  *sub;
	char       *divider;

	if (!(divider = strstr (modsub, "::")))
		return 0;
	*divider = 0;
	mod = GIB_Find_Module (modsub);
	*divider = ':';
	if (!mod)
		return 0;
	sub = GIB_Find_Sub (mod, divider + 2);
	return sub;
}
gib_module_t *
GIB_Get_ModSub_Mod (char *modsub)
{
	gib_module_t *mod;
	char       *divider;

	if (!(divider = strstr (modsub, "::")))
		return 0;
	*divider = 0;
	mod = GIB_Find_Module (modsub);
	*divider = ':';
	return mod;
}

int
GIB_ExpandEscapes (char *source)
{
	int         i, m;

	for (i = 0, m = 0; i <= strlen (source); i++) {
		if (source[i] == '\\') {
			switch (source[++i]) {
				case 0:
				return GIB_E_PARSE;
				break;
				case 'n':
				case 'N':
				source[m] = '\n';
				break;
				default:
				source[m] = source[i];
			}
		} else
			source[m] = source[i];
		m++;
	}
	return 0;
}
