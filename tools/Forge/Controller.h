#import <Foundation/Foundation.h>

@interface Controller: NSObject
{
@public
}

// App delegate methods
- (BOOL) application: (NSApplication *) app openFile: (NSString *) filename;
- (BOOL) application: (NSApplication *) app openTempFile: (NSString *) filename;
- (BOOL) applicationOpenUntitledFile: (NSApplication *) app;
- (BOOL) applicationShouldOpenUntitledFile: (NSApplication *) app;
- (BOOL) applicationShouldTerminate: (NSApplication *) app;
- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication *) app;

// Notifications
- (void) applicationDidFinishLaunching: (NSNotification *) not;
- (void) applicationWillFinishLaunching: (NSNotification *) not;
- (void) applicationWillTerminate: (NSNotification *) not;

// Action methods
- (void) createNew: (id) sender;
- (void) createNewProject: (id) sender;
- (void) infoPanel: (id) sender;
- (void) open: (id) sender;
- (void) openProject: (id) sender;
- (void) saveAll: (id) sender;

@end
