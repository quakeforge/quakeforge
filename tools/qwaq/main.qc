float foo = 1;
float bar = 1;
float snafu = 2;
float negative = -------2;

void () eek;

float () main =
{
	local float handle;
	local string buffer;

	handle = open ("main.qc", 0);
	if (handle == -1) {
		print (strerror (errno ()) + "\n");
		return 1;
	}
	do {
		buffer = read (handle, 1024);
		if (read_result == -1) {
			print (strerror (errno ()) + "\n");
			return 1;
		}
		print (buffer);
	} while (read_result == 1024);
	close (handle);
	eek ();
	return 0;
};

float () baz =
{
	return foo + bar;
};

void (float x) test =
{
	local string str;
	local float urk = 0;

	if (urk) {
		urk = foo || snafu;
	} else {
		print ("owie");
	}
};

void () blarg =
{
	local float foo = -1;
	local float bar;
	do {
		foo = 1;
	} while (foo);
	if (bar = foo);
	foo = 2;
};

void () eek =
{
	traceon();
	self.origin = '0 0 0';
	self.origin = self.origin + '1 2 3';
	traceoff();
};

.string classname;
.string netname;
float (entity tester) IsBuilding =
{
	if (
		tester.classname == "building_sentrygun" ||
		tester.classname == "building_sentrygun_base" ||
		tester.classname == "building_tesla" ||
		tester.classname == "building_dispenser" ||
		tester.classname == "building_camera" ||
		tester.classname == "building_arawana" ||
		tester.classname == "building_teleporter"||
		tester.classname == "building_fieldgen"||
		tester.classname == "building_sensor"
	)
		return 1;
	return 0;
};
float(entity thing) IsMonster =
{
	if (thing.classname=="monster_shambler")
		return 1;
	if (thing.classname=="monster_demon1")
		return 1;
	if (thing.classname=="monster_wizard")
		return 1;
	if (thing.classname=="monster_army")
		return 1;

	if (thing.classname=="monster_knight")
		return 1;
	if (thing.classname=="monster_hknight")
		return 1;

	return 0;
};
string(entity thething) GetEnemyName =
{
	if (thething.classname == "player")
		return thething.netname;

	if (IsMonster(thething))
		return "";

	if (IsBuilding(thething))
		return "";

	if (thething.classname == "grenade" && thething.netname == "land_mine")
		return "land mine";

	if (thething.classname != "")
		return thething.classname;

	return "unknown stuff";
};
void () assign =
{
	local float foo;
	local float bar;

	foo = bar;
	foo = bar = 1;
	foo = bar = bar + 1;
};
