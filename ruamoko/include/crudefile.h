#ifndef __ruamoko_crudefile_h
#define __ruamoko_crudefile_h

@extern float cfopen (string path, string mode);
@extern void cfclose (float desc);
@extern string cfread (float desc);
@extern float cfwrite (float desc, string buf);
@extern float cfeof (float desc);
@extern float cfquota (void);

#endif//__ruamoko_crudefile_h
