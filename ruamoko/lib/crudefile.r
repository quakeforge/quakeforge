#include "crudefile.h"

float (string path, string mode) cfopen = #103;
void (float desc) cfclose = #104;
string (float desc) cfread = #105;
float (float desc, string buf) cfwrite = #106;
float (float desc) cfeof = #107;
float () cfquota = #108;
