#ifndef __ruamoko_qfile_h
#define __ruamoko_qfile_h

struct _qfile_t {};
typedef struct _qfile_t [] QFile;

@extern integer (string old, string new) Qrename;
@extern integer (string path) Qremove;
@extern QFile (string path, string mode) Qopen;
@extern void (QFile file) Qclose;
@extern string (QFile file) Qgetline;
@extern string (QFile file, integer len) Qreadstring;
@extern integer (QFile file, void [] buf, integer count) Qread;
@extern integer (QFile file, void [] buf, integer count) Qwrite;
@extern integer (QFile file, string str) Qputs;
//@extern integer (QFile file, void [] buf, integer count) Qgets;
@extern integer (QFile file) Qgetc;
@extern integer (QFile file, integer c) Qputc;
@extern integer (QFile file, integer offset, integer whence) Qseek;
@extern integer (QFile file) Qtell;
@extern integer (QFile file) Qflush;
@extern integer (QFile file) Qeof;
@extern integer (QFile file) Qfilesize;

#endif//__ruamoko_qfile_h
