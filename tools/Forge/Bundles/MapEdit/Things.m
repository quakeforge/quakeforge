#include "QF/sys.h"

#include "Things.h"
#include "QuakeEd.h"
#include "Map.h"
#include "EntityClass.h"
#include "KeypairView.h"
#include "Project.h"

id          things_i;

@implementation Things

-init
{
	[super init];

	things_i = self;
	lastSelected = 0;

	return self;
}

-(void)awakeFromNib
{
	//FIXME this should not be needed (bug in gnustep?)
	[flags_i selectAll: self];
	[flags_i deselectAllCells];
}

//
//  Load the TEXT object with the entity comment
//
-loadEntityComment:(id) obj
{
	[entity_comment_i selectAll:self];
	[entity_comment_i replaceCharactersInRange: [entity_comment_i selectedRange] withString: [NSString stringWithCString:[obj
	 comments]]];

	return self;
}


-initEntities
{
	char       *path;

	path =[project_i getProgDirectory];

	[prog_path_i setStringValue: [NSString stringWithCString:path]];

	[[EntityClassList alloc] initForSourceDirectory:path];

	[self loadEntityComment: [entity_classes_i objectAtIndex:lastSelected]];
	[entity_browser_i loadColumnZero];
	[[entity_browser_i matrixInColumn: 0] selectCellAtRow: lastSelected column:0];

	[entity_browser_i setDoubleAction: @selector (doubleClickEntity:)];

	return self;
}

-selectEntity:sender
{
	id          matr;

	matr =[sender matrixInColumn:0];
	lastSelected =[matr selectedRow];
	[self loadEntityComment: [entity_classes_i objectAtIndex:lastSelected]];
	[quakeed_i makeFirstResponder:quakeed_i];

	return self;
}

-doubleClickEntity:sender
{
	[map_i makeEntity:sender];
	[quakeed_i makeFirstResponder:quakeed_i];
	return self;
}

-(char *) spawnName
{
	return[[entity_classes_i objectAtIndex:lastSelected] classname];
}


//
//  Flush entity classes & reload them!
//
-reloadEntityClasses:sender
{
	EntityClass *ent;
	char       *path;

	path = (char *)[prog_path_i stringValue];
	if (!path || !path[0]) {
		path =[project_i getProgDirectory];
		[prog_path_i setStringValue: [NSString stringWithCString:path]];
	}
	// Free all entity info in memory...
	[entity_classes_i removeAllObjects];
	[entity_classes_i release];

	// Now, RELOAD!
	[[EntityClassList alloc] initForSourceDirectory:path];

	lastSelected = 0;
	ent =[entity_classes_i objectAtIndex:lastSelected];
	[self loadEntityComment: [entity_classes_i objectAtIndex:lastSelected]];

	[entity_browser_i loadColumnZero];
	[[entity_browser_i matrixInColumn: 0] selectCellAtRow: lastSelected column:0];

	[self newCurrentEntity];			// in case flags changed

	return self;
}


-selectClass:(const char *) class
{
	id          classent;

	classent =[entity_classes_i classForName:class];
	if (!classent)
		return self;
	lastSelected =[entity_classes_i indexOfObject:classent];

	if (lastSelected < 0)
		lastSelected = 0;

	[self loadEntityComment:classent];
	[[entity_browser_i matrixInColumn: 0] selectCellAtRow: lastSelected column:0];
	[[entity_browser_i matrixInColumn: 0] scrollCellToVisibleAtRow: lastSelected column:0];

	return self;
}


