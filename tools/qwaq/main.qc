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
