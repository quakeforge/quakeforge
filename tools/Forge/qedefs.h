
#import <AppKit/AppKit.h>
#import <ctype.h>
#import <sys/types.h>
#import <sys/dir.h>

#import "UserPath.h"
#import "cmdlib.h"
#import "mathlib.h"

#import "EntityArray.h"
#import "EntityClass.h"
#import	"Project.h"
#import "Forge.h"
#import "Map.h"
#import "TexturePalette.h"
#import "SetBrush.h"
#import "render.h"
#import "Entity.h"

#import "XYView.h"
#import "CameraView.h"
#import "ZView.h"
#import "ZScrollView.h"
#import	"Preferences.h"
#import	"InspectorControl.h"
#import "PopScrollView.h"
#import "KeypairView.h"
#import "Things.h"
#import "TextureView.h"
#import "Clipper.h"


void PrintRect (NSRect *r);
int	FileTime (char *path);
void Sys_UpdateFile (char *path, char *netpath);
void CleanupName (char *in, char *out);

extern	BOOL	in_error;
void Error (char *error, ...);

#define	MAXTOKEN	128
extern	char	token[MAXTOKEN];
extern	int		scriptline;
void	StartTokenParsing (char *data);
qboolean GetToken (qboolean crossline);	// returns false at eof
void UngetToken ();


#define	FN_CMDOUT		"/tmp/QuakeEdCmd.txt"
#define	FN_TEMPSAVE		"/qcache/temp.map"
#define	FN_AUTOSAVE		"/qcache/AutoSaveMap.map"
#define	FN_CRASHSAVE	"/qcache/ErrorSaveMap.map"
#define	FN_DEVLOG		"/qcache/devlog"

