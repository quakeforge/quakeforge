@interface Foo : Object
-run;
@end


@implementation Foo

+ (void) initialize
{
	print ("+initialize\n");
}

-run
{
	print ("Hello world\n");
	printf ("%i\n", self);
}

@end
