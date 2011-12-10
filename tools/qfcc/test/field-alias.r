.int blah;
.void () think;
.SEL thinkMethod = think;

void foo (entity e)
{
	*(SEL *)&e.think = @selector(foo:);
	e.thinkMethod = @selector(foo:);
}
void __obj_exec_class (obj_module_t *msg) = #0;

