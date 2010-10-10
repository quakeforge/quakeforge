#ifndef Project_h
#define Project_h

#include <AppKit/AppKit.h>
#include <sys/stat.h>

#define BASEPATHKEY "basepath"
#define MAPNAMESKEY "maps"
#define DESCKEY     "desc"
#define WADSKEY     "wads"
#define BSPFULLVIS  "bspfullvis"
#define BSPFASTVIS  "bspfastvis"
#define BSPNOVIS    "bspnovis"
#define BSPRELIGHT  "bsprelight"
#define BSPLEAKTEST "bspleaktest"
#define BSPENTITIES "bspentities"

#define SUBDIR_ENT  @"progs"         // subdir names in heirarchy
#define SUBDIR_MAPS @"maps"
#define SUBDIR_GFX  @"gfx"

extern id  project_i;

@interface Project: NSObject
{
	id  projectInfo;                // dictionary storage of project info

	id  basepathinfo_i;             // outlet to base path info textfield
	id  mapbrowse_i;                // outlet to QuakeEd Maps browser
	id  currentmap_i;               // outlet to current map textfield
	struct plitem_s *mapList;       // list of map names
	struct plitem_s *descList;      // list of map descriptions
	struct plitem_s *wadList;       // list of wad names

	id  pis_panel_i;                // outlet to Project Info Settings (PIS)
	                                // panel

	id  pis_basepath_i;             // outlet to PIS->base path
	id  pis_wads_i;                 // outlet to PIS->wad browser
	id  pis_fullvis_i;              // outlet to PIS->full vis command
	id  pis_fastvis_i;              // outlet to PIS->fast vis command
	id  pis_novis_i;                // outlet to PIS->no vis command
	id  pis_relight_i;              // outlet to PIS->relight command
	id  pis_leaktest_i;             // outlet to PIS->leak test command

	id  BSPoutput_i;                // outlet to Text

	NSString *path_projectinfo;     // path of QE_Project file

	NSString *path_basepath;        // base path of heirarchy

	NSString *path_progdir;         // derived from basepath
	NSString *path_mapdirectory;    // derived from basepath
	NSString *path_finalmapdir;     // derived from basepath

	char    path_wad8[128];         // path of texture WAD for cmd-8 key
	char    path_wad9[128];         // path of texture WAD for cmd-9 key
	char    path_wad0[128];         // path of texture WAD for cmd-0 key

	const char *string_fullvis;     // cmd-line parm
	const char *string_fastvis;     // cmd-line parm
	const char *string_novis;       // cmd-line parm
	const char *string_relight;     // cmd-line parm
	const char *string_leaktest;    // cmd-line parm
	const char *string_entities;    // cmd-line parm

	int  showDescriptions;          // 1 = show map descs in browser

	time_t  lastModified;           // last time project file was modified
}

- (id) initProject;
- (id) initVars;

- (NSString *) currentProjectFile;

- (id) setTextureWad: (const char *)wf;

- (id) addToOutput: (const char *)string;
- (id) clearBspOutput: (id)sender;
- (id) initProjSettings;

- (id) parseProjectFile;            // read defaultsdatabase for project path
- (id) openProjectFile: (NSString *)path; // called by openProject, newProject
- (id) openProject;
- (id) clickedOnMap: sender;        // called if clicked on map in browser
- (id) clickedOnWad: sender;        // called if clicked on wad in browser

//
//  methods to query the project file
//
- (NSString *) baseDirectoryPath;
- (NSString *) getMapDirectory;
- (NSString *) getFinalMapDirectory;
- (NSString *) getProgDirectory;

- (const char *) getWAD8;
- (const char *) getWAD9;
- (const char *) getWAD0;

- (const char *) getFullVisCmd;
- (const char *) getFastVisCmd;
- (const char *) getNoVisCmd;
- (const char *) getRelightCmd;
- (const char *) getLeaktestCmd;
- (const char *) getEntitiesCmd;

@end

#endif // Project_h
