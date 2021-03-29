/*
	va.h

	Definitions common to client and server.

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000  Marcus Sundberg <mackan@stacken.kth.se>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __QF_va_h
#define __QF_va_h

/** \addtogroup misc
	Formatted printing.
*/
///@{

/** Opaque context for va so it can have per-thread data.
 */
typedef struct va_ctx_s va_ctx_t;

/** Create a va context with the specified number of buffers.
 *
 * Having multiple buffers allows va to be used in short chains.
 *
 * \param buffers	The number of buffers to create in the context. va() will
 *					cycle through the buffers for each call.
 * \return			Pointer to the context. Pass to va() for thread-safe usage.
 */
va_ctx_t *va_create_context (int buffers);

/** Destroy a va context.
 *
 * \param ctx		The context to be destroyed. Must have been created by
 *					va_create_context().
 * \note			Any pointers to strings returned by va() using the context
 *					referenced by \a ctx will become invalid as the backing
 *					memory for the strings will have been freed.
 */
void va_destroy_context (va_ctx_t *ctx);

/** Does a varargs printf into a private buffer.
 *
 * \param ctx		Context used for storing the private buffer such that va
 *                  can be used in a multi-threaded environment. If null then
 *					a static context is used, in which case va is NOT
 *					thread-safe.
 * \param format	Standard printf() format string.
 * \return			Pointer to the beginning of the output string. The memory
 *					for the returned string is owned by the context pointed to
 *					by \a ctx.
 * \note			The static context is created with 4 buffers, so va (0,...)
 *					can be used to produce mildly complex output (eg, 3 calls
 *					to va sent to a 4th) with a reduced risk of strings being
 *					trampled.
 */
const char *va(va_ctx_t *ctx, const char *format, ...) __attribute__((format(PRINTF,2,3)));

/** Does a varargs printf into a malloced buffer.
 *
 * Combines the effect of strdup and sprintf, but in a safe manner. Essentially
 * the equivalent of strdup (va (ctx, format, ...));
 *
 * \param format	Standard printf() format string.
 * \return			Pointer to the beginning of the output string. The caller
 *					is responsible for freeing the memory holding the string.
 * \note			As nva() creates a new buffer every time it is called, it
 *					is thread-safe and there is no risk of the string being
 *					trampled. In addition, it does not use va(), so combining
 *					nva() with va() is safe.
 */
char	*nva(const char *format, ...) __attribute__((format(PRINTF,1,2)));

///@}

#endif//__QF_va_h
