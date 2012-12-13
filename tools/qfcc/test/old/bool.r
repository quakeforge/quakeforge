integer (integer x, integer y, integer z) foo =
{
	if (x || !y && z) {
		return x || y && z;
	} else {
		for (x = 0; (x || y) && z < 3; x++)
			return (x || y && z) ? 4 : 9;
	}
	return !x;
};

float bar (void *a)
{
	void *b;
	if (!(b = a))
		return 1;
	return 2;
}
