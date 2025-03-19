#pragma bug die
@interface Array
-count;
-lastObject;
@end
@interface Object
@end
@static entity waypoint_thinker;
@static Array *waypoint_queue;
void foo (void)
{
	if ([waypoint_queue count]
		&& (waypoint_thinker.@this = [waypoint_queue lastObject])) {
		waypoint_thinker = nil; // just to deconfuse the dot graphs
	}
}

int main ()
{
	return 0;	// test succeeds if compile succeeds
}
@attribute(no_va_list) id obj_msgSend (id receiver, SEL op, ...) = #0;
void __obj_exec_class (struct obj_module *msg) = #0;
@implementation Object
@end
@implementation Array
-count { return self; }
-lastObject { return nil; }
@end
