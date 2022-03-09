#ifndef __vid_sw_h
#define __vid_sw_h

struct vrect_s;
typedef struct sw_ctx_s {
	int         pixbytes;
	void      (*choose_visual) (struct sw_ctx_s *ctx);
	void      (*create_context) (struct sw_ctx_s *ctx);
	void      (*set_palette) (struct sw_ctx_s *ctx, const byte *palette);
	void      (*update) (struct sw_ctx_s *ctx, struct vrect_s *rects);
} sw_ctx_t;

extern sw_ctx_t *sw_ctx;

struct tex_s *sw_SCR_CaptureBGR (void);

#endif//__vid_sw_h
