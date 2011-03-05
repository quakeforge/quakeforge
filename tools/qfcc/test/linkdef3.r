@extern integer foo (void);
@extern integer bar (void);

void __obj_exec_class (obj_module_t *msg) = #0;
id (id receiver, SEL op, ...) obj_msgSend = #0;

integer baz (void)
{
	return foo () + bar ();
}
