integer [5] y;
void () foo =
{
	local integer x;

	for (x = 0; x < 5; x++)
		y[x] = 0;
	for (; x < 5; x++)
		y[x] = 0;
	for (x = 0;; x++)
		y[x] = 0;
	for (;; x++)
		y[x] = 0;
	for (x = 0; x < 5;)
		y[x] = 0;
	for (; x < 5;)
		y[x] = 0;
	for (x = 0;;)
		y[x] = 0;
	for (;;)
		y[x] = 0;
}
