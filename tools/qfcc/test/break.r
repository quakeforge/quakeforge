void () break = #0;

void () foo =
{
	break ();
	while (1) {
		break;
	}
};

float () bar =
{
	local float break;
	local float x;
	break = 0;
	break ++;
	x = break;
	break = x;
	return break;
};
