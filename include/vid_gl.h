#ifndef __vid_gl_h
#define __vid_gl_h

// GL_context is a pointer to opaque data
typedef struct GL_context *GL_context;

typedef struct gl_ctx_s {
	GL_context  context;
	void        (*load_gl) (void);
	void        (*choose_visual) (struct gl_ctx_s *ctx);
	void        (*create_context) (struct gl_ctx_s *ctx);
	void        (*init_gl) (void);
	void        *(*get_proc_address) (const char *name, qboolean crit);
	void        (*end_rendering) (void);

	int         begun;
	double      start_time;
	int         brush_polys;
	int         alias_polys;
} gl_ctx_t;

extern gl_ctx_t *gl_ctx;
extern gl_ctx_t *glsl_ctx;

struct tex_s *gl_SCR_CaptureBGR (void);
struct tex_s *glsl_SCR_CaptureBGR (void);

void gl_Fog_SetupFrame (void);
void gl_Fog_EnableGFog (void);
void gl_Fog_DisableGFog (void);
void gl_Fog_StartAdditive (void);
void gl_Fog_StopAdditive (void);

#endif//__vid_gl_h
