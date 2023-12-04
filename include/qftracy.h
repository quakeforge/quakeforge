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
