float (float val, float inc, float max) inc = #0;
entity self;
float (.string fld) foo = { return 0; };

float (.float fld) inc_field =
{
	local float new;
	new = inc (self.fld, 2, 3);
	if (new == self.fld)
		return 0;
	self.fld = new;
	return 1;
}
