#define IT_AXE              0x001000
#define IT_SHOTGUN          0x000001
#define IT_SUPER_SHOTGUN    0x000002
#define IT_NAILGUN          0x000004
#define IT_SUPER_NAILGUN    0x000008
#define IT_GRENADE_LAUNCHER 0x000010
#define IT_ROCKET_LAUNCHER  0x000020
#define IT_LIGHTNING        0x000040
#define IT_EXTRA_WEAPON     0x000080
string foo = "oo";
vector (int wep)
weapon_range =
{
	switch (wep) {
		case IT_AXE:
			return '48 0 64';
		case IT_SHOTGUN:
			return '128 0 99999';
		case IT_SUPER_SHOTGUN:
			return '128 0 99999';
		case IT_NAILGUN:
			return '180 0 3000';
		case IT_SUPER_NAILGUN:
			return '180 0 3000';
		case IT_GRENADE_LAUNCHER:
			return '180 48 3000';
		case IT_ROCKET_LAUNCHER:
			return '180 48 3000';
		case IT_LIGHTNING:
			return '350 0 512';
		default:
			return '0 0 0';
	}
};

void
duffs_device (int *to, int *from, int count)
{
	int         n = (count + 7) / 8;

	switch (count % 8) {
	case 0: do {    *to = *from++;
	case 7:         *to = *from++;
	case 6:         *to = *from++;
	case 5:         *to = *from++;
	case 4:         *to = *from++;
	case 3:         *to = *from++;
	case 2:         *to = *from++;
	case 1:         *to = *from++;
			} while(--n > 0);
	}
}

