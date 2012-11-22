float (float x, float y, float z) foo =
{
//	return x || (y && z);
//	return (x || y) && z;
//	return x || y && z;
//	return !x & y;
	return x * y + z;
	return x + y * z;
};
