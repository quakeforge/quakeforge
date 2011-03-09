.float () th_takeitem;

void (entity e1, entity e2)
_override_swap =
{
	local float () t_f_v;
	t_f_v = e1.th_takeitem;
	e1.th_takeitem = e2.th_takeitem;
	e2.th_takeitem = t_f_v;
}
