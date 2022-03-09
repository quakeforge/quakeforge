#ifndef __vid_sw_h
#define __vid_sw_h

#include "QF/qtypes.h"

struct surf_s;
struct vrect_s;
struct particle_s;
struct spanpackage_s;
struct qpic_s;
struct espan_s;
struct sspan_s;

typedef struct sw_draw_s {
#define SW_DRAW_FUNC(name, rettype, params) \
	rettype (*name) params;
#include "vid_sw_draw.h"
} sw_draw_t;

struct vrect_s;
typedef struct sw_ctx_s {
	int         pixbytes;
	void      (*choose_visual) (struct sw_ctx_s *ctx);
	void      (*create_context) (struct sw_ctx_s *ctx);
	void      (*set_palette) (struct sw_ctx_s *ctx, const byte *palette);
	void      (*update) (struct sw_ctx_s *ctx, struct vrect_s *rects);
	sw_draw_t  *draw;
} sw_ctx_t;

extern sw_ctx_t *sw_ctx;

#define SW_DRAW_FUNC(name, rettype, params) \
	rettype name##_8 params;
#include "vid_sw_draw.h"

#define SW_DRAW_FUNC(name, rettype, params) \
	rettype name##_16 params;
#include "vid_sw_draw.h"

#define SW_DRAW_FUNC(name, rettype, params) \
	rettype name##_32 params;
#include "vid_sw_draw.h"

struct tex_s *sw_SCR_CaptureBGR (void);

#endif//__vid_sw_h
