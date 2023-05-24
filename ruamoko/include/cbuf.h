#ifndef __ruamoko_cbuf_h
#define __ruamoko_cbuf_h

typedef struct { int x; } cbuf_t;

@extern void Cbuf_AddText (cbuf_t *cbuf, string text);
@extern void Cbuf_InsertText (cbuf_t *cbuf, string text);
@extern void Cbuf_Execute (cbuf_t *cbuf);
@extern void Cbuf_Execute_Sets (cbuf_t *cbuf);

#endif//__ruamoko_cbuf_h
