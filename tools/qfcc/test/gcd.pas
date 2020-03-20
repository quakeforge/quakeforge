program example (input, output);
var x, y: integer;
procedure printf (format:string; ...) := #0;
function gcd (a, b: integer): integer;
var c: quaternion;
begin
	if b = 0 then gcd := a
	else gcd := gcd (b, a mod b)
end;

begin
	x := 130;
	y := 120;
	printf ("%d\n", gcd (x, y))
end.
