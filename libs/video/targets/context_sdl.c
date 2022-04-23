#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>
#include <SDL.h>

#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "context_sdl.h"
#include "vid_internal.h"

void
VID_SDL_GammaCheck (void)
{
	Uint16 redtable[256], greentable[256], bluetable[256];

	if (SDL_GetGammaRamp(redtable, greentable, bluetable) < 0)
		vid_gamma_avail = false;
	else
		vid_gamma_avail = true;
}

void
VID_SetCaption (const char *text)
{
	if (text && *text) {
		char		*temp = strdup (text);

		SDL_WM_SetCaption (va (0, "%s: %s", PACKAGE_STRING, temp), NULL);
		free (temp);
	} else {
		SDL_WM_SetCaption (va (0, "%s", PACKAGE_STRING), NULL);
	}
}

qboolean
VID_SetGamma (double gamma)
{
	return SDL_SetGamma((float) gamma, (float) gamma, (float) gamma);
}

static void
VID_UpdateFullscreen (void *data, const cvar_t *cvar)
{
	if (!r_data || !viddef.initialized)
		return;
	if ((cvar && !(sdl_screen->flags & SDL_FULLSCREEN))
		|| (!cvar && sdl_screen->flags & SDL_FULLSCREEN))
		if (!SDL_WM_ToggleFullScreen (sdl_screen))
			Sys_Printf ("VID_UpdateFullscreen: error setting fullscreen\n");
	IN_UpdateGrab (in_grab);
}

void
SDL_Init_Cvars (void)
{
	Cvar_Register (&vid_fullscreen_cvar, VID_UpdateFullscreen, 0);
	Cvar_Register (&vid_system_gamma_cvar, 0, 0);
}
