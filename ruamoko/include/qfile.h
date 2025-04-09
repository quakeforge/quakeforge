#ifndef __ruamoko_qfile_h
#define __ruamoko_qfile_h

typedef @handle _qfile_t QFile;

@extern int Qrename (string old, string new);
@extern int Qremove (string path);
@extern QFile Qopen (string path, string mode);
@extern void Qclose (QFile file);
@extern string Qgetline (QFile file);
@extern string Qreadstring (QFile file, int len);
@extern int Qread (QFile file, void *buf, int count);
@extern int Qwrite (QFile file, void *buf, int count);
@extern int Qputs (QFile file, string str);
//@extern int Qgets (QFile file, void *buf, int count);
@extern int Qgetc (QFile file);
@extern int Qputc (QFile file, int c);
@extern int Qseek (QFile file, int offset, int whence);
@extern int Qtell (QFile file);
@extern int Qflush (QFile file);
@extern int Qeof (QFile file);
@extern int Qfilesize (QFile file);

#endif//__ruamoko_qfile_h
