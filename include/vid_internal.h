#ifndef __vid_internal_h
#define __vid_internal_h

#include "QF/vid.h"
#include "QF/plugin/vid_render.h"

typedef struct vid_internal_s {
	int         (*surf_cache_size) (int width, int height);
	void        (*flush_caches) (void);
	void        (*init_caches) (void *cache, int size);
	void        (*do_screen_buffer) (void);
	void        (*set_palette) (const byte *palette);

	// gl stuff
	void        (*load_gl) (void);
	void        (*init_gl) (void);
	void        *(*get_proc_address) (const char *name, qboolean crit);
	void        (*end_rendering) (void);
} vid_internal_t;

extern struct cvar_s *vid_fullscreen;
extern struct cvar_s *vid_system_gamma;
extern struct cvar_s *vid_gamma;

extern unsigned short  sw32_8to16table[256];

void VID_GetWindowSize (int def_w, int def_h);

void VID_InitGamma (unsigned char *);
qboolean VID_SetGamma (double);
void VID_UpdateGamma (struct cvar_s *);

void VID_Update (vrect_t *rects);
void VID_LockBuffer (void);
void VID_UnlockBuffer (void);
void VID_InitBuffers (void);
void VID_MakeColormaps (void);


#endif//__vid_internal_h
