id func (id self, SEL bar, ...)
{
	return (id)1;
}

IMP get_func ()
{
	return func;
}

id foo (IMP msg)
{
	if (!(msg = get_func ())) {
		return nil;
	}
	return msg(nil, nil);
}

int
main()
{
	return foo (nil) == nil;
}
