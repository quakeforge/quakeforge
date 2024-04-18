// do not include this directly: it's meant for -include
#ifdef HAVE_TRACY
// tracy includes math.h but _GNU_SOURCE is needed for sincosf
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// enable tracy
#define TRACY_ENABLE
#define TRACY_VK_USE_SYMBOL_TABLE
#include "tracy/TracyC.h"

static inline void __qfZoneEnd (TracyCZoneCtx **ctxptr)
{
	TracyCZoneEnd (**ctxptr);
}

#define qftVkCtx_t struct ___tracy_vkctx

#define qfConcatInternal(a,b) a##b
#define qfConcat(a,b) qfConcatInternal(a, b)

#define qfFrameMark TracyCFrameMark
#define qfZoneNamed(varname, active) \
	TracyCZone (varname, active) \
	__attribute__((cleanup(__qfZoneEnd))) \
	TracyCZoneCtx *qfConcat(__qfScoped##varname, __COUNTER__) = &varname
#define qfZoneScoped(active) \
	qfZoneNamed (__qfZone, active)

#define qfZoneEnd(varname) TracyCZoneEnd (varname)

#define qfZoneName(ctx, name, size) TracyCZoneName (ctx, name, size)
#define qfZoneColor(ctx, color) TracyCZoneColor (ctx, color)
#define qfZoneValue(ctx, value) TracyCZoneValue (ctx, value)

#define qfZoneNamedN(varname, name, active) \
	TracyCZoneN (varname, name, active) \
	__attribute__((cleanup(__qfZoneEnd))) \
	TracyCZoneCtx *qfConcat(__qfZone, __COUNTER__) = &varname

#define qfMessageL(msg) TracyCMessageL(msg)

#ifndef __cplusplus
#define aligned_alloc(a, s) qf_aligned_alloc(a, s)
#define malloc(s) qf_malloc(s)
#define calloc(c,s) qf_calloc(c,s)
#define realloc(p,s) qf_realloc(p,s)
#define free(p) qf_free(p)
#define strdup(s) qf_strdup(s)
#include <stdlib.h>
#undef aligned_alloc
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#define getcwd(b,s) qf_getcwd(b,s)
#include <unistd.h>
#undef getcwd

void *aligned_alloc (size_t a, size_t s) asm("__qf_aligned_alloc");
void *malloc (size_t s) asm("__qf_malloc");
void *calloc (size_t c, size_t s) asm("__qf_calloc");
void *realloc (void *p, size_t s) asm("__qf_realloc");
void free (void *p) asm("__qf_free");
char *strdup (const char *s) asm("__qf_strdup");
char *getcwd (char *b, size_t s) asm("__qf_getcwd");
#endif

#else

#define qfFrameMark
#define qfZoneNamed(varname, active)
#define qfZoneScoped(active)
#define qfZoneEnd(varname)
#define qfZoneName(ctx, name, size)
#define qfZoneColor(ctx, color)
#define qfZoneValue(ctx, value)
#define qfZoneNamedN(varname, name, active)
#define qfMessageL(msg)

#define qftVkCtx_t void

#endif
