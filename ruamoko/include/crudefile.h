#ifndef __ruamoko_crudefile_h
#define __ruamoko_crudefile_h

@extern float (string path, string mode) cfopen;
@extern void (float desc) cfclose;
@extern string (float desc) cfread;
@extern float (float desc, string buf) cfwrite;
@extern float (float desc) cfeof;
@extern float () cfquota;

#endif//__ruamoko_crudefile_h
