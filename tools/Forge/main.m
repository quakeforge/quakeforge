#import <AppKit/AppKit.h>
#import "Controller.h"

#define APP_NAME @"GNUstep"

int main(int argc, const char *argv[], const char *env[]) 
{
	[NSApplication sharedApplication];
	[NSApp setDelegate: [[Controller alloc] init]];

	return NSApplicationMain (argc, argv);
}
