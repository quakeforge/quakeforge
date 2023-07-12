/*
	qtv.h

	qtv main file

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2004/02/21

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

#ifndef __qtv_h
#define __qtv_h

/**	\defgroup qtv QuakeForge QTV Proxy
	\image html qwtv.svg
	\image latex qwtv.eps ""
	\ref qtv_overview
*/

/**	\defgroup qtv_general General Functions
	\ingroup qtv
*/
///@{

#define PORT_QTV 27501	///< Default port to listen for connecting clients.

typedef enum {
	RD_NONE,			///< No redirection. Default state.
	RD_CLIENT,			///< Output is sent to a specific client connection.
	RD_PACKET,			///< Output is sent as a connectionless packet.
} redirect_t;

extern double realtime;

extern struct cbuf_s *qtv_cbuf;
extern struct cbuf_args_s *qtv_args;
extern float sv_timeout;

struct client_s;

/**	Formatted console printing with possible redirection.

	Parameters are as per printf.

	Calling qtv_begin_redirect() before, and qtv_end_redirect() after a series
	of calls will redirect output.
*/
void qtv_printf (const char *fmt, ...) __attribute__((format(PRINTF,1,2)));

/**	Begin redirection of console printing.

	All calls to qtv_printf() between a call to this and qtv_end_redirect()
	will be redirected.

	\param rd		Redirection type.
	\param cl		Destination client of redirected output. Ignored for
					RD_PACKET redirection, required for RD_CLIENT.
*/
void qtv_begin_redirect (redirect_t rd, struct client_s *cl);

/**	End redirection of console printing.

	All calls to qtv_printf() between a call to qtv_begin_redirect() and this
	will be redirected.

	Causes the redirected output to be flushed to the destination set by
	qtv_begin_redirect ();
*/
void qtv_end_redirect (void);


void qtv_sbar_init (void);

///@}

#endif//__qtv_h
