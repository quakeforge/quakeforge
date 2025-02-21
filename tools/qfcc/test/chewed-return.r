#pragma bug die
#pragma traditional

entity spawn (void) {return @nil;}
float time;
.void () think;
.float nextthink;
void rthink(){}
void foo (void)
{
	entity rspawn;

	rspawn = spawn();
	rspawn.nextthink = time + 0.1;
	rspawn.think = rthink;
};
float main ()
{
	return 0;		// compile test
}
