int
foo (int bar)
{
	int x, y;

	for (x = 0; x < bar; x++) {
		for (y = 0; y < bar; y++) {
			if (x * y > bar)
				break;
			else
				continue;
		}
		break;
	}
	return x * y;
}
