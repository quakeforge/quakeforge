#import <AppKit/AppKit.h>
#include <sys/stat.h>

#define EntSubdir		@"progs"
#define MapSubdir		@"maps"

#define	MapNames		@"mapNames"
#define Descriptions	@"descriptions"
#define	WadFiles		@"wadFiles"
#define	BSPFullVis		@"bspFullVisCommand"
#define	BSPFastVis		@"bspFastVisCommand"
#define	BSPNoVis		@"bspNoVisCommand"
#define	BSPRelight		@"bspRelightCommand"
#define	BSPLeakTest		@"bspLeakTestCommand"
#define	BSPEntities		@"bspEntitiesCommand"

@interface Project: NSObject
{
	id	projectInfo;		// NSDictionary storage of project info

	id	mapNames;			// Array of map names (NSString)
	id	descriptions;		// Array of map descriptions (NSString)
	id	wadNames;			// Array of wad names (NSString)

	id	pBasePathField;		// outlet to base path info (NSTextField)
	id	pCurrentMapField;	// outlet to current map textfield
	id	pBSPOutputView; 	// outlet to a command output NSTextView
	id	pMapBrowser;		// outlet to Forge Maps browser
	
	id	projectPrefsPanel;	// outlet to Project Preferences panel

	id	ppBasePath;			// outlet to PPanel->"Base Path"
	id	ppWadBrowser;		// outlet to PPanel->"WAD Browser"
	id	ppFullVisField;		// outlet to PPanel->"Full VIS Compile"
	id	ppFastVisField;		// outlet to PPanel->"Fast VIS Compile"
	id	ppNoVisField;		// outlet to PPanel->"No VIS Compile"
	id	ppRelightField;		// outlet to PPanel->"Relighting Compile"
	id	ppLeakTestField;	// outlet to PPanel->"Leak Check Compile"
	
	NSString	*_projectPath;	// path to *.forge file

	NSString	*_basePath; 	// base path of heirarchy
	NSString	*_progsPath;	// derived from basepath
	NSString	*_mapPath;		// derived from basepath
	NSString	*_finalMapPath;	// derived from basepath

	NSString	*_wad8;		// path of texture WAD for cmd-8 key
	NSString	*_wad9;		// path of texture WAD for cmd-9 key
	NSString	*_wad0;		// path of texture WAD for cmd-0 key

	NSString	*_fullVisCommand;	// cmd-line parm
	NSString	*_fastVisCommand;	// cmd-line parm
	NSString	*_noVisCommand; 	// cmd-line parm
	NSString	*_relightCommand;	// cmd-line parm
	NSString	*_leakTestCommand;	// cmd-line parm
	NSString	*_entitiesCommand;	// cmd-line parm

	int	showDescriptions;	// 1 = show map descs in browser

	time_t	lastModified;	// last time project file was modified
}

- (id) initProject;
- (id) initVars;
- (void) initProjectSettings;

- (void) setTextureWad: (NSString *) wadFile;

- (void) addToOutput: (NSString *) string;

- (void) changeString: (NSString *) f to: (NSString *) t in: (id) obj;
- (BOOL) searchForString: (NSString *) str in: (id) obj;

- (void) parseProjectFile;				// read database for project path
- (void) openProjectWithFile: (NSString *) path;	// called by openProject and newProject
- (id) openProject;

- (void) wadWasClicked: (id) sender;	// called if clicked on wad in browser
- (void) mapWasClicked: (id) sender;	// called if clicked on map in browser
- (void) clearBspOutput: (id) sender;	// Called if the BSP output view should
										//	be cleared

//	methods to query the project file
- (NSString *) currentProject;
- (NSString *) mapDirectory;
- (NSString *) finalMapDirectory;
- (NSString *) progDirectory;

- (NSString *) wad8;
- (NSString *) wad9;
- (NSString *) wad0;

- (NSString *) fullVisCommand;
- (NSString *) fastVisCommand;
- (NSString *) noVisCommand;
- (NSString *) relightCommand;
- (NSString *) leakTestCommand;
- (NSString *) entitiesCommand;

@end

void changeString(char cf, char ct, char *string);

