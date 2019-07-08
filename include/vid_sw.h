#ifndef __vid_sw_h
#define __vid_sw_h

// GLXContext is a pointer to opaque data
typedef struct __GLXcontextRec *GLXContext;
struct vrect_s;
typedef struct sw_ctx_s {
	GLXContext  context;
	void        (*choose_visual) (struct sw_ctx_s *ctx);
	void        (*create_context) (struct sw_ctx_s *ctx);
	void        (*set_palette) (const byte *palette);
	void        (*update) (struct vrect_s *rects);
} sw_ctx_t;

extern sw_ctx_t *sw_ctx;
extern sw_ctx_t *sw32_ctx;

#endif//__vid_sw_h
