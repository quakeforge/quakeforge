@interface nack
+arg;
-bla;
@end

@interface foo : nack
+bar;
+baz;
-snafu;
@end

@implementation foo

+bar
{
	[self bla];
	return [self baz];
}

+baz
{
	return [self snafu];
}

-snafu
{
	[self baz];
	return [self snafu];
}
@end
