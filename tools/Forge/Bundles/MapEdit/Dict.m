#include "QF/dstring.h"
#include "QF/qfplist.h"
#include "QF/script.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "Dict.h"

@implementation Dict

/*
- (id) print
{
	NSUInteger  i;
	dict_t      *d;

	for (i = 0; i < numElements; i++) {
		d = [self elementAt: i];
		printf ("%s : %s\n", d->key, d->value);
	}
	return self;
}
*/
/*
===========
copyFromZone

JDC
===========
*/
- (id) copy
{
	Sys_Printf ("Dict copy: not implemented\n");
	return 0;
}

- (id) initFromFile: (FILE *)fp
{
	dstring_t   *text = dstring_newstr ();
	char        *str;
	size_t      read;
	const size_t readsize = 1024;

	[self init];

	do {
		str = dstring_reservestr (text, readsize);
		read = fread (str, 1, readsize, fp);
		if (read)
			str[read] = 0;
	} while (read == readsize);


	plist = PL_GetPropertyList (text->str);
	dstring_delete (text);
	if (!plist)
		return 0;
	return self;
}

- (void) dealloc
{
	if (plist)
		PL_Free (plist);
	[super dealloc];
}

// ===============================================
//
//  Dictionary pair functions
//
// ===============================================

//
//  Write a { } block out to a FILE*
//
- (id) writeBlockTo: (FILE *)fp
{
	char   *data;

	data = PL_WritePropertyList (plist);
	fputs (data, fp);
	free (data);

	return self;
}

//
//  Write a single { } block out
//
- (id) writeFile: (const char *)path
{
	FILE  *fp;

	fp = fopen (path, "w+t");
	if (fp != NULL) {
		printf ("Writing dictionary file %s.\n", path);
		fprintf (fp, "// QE_Project file %s\n", path);
		[self writeBlockTo: fp];
		fclose (fp);
	} else {
		printf ("Error writing %s!\n", path);
		return NULL;
	}

	return self;
}

// ===============================================
//
//  Utility methods
//
// ===============================================

//
//  Change a keyword's string
//
- (id) changeStringFor: (const char *)key to: (const char *)value
{
	PL_D_AddObject (plist, key, PL_NewString (value));
	return self;
}

- (plitem_t *) getArrayFor: (const char *)name
{
	plitem_t    *item;
	item = PL_ObjectForKey (plist, name);
	if (item && PL_Type (item) == QFArray)
		return item;
	return 0;
}

//
//  Search for keyword, return the string *
//
- (const char *) getStringFor: (const char *)name
{
	plitem_t    *item;
	const char  *str;

	item = PL_ObjectForKey (plist, name);
	if (item && (str = PL_String (item)))
		return str;
	return "";
}

//
//  Search for keyword, return the value
//
- (unsigned int) getValueFor: (const char *)name
{
	return atol ([self getStringFor: name]);
}

//
//  Return # of units in keyword's value
//
- (int) getValueUnits: (const char *)key
{
	plitem_t    *item;

	item = PL_ObjectForKey (plist, key);
	if (!item || PL_Type (item) != QFArray)
		return 0;

	return PL_A_NumObjects (item);
}

@end
