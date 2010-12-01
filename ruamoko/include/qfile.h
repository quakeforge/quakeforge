#ifndef __ruamoko_qfile_h
#define __ruamoko_qfile_h

struct _qfile_t {};
typedef struct _qfile_t [] QFile;

@extern integer Qrename (string old, string new);
@extern integer Qremove (string path);
@extern QFile Qopen (string path, string mode);
@extern void Qclose (QFile file);
@extern string Qgetline (QFile file);
@extern string Qreadstring (QFile file, integer len);
@extern integer Qread (QFile file, void [] buf, integer count);
@extern integer Qwrite (QFile file, void [] buf, integer count);
@extern integer Qputs (QFile file, string str);
//@extern integer Qgets (QFile file, void [] buf, integer count);
@extern integer Qgetc (QFile file);
@extern integer Qputc (QFile file, integer c);
@extern integer Qseek (QFile file, integer offset, integer whence);
@extern integer Qtell (QFile file);
@extern integer Qflush (QFile file);
@extern integer Qeof (QFile file);
@extern integer Qfilesize (QFile file);

#endif//__ruamoko_qfile_h
