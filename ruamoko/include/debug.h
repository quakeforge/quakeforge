#ifndef __debug_h
#define __debug_h

//FIXME@extern void () break;
@extern void (string e) error;
@extern void (string e) objerror;
@extern void (string s) dprint;
@extern void () coredump;
@extern void () traceon;
@extern void () traceoff;
@extern void (entity e) eprint;

#endif//__debug_h
