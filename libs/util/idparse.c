/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

static const char rcsid[] =
        "$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>

#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/cmd.h"

static dstring_t *_com_token;
const char *com_token;

const char *
COM_Parse (const char *data)
{
	int         c;
	int         i;

	if (!_com_token) {
		_com_token = dstring_newstr ();
	} else {
		dstring_clearstr (_com_token);
	}
	com_token = _com_token->str;
	
	if (!data)
		return 0;

skipwhite:
	while (isspace ((byte) *data))
		data++;
	if (!*data)
		return 0;
	if (data[0] == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	if (*data == '"') {
		data++;
		i = 0;
		while (1) {
			c = data[i++];
			if (c == '"' || !c) {
				dstring_insert (_com_token, 0, data, i - 1);
				goto done;
			}
		}
	}
	i = 0;
	do {
		i++;
	} while (data[i] && !isspace ((byte) data[i]));
	dstring_insert (_com_token, 0, data, i);
done:
	com_token = _com_token->str;
	return data + i;
}

void
COM_TokenizeString (const char *str, cbuf_args_t *args)
{
	const char *s;

	args->argc = 0;

	while (1) {
		while (isspace ((byte)*str) && *str != '\n')
			str++;
		if (*str == '\n') {
			str++;
			break;
		}

		if (!*str)
			return;

		s = COM_Parse (str);
		if (!s)
			return;

		Cbuf_ArgsAdd (args, com_token);
		args->args[args->argc - 1] = str;
		str = s;
	}
	return;
}

void
COM_extract_line (cbuf_t *cbuf)
{
	int         i;
	int         len = cbuf->buf->size - 1;
	char       *text = cbuf->buf->str;
	int         quotes = 0;

	dstring_clearstr (cbuf->line);
	for (i = 0; i < len; i++) {
		if (text[i] == '"')
			quotes++;
		if (!(quotes & 1)) {
			if (text[i] == ';')
				// don't break if inside a quoted string
				break;
			if (text[i] == '/' && text[i + 1] == '/') {
				int         j = i;
				while (j < len && text[j] != '\n' && text[j] != '\r')
					j++;
				dstring_snip (cbuf->buf, i, j - i);
				len -= j - i;
			}
		}
		if (text[i] == '\n' || text[i] == '\r')
			break;
	}
	if (i)
		dstring_insert (cbuf->line, 0, text, i);
	if (text[i]) {
		dstring_snip (cbuf->buf, 0, i + 1);
	} else {
		// We've hit the end of the buffer, just clear it
		dstring_clearstr (cbuf->buf);
	}
}

void
COM_parse_line (cbuf_t *cbuf)
{
	COM_TokenizeString (cbuf->line->str, cbuf->args);
	dstring_clearstr (cbuf->line);
}

void
COM_execute_line (cbuf_t *cbuf)
{
	Cmd_Command (cbuf->args);
}

cbuf_interpreter_t id_interp = {
	COM_extract_line,
	COM_parse_line,
	COM_execute_line,
	NULL,
	NULL,
};
