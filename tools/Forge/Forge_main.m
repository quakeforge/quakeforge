#import <AppKit/AppKit.h>

NSString *commandOutput = @"/tmp/Forge/command.txt";
NSString *developerLogFile = @"/tmp/Forge/developer.log";
NSString *tempSaveFile = @"/tmp/Forge/temp.map";
NSString *autoSaveFile = @"/tmp/Forge/autoSave.map";
NSString *errorSaveFile = @"/tmp/Forge/errorSave.map";

int
main (int argc, char *argv[]) {

	[NSApplication new];
	if ([NSBundle loadNibNamed: @"Forge" owner: NSApp])
		[NSApp run];
	    
	[NSApp release];
	return 0;
}
