#ifndef __vid_internal_h
#define __vid_internal_h

#include "QF/vid.h"
#include "QF/plugin/vid_render.h"

typedef struct vid_internal_s {
	int       (*surf_cache_size) (void *data, int width, int height);
	void      (*flush_caches) (void *data);
	void      (*init_caches) (void *data, void *cache, int size);
	void      (*init_buffers) (void *data);
	void      (*set_palette) (void *data, const byte *palette);

	void      (*choose_visual) (void *data);
	void      (*create_context) (void *data);

	void       *data;

	struct gl_ctx_s *(*gl_context) (void);
	struct sw_ctx_s *(*sw_context) (void);
	struct vulkan_ctx_s *(*vulkan_context) (void);
} vid_internal_t;

extern struct cvar_s *vid_fullscreen;
extern struct cvar_s *vid_system_gamma;
extern struct cvar_s *vid_gamma;

void VID_GetWindowSize (int def_w, int def_h);

extern unsigned short  d_8to16table[256];

void VID_InitGamma (const byte *);
qboolean VID_SetGamma (double);
void VID_UpdateGamma (struct cvar_s *);

void VID_InitBuffers (void);
void VID_MakeColormaps (void);

#endif//__vid_internal_h
