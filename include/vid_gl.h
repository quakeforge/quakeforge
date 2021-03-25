#ifndef __vid_gl_h
#define __vid_gl_h

// GLXContext is a pointer to opaque data
typedef struct __GLXcontextRec *GLXContext;

typedef struct gl_ctx_s {
	GLXContext  context;
	void        (*load_gl) (void);
	void        (*choose_visual) (struct gl_ctx_s *ctx);
	void        (*create_context) (struct gl_ctx_s *ctx);
	void        (*init_gl) (void);
	void        *(*get_proc_address) (const char *name, qboolean crit);
	void        (*end_rendering) (void);
} gl_ctx_t;

extern gl_ctx_t *gl_ctx;
extern gl_ctx_t *glsl_ctx;

#endif//__vid_gl_h
