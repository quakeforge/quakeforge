#include <AppKit/AppKit.h>

#include "QF/sys.h"

#include "QuakeEd.h"

@interface QuakeEdApp: NSApplication
- (void) sendEvent: (NSEvent *)evt;
@end

@implementation QuakeEdApp

- (void) sendEvent: (NSEvent *)evt;
{
	if ([evt type] == NSApplicationDefined)
		[quakeed_i applicationDefined: evt];
	else
		[super sendEvent: evt];
}

@end

int
main (int argc, const char *argv[])
{
	return NSApplicationMain (argc, argv);
}
