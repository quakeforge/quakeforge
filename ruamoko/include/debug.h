#ifndef __ruamoko_debug_h
#define __ruamoko_debug_h

//FIXME@extern void () break;
@extern void (string e) error;
@extern void (string e) objerror;
@extern void (string s) dprint;
@extern void () coredump;
@extern void () traceon;
@extern void () traceoff;
@extern void (entity e) eprint;

#endif//__ruamoko_debug_h
