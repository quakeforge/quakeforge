#include "pakfile.h"

int
main (int argc, char **argv)
{
	pack_t     *pack;
	int         i;

	pack = pack_open (argv[1]);
	for (i = 0; i < pack->numfiles; i++)
		printf ("%6d %s\n", pack->files[i].filelen, pack->files[i].name);
	pack_close (pack);
	return 0;
}
