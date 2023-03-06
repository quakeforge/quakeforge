#ifndef __vid_internal_h
#define __vid_internal_h

#include "QF/vid.h"
#include "QF/plugin/vid_render.h"

typedef struct vid_system_s {
	void      (*init) (byte *palette, byte *colormap);
	void      (*shutdown) (void);
	void      (*set_palette) (byte *palette, byte *colormap);
	void      (*init_cvars) (void);
	void      (*update_fullscreen) (int fullscreen);
} vid_system_t;

extern vid_system_t vid_system;

typedef struct vid_internal_s {
	void      (*flush_caches) (void *ctx);
	void      (*init_buffers) (void *ctx);
	void      (*set_palette) (void *ctx, const byte *palette);
	void      (*set_colormap) (void *ctx, const byte *colormap);

	void      (*choose_visual) (void *ctx);
	void      (*create_context) (void *ctx);

	void       *ctx;

	struct gl_ctx_s *(*gl_context) (struct vid_internal_s *);
	struct sw_ctx_s *(*sw_context) (struct vid_internal_s *);
	struct vulkan_ctx_s *(*vulkan_context) (struct vid_internal_s *);

	void      (*unload) (void *data);
} vid_internal_t;

extern int vid_fullscreen;
extern int vid_system_gamma;
extern float vid_gamma;

void VID_GetWindowSize (int def_w, int def_h);
void VID_SetWindowSize (int width, int height);

void VID_InitGamma (const byte *);
qboolean VID_SetGamma (double);
void VID_UpdateGamma (void);

void VID_MakeColormaps (void);

#endif//__vid_internal_h
