float (float a, float b) idiv =
{
	return ((a & -1) / (b & -1)) & -1;
};
float (float a, float b) imod =
{
	return (a & -1) - (b & -1) * (((a & -1) / (b & -1)) & -1);
};
