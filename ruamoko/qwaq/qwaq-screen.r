#include "qwaq-curses.h"
#include "qwaq-screen.h"

@implementation Screen
+(Screen *) screen
{
	return [[[self alloc] init] autorelease];
}

-init
{
	if (!(self = [super init])) {
		return nil;
	}
	window = stdscr;
	rect = getwrect (window);
	return self;
}

-add: obj
{
	return self;
}

-setBackground: (int) ch
{
	wbkgd (window, ch);
	wrefresh (window);
	return self;
}

@end
