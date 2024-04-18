#ifndef __ruamoko_cbuf_h
#define __ruamoko_cbuf_h

typedef @handle cbuf_h cbuf_t;
typedef struct cbuf_interpreter_s {
	void (*construct) (cbuf_t cbuf);
	void (*destruct) (cbuf_t cbuf);
	void (*reset) (cbuf_t cbuf);
	void (*add) (cbuf_t cbuf, string str);
	void (*insert) (cbuf_t cbuf, string str);
	void (*execute) (cbuf_t cbuf);
	string *(*complete) (cbuf_t cbuf, string str);
} cbuf_interpreter_t;

@extern cbuf_t Cbuf_New (cbuf_interpreter_t *interpreter, void *obj);
@extern void Cbuf_Delete (cbuf_t cbuf);
@extern void Cbuf_Reset (cbuf_t cbuf);
@extern void Cbuf_AddText (cbuf_t cbuf, string text);
@extern void Cbuf_InsertText (cbuf_t cbuf, string text);
@extern void Cbuf_Execute (cbuf_t cbuf);
@extern void Cbuf_Execute_Stack (cbuf_t cbuf);
@extern void Cbuf_Execute_Sets (cbuf_t cbuf);

#endif//__ruamoko_cbuf_h
