#pragma bug die
#pragma warn error

@interface foo
{
	id isa;
}
@end

void bar ()
{
	foo foos[1];
}
