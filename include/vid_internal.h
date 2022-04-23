#ifndef __vid_internal_h
#define __vid_internal_h

#include "QF/vid.h"
#include "QF/plugin/vid_render.h"

typedef struct vid_system_s {
	void      (*init) (byte *palette, byte *colormap);
	void      (*init_cvars) (void);
	void      (*update_fullscreen) (int fullscreen);
} vid_system_t;

extern vid_system_t vid_system;

typedef struct vid_internal_s {
	void      (*flush_caches) (void *data);
	void      (*init_buffers) (void *data);
	void      (*set_palette) (void *data, const byte *palette);
	void      (*set_colormap) (void *data, const byte *colormap);

	void      (*choose_visual) (void *data);
	void      (*create_context) (void *data);

	void       *data;

	struct gl_ctx_s *(*gl_context) (void);
	struct sw_ctx_s *(*sw_context) (void);
	struct vulkan_ctx_s *(*vulkan_context) (void);
} vid_internal_t;

extern int vid_fullscreen;
extern int vid_system_gamma;
extern float vid_gamma;

void VID_GetWindowSize (int def_w, int def_h);

void VID_InitGamma (const byte *);
qboolean VID_SetGamma (double);
void VID_UpdateGamma (void);

void VID_MakeColormaps (void);

#endif//__vid_internal_h
