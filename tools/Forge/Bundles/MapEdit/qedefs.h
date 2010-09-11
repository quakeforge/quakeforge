#ifndef qedefs_h
#define qedefs_h

#include <AppKit/AppKit.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <math.h>
#include <unistd.h>
#include <sys/fcntl.h>

#include "UserPath.h"
#include "cmdlib.h"
#include "mathlib.h"

#include "EntityClass.h"
#include	"Project.h"
#include "QuakeEd.h"
#include "Map.h"
#include "TexturePalette.h"
#include "SetBrush.h"
#include "render.h"
#include "Entity.h"

#include "XYView.h"
#include "CameraView.h"
#include "ZView.h"
#include "ZScrollView.h"
#include	"Preferences.h"
#include	"InspectorControl.h"
#include "PopScrollView.h"
#include "KeypairView.h"
#include "Things.h"
#include "TextureView.h"
#include "Clipper.h"


void        PrintRect (NSRect * r);
int         FileTime (char *path);
void        Sys_UpdateFile (char *path, char *netpath);
void        CleanupName (char *in, char *out);

extern BOOL in_error;
void        Error (char *error, ...);

#define	MAXTOKEN	128
extern char token[MAXTOKEN];
extern int  scriptline;
void        StartTokenParsing (char *data);
boolean     GetToken (boolean crossline);	// returns false at eof
void        UngetToken ();


#define	FN_CMDOUT		"/tmp/QuakeEdCmd.txt"
#define	FN_TEMPSAVE		"/qcache/temp.map"
#define	FN_AUTOSAVE		"/qcache/AutoSaveMap.map"
#define	FN_CRASHSAVE	"/qcache/ErrorSaveMap.map"
#define	FN_DEVLOG		"/qcache/devlog"

#endif // qedefs_h
