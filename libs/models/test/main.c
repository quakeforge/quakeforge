#define num_tests (sizeof (tests) / sizeof (tests[0]))

static int test_enabled[num_tests] = { 0 };

int
main (int argc, char **argv)
{
//	vec3_t      start, end;
	int         c;
	size_t      i, test;
	int         pass = 1;

	while ((c = getopt (argc, argv, "qvt:")) != EOF) {
		switch (c) {
			case 'q':
				verbose--;
				break;
			case 'v':
				verbose++;
				break;
			case 't':
				test = atoi (optarg);
				if (test < num_tests) {
					test_enabled[test] = 1;
				} else {
					fprintf (stderr, "Bad test number (0 - %zd)\n", num_tests);
					return 1;
				}
				break;
			default:
				fprintf (stderr, "-q (quiet) -v (verbose) and/or -t TEST "
								 "(test number)\n");
				return 1;
		}
	}

	for (i = 0; i < num_tests; i++)
		if (test_enabled[i])
			break;
	if (i == num_tests) {
		for (i = 0; i < num_tests; i++)
			test_enabled[i] = 1;
	}

	if (verbose > 0)
		printf ("start -> end => stop frac allsolid startsolid inopen "
				"inwater\n");
	for (i = 0; i < num_tests; i++) {
		if (!test_enabled[i])
			continue;
		pass &= run_test (&tests[i]);
	}

	return !pass;
}
