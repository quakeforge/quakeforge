@interface Foo : Object
-run;
@end


@implementation Foo

-run
{
	print ("Hello world\n");
	printf ("%i\n", self);
}

@end