-newCurrentEntity
{
	id          ent, classent, cell;
	const char *classname;
	int         r, c;
	const char *flagname;
	int         flags;

	ent =[map_i currentEntity];
	classname =[ent valueForQKey:"classname"];
	if (ent !=[map_i objectAtIndex:0])
		[self selectClass:classname];	// don't reset for world
	classent =[entity_classes_i classForName:classname];
	flagname =[ent valueForQKey:"spawnflags"];
	if (!flagname)
		flags = 0;
	else
		flags = atoi (flagname);

	//[flags_i setAutodisplay:NO];
	for (r = 0; r < 4; r++)
		for (c = 0; c < 3; c++) {
			cell =[flags_i cellAtRow: r column:c];
			if (c < 2) {
				flagname =[classent flagName:c * 4 + r];
				[cell setTitle: [NSString stringWithCString:flagname]];
			}
			[cell setIntValue:(flags & (1 << ((c * 4) + r))) > 0];
		}
	//[flags_i setAutodisplay:YES];
	[flags_i display];

//  [keyInput_i setStringValue: ""];
//  [valueInput_i setStringValue: ""];

	[keypairview_i calcViewSize];
	[keypairview_i display];

	[quakeed_i makeFirstResponder:quakeed_i];
	return self;
}

//
//  Clicked in the Keypair view - set as selected
//
-setSelectedKey:(epair_t *) ep;
{
	[keyInput_i setStringValue: [NSString stringWithCString:ep->key]];
	[valueInput_i setStringValue: [NSString stringWithCString:ep->value]];
	[valueInput_i selectText:self];
	return self;
}

-clearInputs
{
//  [keyInput_i setStringValue: ""];
//  [valueInput_i setStringValue: ""];

	[quakeed_i makeFirstResponder:quakeed_i];
	return self;
}

//
//  Action methods
//

-addPair:sender
{
	char       *key, *value;

	key = (char *)[keyInput_i stringValue];
	value = (char *)[valueInput_i stringValue];

	[[map_i currentEntity] setKey: key toValue:value];

	[keypairview_i calcViewSize];
	[keypairview_i display];

	[self clearInputs];
	[quakeed_i updateXY];

	return self;
}

-delPair:sender
{
	[quakeed_i makeFirstResponder:quakeed_i];

	[[map_i currentEntity] removeKeyPair:(char *)[keyInput_i stringValue]];

	[keypairview_i calcViewSize];
	[keypairview_i display];

	[self clearInputs];

	[quakeed_i updateXY];

	return self;
}


//
//  Set the key/value fields to "angle <button value>"
//
-setAngle:sender
{
	const char *title;
	char        value[10];

	title =[[[sender selectedCell] title] cString];
	if (!strcmp (title, "Up"))
		strcpy (value, "-1");
	else if (!strcmp (title, "Dn"))
		strcpy (value, "-2");
	else
		strcpy (value, title);

	[keyInput_i setStringValue:@"angle"];
	[valueInput_i setStringValue: [NSString stringWithCString:value]];
	[self addPair:NULL];

	[self clearInputs];

	[quakeed_i updateXY];

	return self;
}

-setFlags:sender
{
	int         flags;
	int         r, c, i;
	id          cell;
	char        str[20];

	[self clearInputs];
	flags = 0;

	for (r = 0; r < 4; r++)
		for (c = 0; c < 3; c++) {
			cell =[flags_i cellAtRow: r column:c];
			i = ([cell intValue] > 0);
			flags |= (i << ((c * 4) + r));
		}

	if (!flags)
		[[map_i currentEntity] removeKeyPair:"spawnflags"];
	else {
		sprintf (str, "%i", flags);
		[[map_i currentEntity] setKey: "spawnflags" toValue:str];
	}

	[keypairview_i calcViewSize];
	[keypairview_i display];

	return self;
}


//
//  Fill the Entity browser
//  (Delegate method - delegated in Interface Builder)
//
-(void) browser: sender createRowsForColumn:(int) column inMatrix: matrix
{
	id          cell;
	int         max;
	int         i;
	id          object;

	max =[entity_classes_i count];
	i = 0;
	while (max--) {
		object =[entity_classes_i objectAtIndex:i];
		[matrix addRow];
		cell =[matrix cellAtRow: i++ column:0];
		[cell setStringValue: [NSString stringWithCString:[object
		 classname]]];
		[cell setLeaf:YES];
		[cell setLoaded:YES];
	}
}

@end
