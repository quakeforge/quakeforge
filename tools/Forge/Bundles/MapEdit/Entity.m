
#include "QF/dstring.h"
#include "QF/script.h"
#include "QF/sys.h"

#include "Entity.h"
#include "EntityClass.h"
#include "TexturePalette.h"
#include "SetBrush.h"
#include "Map.h"
#include "CameraView.h"

#define THING Entity
#include "THING+NSArray.m"

@implementation Entity

vec3_t      bad_mins = { -8, -8, -8 };
vec3_t      bad_maxs = { 8, 8, 8 };

-createFixedBrush:(vec3_t) org
{
	vec3_t      emins, emaxs;
	float      *v, *v2, *color;
	id          new;
	texturedef_t td;

// get class
	new =[entity_classes_i classForName: [self valueForQKey:"classname"]];
	if (new) {
		v =[new mins];
		v2 =[new maxs];
	} else {
		v = bad_mins;
		v2 = bad_maxs;
	}

	color =[new drawColor];

	modifiable = NO;
	memset (&td, 0, sizeof (td));
	strcpy (td.texture, "entity");

	VectorAdd (org, v, emins);
	VectorAdd (org, v2, emaxs);
	new =[[SetBrush alloc] initOwner: self mins: emins maxs: emaxs texture:&td];
	[new setEntityColor:color];

	[self addObject:new];

	return self;
}

-initClass:(const char *) classname
{
	id          new;
	esize_t     esize;
	char        value[80];
	vec3_t      min, max;
	float      *v;

	self = [super init];
	array = [[NSMutableArray alloc] init];

	modifiable = YES;

	[self setKey: "classname" toValue:classname];

// get class
	new =[entity_classes_i classForName: [self valueForQKey:"classname"]];
	if (!new)
		esize = esize_model;
	else
		esize =[new esize];

// create a brush if needed
	if (esize == esize_fixed) {
		v =[new mins];
		[[map_i selectedBrush] getMins: min maxs:max];
		VectorSubtract (min, v, min);

		sprintf (value, "%i %i %i", (int) min[0], (int) min[1], (int) min[2]);
		[self setKey: "origin" toValue:value];

		[self createFixedBrush:min];
	} else
		modifiable = YES;

	return self;
}


-(void) dealloc
{
	epair_t    *e, *n;

	for (e = epairs; e; e = n) {
		n = e->next;
		free (e->key);
		free (e->value);
		free (e);
	}
	[array release];
	[super dealloc];
}

-(BOOL) modifiable
{
	return modifiable;
}

-setModifiable:(BOOL) m
{
	modifiable = m;
	return self;
}

-(void) removeObject:o
{
	[super removeObject:o];
	if ([self count])
		return;
// the entity is empty, so remove the entire thing
	if (self ==[map_i objectAtIndex:0])
		return;							// never remove the world

	[map_i removeObject:self];
	[self release];
}


-(const char *) valueForQKey:(const char *) k
{
	epair_t    *e;

	for (e = epairs; e; e = e->next)
		if (!strcmp (k, e->key))
			return e->value;
	return "";
}

-getVector:(vec3_t) v forKey:(const char *) k
{
	const char *c;

	c =[self valueForQKey:k];

	v[0] = v[1] = v[2] = 0;

	sscanf (c, "%f %f %f", &v[0], &v[1], &v[2]);

	return self;
}

-print
{
	epair_t    *e;

	for (e = epairs; e; e = e->next)
		printf ("%20s : %20s\n", e->key, e->value);

	return self;
}

-setKey:(const char *)
k           toValue:(const char *) v
{
	epair_t    *e;

	while (*k && *k <= ' ')
		k++;
	if (!*k)
		return self;					// don't set NULL values

	for (e = epairs; e; e = e->next) {
		if (!strcmp (k, e->key)) {
			free (e->value);
			e->value = strdup (v);
			return self;
		}
	}

	e = malloc (sizeof (epair_t));

	e->key = strdup (k);
	e->value = strdup (v);
	e->next = epairs;
	epairs = e;

	return self;
}

-(int) numPairs
{
	int         i;
	epair_t    *e;

	i = 0;
	for (e = epairs; e; e = e->next)
		i++;
	return i;
}

-(epair_t *) epairs
{
	return epairs;
}

-removeKeyPair:(char *) key
{
	epair_t    *e, *e2;

	if (!epairs)
		return self;
	e = epairs;
	if (!strcmp (e->key, key)) {
		epairs = e->next;
		free (e);
		return self;
	}

	for (; e; e = e->next) {
		if (e->next && !strcmp (e->next->key, key)) {
			e2 = e->next;
			e->next = e2->next;
			free (e2);
			return self;
		}
	}

	printf ("WARNING: removeKeyPair: %s not found\n", key);
	return self;
}


