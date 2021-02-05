#include <qfile.h>

int Qrename (string old, string new) = #0;
int Qremove (string path) = #0;
QFile Qopen (string path, string mode) = #0;
void Qclose (QFile file) = #0;
string Qgetline (QFile file) = #0;
string Qreadstring (QFile file, int length) = #0;
int Qread (QFile file, void *buf, int count) = #0;
int Qwrite (QFile file, void *buf, int count) = #0;
int Qputs (QFile file, string str) = #0;
//int Qgets (QFile file, void *buf, int count) = #0;
int Qgetc (QFile file) = #0;
int Qputc (QFile file, int c) = #0;
int Qseek (QFile file, int offset, int whence) = #0;
int Qtell (QFile file) = #0;
int Qflush (QFile file) = #0;
int Qeof (QFile file) = #0;
int Qfilesize (QFile file) = #0;
