
#include "DictList.h"
#include "Dict.h"

@implementation DictList
//
//  Read in variable # of objects from FILE *
//
- initListFromFile:(FILE *) fp
{
	id          d;

	[super init];
	do {
		d =[(Dict *)[Dict alloc] initFromFile:fp];
		if (d != NULL)
			[self addObject:d];
	} while (d != NULL);
	[d release];

	return self;
}

//
//  Write out list file
//
-writeListFile:(char *) filename
{
	FILE       *fp;
	int         i;
	id          obj;

	fp = fopen (filename, "w+t");
	if (fp == NULL)
		return NULL;

	fprintf (fp, "// Object List written by QuakeEd\n");

	for (i = 0; i <[self count]; i++) {
		obj =[self objectAtIndex:i];
		[obj writeBlockTo:fp];
	}
	fclose (fp);

	return self;
}

//
//  Find the keyword in all the Dict objects
//
-(id) findDictKeyword:(char *) key
{
	int         i;
	dict_t     *d;
	id          dict;

	for (i = 0; i <[self count]; i++) {
		dict =[self objectAtIndex:i];
		d =[(Dict *) dict findKeyword:key];
		if (d != NULL)
			return dict;
	}
	return NULL;
}

@end