/*
=============
targetname

If the entity does not have a "targetname" key, a unique one is generated
=============
*/
-(const char *) targetname
{
	const char *t;
	int         i, count;
	id          ent;
	int         tval, maxt;
	char        name[20];

	t =[self valueForQKey:"targetname"];
	if (t && t[0])
		return t;

// make a unique name of the form t<number>
	count =[map_i count];
	maxt = 0;
	for (i = 1; i < count; i++) {
		ent =[map_i objectAtIndex:i];
		t =[ent valueForQKey:"targetname"];
		if (!t || t[0] != 't')
			continue;
		tval = atoi (t + 1);
		if (tval > maxt)
			maxt = tval;
	}

	sprintf (name, "t%i", maxt + 1);

	[self setKey: "targetname" toValue:name];

	return[self valueForQKey:"targetname"];
										// so it's not on the stack
}

/*
==============================================================================

FILE METHODS

==============================================================================
*/

int         nument;

-initFromScript:(script_t *) script
{
	char       *key;
	id          eclass, brush;
	const char *spawn;
	vec3_t      emins, emaxs;
	vec3_t      org;
	texturedef_t td;
	esize_t     esize;
	int         i, c;
	float      *color;

	self = [super init];
	array = [[NSMutableArray alloc] init];

	if (!Script_GetToken (script, true)) {
		[self dealloc];
		return nil;
	}

	if (strcmp (Script_Token (script), "{"))
		Sys_Error ("initFromFileP: { not found");

	do {
		if (!Script_GetToken (script, true))
			break;
		if (!strcmp (Script_Token (script), "}"))
			break;
		if (!strcmp (Script_Token (script), "{")) {
			// read a brush
			brush =[[SetBrush alloc] initFromScript: script owner:self];
			[self addObject:brush];
		} else {
			// read a key / value pair
			key = strdup (Script_Token (script));
			Script_GetToken (script, false);
			[self setKey: key toValue:Script_Token (script)];
			free (key);
		}
	} while (1);

	nument++;

// get class
	spawn =[self valueForQKey:"classname"];
	eclass =[entity_classes_i classForName:spawn];

	esize =[eclass esize];

	[self getVector: org forKey:"origin"];

	if ([self count] && esize != esize_model) {
		printf ("WARNING:Entity with brushes and wrong model type\n");
		[self removeAllObjects];
	}

	if (![self count] && esize == esize_model) {
		printf ("WARNING:Entity with no brushes and esize_model: %s\n", [self valueForQKey:"classname"]);
		[texturepalette_i getTextureDef:&td];
		for (i = 0; i < 3; i++) {
			emins[i] = org[i] - 8;
			emaxs[i] = org[i] + 8;
		}
		brush =[[SetBrush alloc] initOwner: self mins: emins maxs: emaxs texture:&td];
		[self addObject:brush];
	}
// create a brush if needed
	if (esize == esize_fixed)
		[self createFixedBrush:org];
	else
		modifiable = YES;

// set all the brush colors
	color =[eclass drawColor];

	c =[self count];
	for (i = 0; i < c; i++) {
		brush =[self objectAtIndex:i];
		[brush setEntityColor:color];
	}

	return self;
}


-writeToFILE:(FILE *)
f           region:(BOOL) reg;
{
	epair_t    *e;
	int         i;
	id          new;
	char        value[80];
	vec3_t      mins, maxs, org;
	const vec_t *v;
	BOOL        temporg;
	char        oldang[80];

	temporg = NO;
	if (reg) {
		if (!strcmp ([self valueForQKey:"classname"], "info_player_start")) {
												// move the playerstart
												// temporarily to the camera
												// position
			temporg = YES;
			strcpy (oldang,[self valueForQKey:"angle"]);
			sprintf (value, "%i", (int) ([cameraview_i yawAngle] * 180 / M_PI));
			[self setKey: "angle" toValue:value];
		} else if (self !=[map_i objectAtIndex:0]
		&&[[self objectAtIndex:0] regioned])
			return self;				// skip the entire entity definition
	}

	fprintf (f, "{\n");

// set an origin epair
	if (!modifiable) {
		[[self objectAtIndex: 0] getMins: mins maxs:maxs];
		if (temporg) {
			[cameraview_i getOrigin:mins];
			mins[0] -= 16;
			mins[1] -= 16;
			mins[2] -= 48;
		}
		new =[entity_classes_i classForName:
		[self valueForQKey:"classname"]];
		if (new)
			v =[new mins];
		else
			v = vec3_origin;

		VectorSubtract (mins, v, org);
		sprintf (value, "%i %i %i", (int) org[0], (int) org[1], (int) org[2]);
		[self setKey: "origin" toValue:value];
	}

	for (e = epairs; e; e = e->next)
		fprintf (f, "\"%s\"\t\"%s\"\n", e->key, e->value);

// fixed size entities don't save out brushes
	if (modifiable) {
		for (i = 0; i <[self count]; i++)
			[[self objectAtIndex: i] writeToFILE: f region:reg];
	}

	fprintf (f, "}\n");

	if (temporg)
		[self setKey: "angle" toValue:oldang];

	return self;
}

/*
==============================================================================

INTERACTION

==============================================================================
*/

@end
