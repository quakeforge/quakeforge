#ifndef __vid_sw_h
#define __vid_sw_h

#include "QF/qtypes.h"

struct vrect_s;
typedef struct sw_ctx_s {
	void      (*choose_visual) (struct sw_ctx_s *ctx);
	void      (*create_context) (struct sw_ctx_s *ctx);
	void      (*set_palette) (struct sw_ctx_s *ctx, const byte *palette);
	void      (*update) (struct sw_ctx_s *ctx, struct vrect_s *rects);
	struct framebuffer_s *framebuffer;
} sw_ctx_t;

typedef struct sw_framebuffer_s {
	byte        *color;
	short       *depth;
	int          rowbytes;
} sw_framebuffer_t;

extern sw_ctx_t *sw_ctx;

#endif//__vid_sw_h
