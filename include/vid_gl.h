#ifndef __vid_gl_h
#define __vid_gl_h

#include "QF/simd/types.h"

// GL_context is a pointer to opaque data
typedef struct GL_context *GL_context;

typedef struct gl_ctx_s {
	GL_context  context;
	void        (*load_gl) (struct gl_ctx_s *ctx);
	void        (*choose_visual) (struct gl_ctx_s *ctx);
	void        (*create_context) (struct gl_ctx_s *ctx, int core);
	void        (*init_gl) (void);
	void        *(*get_proc_address) (const char *name, qboolean crit);
	void        (*end_rendering) (void);

	mat4f_t     projection;

	int         begun;
	double      start_time;
	int         brush_polys;
	int         alias_polys;
} gl_ctx_t;

typedef struct gl_framebuffer_s {
	unsigned    handle;
	unsigned    color;
	unsigned    depth;
} gl_framebuffer_t;

extern gl_ctx_t *gl_ctx;
extern gl_ctx_t *glsl_ctx;

void gl_Fog_SetupFrame (void);
void gl_Fog_EnableGFog (void);
void gl_Fog_DisableGFog (void);
void gl_Fog_StartAdditive (void);
void gl_Fog_StopAdditive (void);

void gl_errors (const char *msg);

#endif//__vid_gl_h
