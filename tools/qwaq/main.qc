string hello = "hello";
string world = "world";
float () main =
{
	local string s;

	print (hello + " " + world + "\n");
	if (hello < world)
		print (hello);
	if (world > hello)
		print (world);
	print ("\n");
	s = hello + world;
	s = s + "\n";
	print (s);
};

float () CheckExistence =
{
	return 1;
};

void () test =
{
	if (CheckExistence() == 0) {
		return;
	}
	local entity p;
};
