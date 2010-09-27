#include <dirent.h>

#include "QF/quakeio.h"
#include "QF/script.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "EntityClass.h"

@implementation EntityClass

static int
parse_vector (script_t * script, vec3_t vec)
{
	int         r;

	if (!Script_GetToken (script, 0))
		return 0;
	if (strcmp (Script_Token (script), "("))
		return 0;

	r = sscanf (script->p, "%f %f %f)", &vec[0], &vec[1], &vec[2]);
	if (r != 3)
		return 0;

	while (strcmp (Script_Token (script), ")")) {
		if (!Script_GetToken (script, 0))
			return 0;
	}
	return 1;
}

// the classname, color triple, and bounding box are parsed out of comments
// A ? size means take the exact brush size.
//
// /*QUAKED <classname> (0 0 0) ?
// /*QUAKED <classname> (0 0 0) (-8 -8 -8) (8 8 8)
//
// Flag names can follow the size description:
//
// /*QUAKED func_door (0 .5 .8) ? START_OPEN STONE_SOUND DOOR_DONT_LINK GOLD_KEY SILVER_KEY

-initFromText:(const char *) text source:(const char *) filename
{
	const char *t;
	size_t      len;
	int         i;
	script_t   *script;

	[super init];

	text += strlen ("/*QUAKED ");

	script = Script_New ();
	Script_Start (script, filename, text);

	// grab the name
	if (!Script_GetToken (script, 0))
		return 0;
	if (!strcmp (Script_Token (script), "*/"))
		return 0;
	name = strdup (Script_Token (script));

	// grab the color
	if (!parse_vector (script, color))
		return 0;
	// get the size 
	if (!Script_GetToken (script, 0))
		return 0;
	if (!strcmp (Script_Token (script), "(")) {
		Script_UngetToken (script);
		if (!parse_vector (script, mins))
			return 0;
		if (!parse_vector (script, maxs))
			return 0;
		esize = esize_fixed;
	} else if (!strcmp (Script_Token (script), "?")) {
		// use the brushes
		esize = esize_model;
	} else {
		return 0;
	}

	// get the flags
	// any remaining words on the line are parm flags
	for (i = 0; i < MAX_FLAGS; i++) {
		if (!Script_TokenAvailable (script, 0))
			break;
		Script_GetToken (script, 0);
		flagnames[i] = strdup (Script_Token (script));
	}
	while (Script_TokenAvailable (script, 0))
		Script_GetToken (script, 0);

// find the length until close comment
	for (t = script->p; t[0] && !(t[0] == '*' && t[1] == '/'); t++)
		;

// copy the comment block out
	len = t - text;
	comments = malloc (len + 1);
	memcpy (comments, text, len);
	comments[len] = 0;

	return self;
}

-(esize_t) esize
{
	return esize;
}

-(const char *) classname
{
	return name;
}

-(float *) mins
{
	return mins;
}

-(float *) maxs
{
	return maxs;
}

-(float *) drawColor
{
	return color;
}

-(const char *) comments
{
	return comments;
}


-(const char *) flagName:(unsigned) flagnum
{
	if (flagnum >= MAX_FLAGS)
		Sys_Error ("EntityClass flagName: bad number");
	return flagnames[flagnum];
}

@end
//===========================================================================

#define THING EntityClassList
#include "THING+NSArray.m"

@implementation EntityClassList
/*
=================
insertEC:
=================
*/
- (void) insertEC:ec
{
	const char  *name;
	unsigned int i;

	name =[ec classname];
	for (i = 0; i <[self count]; i++) {
		if (strcasecmp (name,[[self objectAtIndex:i] classname]) < 0) {
			[self insertObject: ec atIndex:i];
			return;
		}
	}
	[self addObject:ec];
}


/*
=================
scanFile
=================
*/
-(void) scanFile:(const char *) filename
{
	int         size, line;
	char       *data;
	id          cl;
	int         i;
	const char *path;
	QFile      *file;

	path = va ("%s/%s", source_path, filename);

	file = Qopen (path, "rt");
	if (!file)
		return;
	size = Qfilesize (file);
	data = malloc (size + 1);
	size = Qread (file, data, size);
	data[size] = 0;
	Qclose (file);

	line = 1;
	for (i = 0; i < size; i++) {
		if (!strncmp (data + i, "/*QUAKED", 8)) {
			cl =[[EntityClass alloc] initFromText:(data + i)
			source:va ("%s:%d", filename, line)];
			if (cl)
				[self insertEC:cl];
		} else if (data[i] == '\n') {
			line++;
		}
	}

	free (data);
}


/*
=================
scanDirectory
=================
*/
-(void) scanDirectory
{
	int         count, i;
	struct dirent **namelist, *ent;

	[self removeAllObjects];

	count = scandir (source_path, &namelist, NULL, NULL);

	for (i = 0; i < count; i++) {
		int         len;

		ent = namelist[i];
		len = strlen (ent->d_name);
		if (len <= 3)
			continue;
		if (!strcmp (ent->d_name + len - 3, ".qc"))
			[self scanFile:ent->d_name];
	}
}


id          entity_classes_i;


-initForSourceDirectory:(const char *) path
{
	self = [super init];
	array = [[NSMutableArray alloc] init];

	source_path = strdup (path);	//FIXME leak?
	[self scanDirectory];

	entity_classes_i = self;

	nullclass =[[EntityClass alloc]
	initFromText: "/*QUAKED UNKNOWN_CLASS (0 0.5 0) ?*/" source:va ("%s:%d", __FILE__,
					__LINE__ -
					1)];

	return self;
}

-(id) classForName:(const char *) name
{
	unsigned int i;
	id          o;

	for (i = 0; i <[self count]; i++) {
		o =[self objectAtIndex:i];
		if (!strcmp (name,[o classname]))
			return o;
	}

	return nullclass;
}


@end
