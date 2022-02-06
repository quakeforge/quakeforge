string got;

SEL (string name) sel_get_uid = #0;
@param (id receiver, SEL op, @va_list args) obj_msg_sendv = #0;
id (Class class) class_create_instance = #0;
int obj_increment_retaincount (id object) = #0;

void
send (id obj, string cmd, string str)
{
	@static @param params[3];
	@va_list va_list = {3, params};
	SEL sel = sel_get_uid (cmd);

	params[0].pointer_val = obj;
	params[1].pointer_val = sel;
	params[2].string_val = str;
	@return obj_msg_sendv (obj, sel, va_list);
}

@interface Object   //just so the runtime doesn't complain
{
    Class isa;
}
+alloc;
-init;
-catch:(string) it;
@end

int
main ()
{
	id obj = [[Object alloc] init];
	send (obj, "catch:", "it");
	return !(got == "it");
}

@implementation Object
+alloc
{
	return class_create_instance (self);
}

-init
{
	obj_increment_retaincount (self);
	return self;
}

-catch:(string) it
{
	got = it;
	return self;
}
@end

@attribute(no_va_list) id (id receiver, SEL op, ...) obj_msgSend = #0;
void __obj_exec_class (struct obj_module *msg) = #0;
