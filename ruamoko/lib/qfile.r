#include "qfile.h"

integer (string old, string new) Qrename = #0;
integer (string path) Qremove = #0;
QFile (string path, string mode) Qopen = #0;
void (QFile file) Qclose = #0;
string (QFile file) Qgetline = #0;
string (QFile file, integer length) Qreadstring = #0;
integer (QFile file, void *buf, integer count) Qread = #0;
integer (QFile file, void *buf, integer count) Qwrite = #0;
integer (QFile file, string str) Qputs = #0;
//integer (QFile file, void *buf, integer count) Qgets = #0;
integer (QFile file) Qgetc = #0;
integer (QFile file, integer c) Qputc = #0;
integer (QFile file, integer offset, integer whence) Qseek = #0;
integer (QFile file) Qtell = #0;
integer (QFile file) Qflush = #0;
integer (QFile file) Qeof = #0;
integer (QFile file) Qfilesize = #0;
