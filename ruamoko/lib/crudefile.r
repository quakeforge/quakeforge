#include <crudefile.h>

float (string path, string mode) cfopen = #0x000f0000 + 103;
void (float desc) cfclose = #0x000f0000 + 104;
string (float desc) cfread = #0x000f0000 + 105;
float (float desc, string buf) cfwrite = #0x000f0000 + 106;
float (float desc) cfeof = #0x000f0000 + 107;
float () cfquota = #0x000f0000 + 108;
