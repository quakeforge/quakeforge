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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/idparse.h"
#include "QF/sys.h"

typedef struct idbuf_s {
	dstring_t *buf, *line;
} idbuf_t;

#define DATA(x) ((idbuf_t *)(x)->data)

static void
COM_construct (cbuf_t *cbuf)
{
	idbuf_t *new = calloc (1, sizeof (idbuf_t));

	new->buf = dstring_newstr();
	new->line = dstring_newstr();
	cbuf->data = new;
}

static void
COM_destruct (cbuf_t *cbuf)
{
	dstring_delete(DATA(cbuf)->buf);
	dstring_delete(DATA(cbuf)->line);
	free(cbuf->data);
}

static void
COM_reset (cbuf_t *cbuf)
{
	dstring_clearstr (DATA(cbuf)->buf);
	dstring_clearstr (DATA(cbuf)->line);
}

static void
COM_add (cbuf_t *cbuf, const char *str)
{
	dstring_appendstr (DATA(cbuf)->buf, str);
}

static void
COM_insert (cbuf_t *cbuf, const char *str)
{
	dstring_insertstr (DATA(cbuf)->buf, 0, "\n");
	dstring_insertstr (DATA(cbuf)->buf, 0, str);
}

static dstring_t *_com_token;
const char *com_token;

VISIBLE const char *
COM_Parse (const char *data)
{
	int         c;
	int         i;

	if (!_com_token)
		_com_token = dstring_newstr ();
	com_token = _com_token->str;

	if (!data)
		return 0;

skipwhite:
	while (isspace ((byte) *data))
		data++;
	if (!*data) {
		dstring_clearstr (_com_token);
		com_token = _com_token->str;
		return 0;
	}
	// skip // coments
	if (data[0] == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	// skip /*..*/ comments
	if (data[0] == '/' && data[1] == '*') {
		data += 2;		// skip over the leading /*
		while (data[0] && !(data[0] == '*' && data[1] == '/'))
			data++;
		if (data[0])
			data +=2;	// skip over the trailing */
		goto skipwhite;
	}
	if (*data == '"') {
		data++;
		i = 0;
		while (1) {
			c = data[i++];
			if (c == '"' || !c) {
				dstring_copysubstr (_com_token, data, i - 1);
				goto done;
			}
		}
	}
	i = 0;
	do {
		i++;
	} while (data[i] && !isspace ((byte) data[i]));
	dstring_copysubstr (_com_token, data, i);
done:
	com_token = _com_token->str;
	return data + i;
}

VISIBLE void
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

static void
COM_extract_line (cbuf_t *cbuf)
{
	int			i;
	dstring_t	*buf = DATA(cbuf)->buf;
	dstring_t	*line = DATA(cbuf)->line;
	int			len = buf->size - 1;
	char		*text = buf->str;
	int			quotes = 0;

	dstring_clearstr (line);
	for (i = 0; i < len; i++) {
		if (text[i] == '"')
			quotes++;
		if (!(quotes & 1)) {		// don't break if inside a quoted string
			if (text[i] == ';')
				break;
			if (text[i] == '/' && text[i + 1] == '/') {
				int         j = i;
				while (j < len && text[j] != '\n'
					   && (text[j] != '\r'
						   || (j < len - 1 && text[j + 1] != '\n')))
					j++;
				dstring_snip (buf, i, j - i);
				break;
			}
		}
		if (text[i] == '\n'
			|| (text[i] == '\r' && (i == len - 1 || text[ i + 1] == '\n')))
			break;
	}
	if (i)
		dstring_insert (line, 0, text, i);
	if (text[i]) {
		dstring_snip (buf, 0, i + 1);
	} else {
		// We've hit the end of the buffer, just clear it
		dstring_clearstr (buf);
	}
}

static void
COM_execute (cbuf_t *cbuf)
{
	dstring_t *buf = DATA(cbuf)->buf;
	dstring_t *line = DATA(cbuf)->line;
	while (cbuf->state == CBUF_STATE_NORMAL && *buf->str) {
		COM_extract_line (cbuf);
		COM_TokenizeString (line->str, cbuf->args);
		if (cbuf->args->argc)
			Cmd_Command (cbuf->args);
	}
}

static void
COM_execute_sets (cbuf_t *cbuf)
{
	dstring_t *buf = DATA(cbuf)->buf;
	dstring_t *line = DATA(cbuf)->line;
	while (*buf->str) {
		COM_extract_line (cbuf);
		COM_TokenizeString (line->str, cbuf->args);
		if (cbuf->args->argc &&
		   (!strcmp (cbuf->args->argv[0]->str, "set") ||
		   !strcmp (cbuf->args->argv[0]->str, "setrom")))
			Cmd_Command (cbuf->args);
	}
}

VISIBLE cbuf_interpreter_t id_interp = {
	COM_construct,
	COM_destruct,
	COM_reset,
	COM_add,
	COM_insert,
	COM_execute,
	COM_execute_sets,
	NULL
};

static void
idparse_shutdown (void *data)
{
	if (_com_token) {
		dstring_delete (_com_token);
	}
}

static void __attribute__((constructor))
idparse_init (void)
{
	Sys_RegisterShutdown (idparse_shutdown, 0);
}
