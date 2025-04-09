#pragma bug die
int mul_e_c_t_c (int x) { return x * 6 * 7; }
int mul_c_e_t_c (int x) { return 6 * x * 7; }
int mul_c_c_t_e (int x) { return 6 * 7 * x; }
int mul_e_t_c_c (int x) { return x * (6 * 7); }
int mul_c_t_e_c (int x) { return 6 * (x * 7); }
int mul_c_t_c_e (int x) { return 6 * (7 * x); }

int addadd_e_c_t_c (int x) { return x + 6 + 7; }
int addadd_c_e_t_c (int x) { return 6 + x + 7; }
int addadd_c_c_t_e (int x) { return 6 + 7 + x; }
int addadd_e_t_c_c (int x) { return x + (6 + 7); }
int addadd_c_t_e_c (int x) { return 6 + (x + 7); }
int addadd_c_t_c_e (int x) { return 6 + (7 + x); }

int addsub_e_c_t_c (int x) { return x + 6 - 7; }
int addsub_c_e_t_c (int x) { return 6 + x - 7; }
int addsub_c_c_t_e (int x) { return 6 + 7 - x; }
int addsub_e_t_c_c (int x) { return x + (6 - 7); }
int addsub_c_t_e_c (int x) { return 6 + (x - 7); }
int addsub_c_t_c_e (int x) { return 6 + (7 - x); }

int subadd_e_c_t_c (int x) { return x - 6 + 7; }
int subadd_c_e_t_c (int x) { return 6 - x + 7; }
int subadd_c_c_t_e (int x) { return 6 - 7 + x; }
int subadd_e_t_c_c (int x) { return x - (6 + 7); }
int subadd_c_t_e_c (int x) { return 6 - (x + 7); }
int subadd_c_t_c_e (int x) { return 6 - (7 + x); }

int subsub_e_c_t_c (int x) { return x - 6 - 7; }
int subsub_c_e_t_c (int x) { return 6 - x - 7; }
int subsub_c_c_t_e (int x) { return 6 - 7 - x; }
int subsub_e_t_c_c (int x) { return x - (6 - 7); }
int subsub_c_t_e_c (int x) { return 6 - (x - 7); }
int subsub_c_t_c_e (int x) { return 6 - (7 - x); }

int
main ()
{
	bool        fail = false;

	fail |= mul_e_c_t_c (10) != 420;
	fail |= mul_c_e_t_c (10) != 420;
	fail |= mul_c_c_t_e (10) != 420;
	fail |= mul_e_t_c_c (10) != 420;
	fail |= mul_c_t_e_c (10) != 420;
	fail |= mul_c_t_c_e (10) != 420;

	fail |= addadd_e_c_t_c (29) != 42;
	fail |= addadd_c_e_t_c (29) != 42;
	fail |= addadd_c_c_t_e (29) != 42;
	fail |= addadd_e_t_c_c (29) != 42;
	fail |= addadd_c_t_e_c (29) != 42;
	fail |= addadd_c_t_c_e (29) != 42;

	fail |= addsub_e_c_t_c (43) != 42;
	fail |= addsub_c_e_t_c (43) != 42;
	fail |= addsub_c_c_t_e (-29) != 42;
	fail |= addsub_e_t_c_c (43) != 42;
	fail |= addsub_c_t_e_c (43) != 42;
	fail |= addsub_c_t_c_e (-29) != 42;

	fail |= subadd_e_c_t_c (41) != 42;
	fail |= subadd_c_e_t_c (-29) != 42;
	fail |= subadd_c_c_t_e (43) != 42;
	fail |= subadd_e_t_c_c (55) != 42;
	fail |= subadd_c_t_e_c (-43) != 42;
	fail |= subadd_c_t_c_e (-43) != 42;

	fail |= subsub_e_c_t_c (55) != 42;
	fail |= subsub_c_e_t_c (-43) != 42;
	fail |= subsub_c_c_t_e (-43) != 42;
	fail |= subsub_e_t_c_c (41) != 42;
	fail |= subsub_c_t_e_c (-29) != 42;
	fail |= subsub_c_t_c_e (43) != 42;
	return fail;
}
