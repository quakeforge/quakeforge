@extern float (integer i) itof;

void (integer size, integer perc_val) func =
{
	local float perc;

	perc = itof (perc_val) / (100 / itof (size));
};
