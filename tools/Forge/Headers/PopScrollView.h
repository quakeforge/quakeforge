#import <AppKit/AppKit.h>

@interface PopScrollView: NSScrollView
{
	NSButton	*button1;
	NSButton	*button2;
}

- (id) initWithFrame: (NSRect) frameRect button1: (NSButton *) b1 button2: (NSButton *) b2;
- (void) tile;

@end
