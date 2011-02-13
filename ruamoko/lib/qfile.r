#include "qfile.h"

integer Qrename (string old, string new) = #0;
integer Qremove (string path) = #0;
QFile Qopen (string path, string mode) = #0;
void Qclose (QFile file) = #0;
string Qgetline (QFile file) = #0;
string Qreadstring (QFile file, integer length) = #0;
integer Qread (QFile file, void *buf, integer count) = #0;
integer Qwrite (QFile file, void *buf, integer count) = #0;
integer Qputs (QFile file, string str) = #0;
//integer Qgets (QFile file, void *buf, integer count) = #0;
integer Qgetc (QFile file) = #0;
integer Qputc (QFile file, integer c) = #0;
integer Qseek (QFile file, integer offset, integer whence) = #0;
integer Qtell (QFile file) = #0;
integer Qflush (QFile file) = #0;
integer Qeof (QFile file) = #0;
integer Qfilesize (QFile file) = #0;
