
#import <AppKit/AppKit.h>
#include <sys/stat.h>

#define BASEPATHKEY		"basepath"
#define	MAPNAMESKEY		"maps"
#define DESCKEY			"desc"
#define	WADSKEY			"wads"
#define	BSPFULLVIS		"bspfullvis"
#define	BSPFASTVIS		"bspfastvis"
#define	BSPNOVIS		"bspnovis"
#define	BSPRELIGHT		"bsprelight"
#define	BSPLEAKTEST		"bspleaktest"
#define	BSPENTITIES		"bspentities"

#define	SUBDIR_ENT		"progs"		// subdir names in heirarchy
#define	SUBDIR_MAPS		"maps"
#define SUBDIR_GFX		"gfx"

extern	id project_i;

@interface Project: NSObject
{
	id	projectInfo;		// dictionary storage of project info

	id	basepathinfo_i;		// outlet to base path info textfield
	id	mapbrowse_i;		// outlet to QuakeEd Maps browser
	id	currentmap_i;		// outlet to current map textfield
	id	mapList;			// list of map names (Storage)
	id	descList;			// list of map descriptions (Storage)
	id	wadList;			// list of wad names (Storage)
	
	id	pis_panel_i;		// outlet to Project Info Settings (PIS) panel

	id	pis_basepath_i;		// outlet to PIS->base path
	id	pis_wads_i;			// outlet to PIS->wad browser	
	id	pis_fullvis_i;		// outlet to PIS->full vis command
	id	pis_fastvis_i;		// outlet to PIS->fast vis command
	id	pis_novis_i;		// outlet to PIS->no vis command
	id	pis_relight_i;		// outlet to PIS->relight command
	id	pis_leaktest_i;		// outlet to PIS->leak test command

	id	BSPoutput_i;		// outlet to Text
	
	NSString	*path_projectinfo;	// path of QE_Project file

	NSString	*path_basepath; 	// base path of heirarchy

	NSString	*path_progdir;		// derived from basepath
	NSString	*path_mapdirectory; // derived from basepath
	NSString	*path_finalmapdir;	// derived from basepath

	NSString	*path_wad8;		// path of texture WAD for cmd-8 key
	NSString	*path_wad9;		// path of texture WAD for cmd-9 key
	NSString	*path_wad0;		// path of texture WAD for cmd-0 key

	NSString	*string_fullvis;	// cmd-line parm
	NSString	*string_fastvis;	// cmd-line parm
	NSString	*string_novis;		// cmd-line parm
	NSString	*string_relight;	// cmd-line parm
	NSString	*string_leaktest;	// cmd-line parm
	NSString	*string_entities;	// cmd-line parm

	int	showDescriptions;	// 1 = show map descs in browser

	time_t	lastModified;	// last time project file was modified
}

- initProject;
- initVars;

- (char *)currentProjectFile;

- setTextureWad: (char *)wf;

- addToOutput: (NSString *) string;
- clearBspOutput: sender;
- initProjSettings;
- changeChar: (char) f to: (char) t in: (id) obj;
- (int) searchForString: (NSString *) str in: (id) obj;

- parseProjectFile;		// read defaultsdatabase for project path
- openProjectFile: (NSString *) path;	// called by openProject and newProject
- openProject;
- clickedOnMap:sender;		// called if clicked on map in browser
- clickedOnWad:sender;		// called if clicked on wad in browser

//	methods to query the project file

- (NSString *) mapDirectory;
- (NSString *) finalMapDirectory;
- (NSString *) progDirectory;

- (NSString *) WAD8;
- (NSString *) WAD9;
- (NSString *) WAD0;

- (NSString *) fullVisCommand;
- (NSString *) fastVisCommand;
- (NSString *) noVisCommand;
- (NSString *) relightCommand;
- (NSString *) leakTestCommand;
- (NSString *) entitiesCommand;

@end

void changeString(char cf, char ct, char *string);

