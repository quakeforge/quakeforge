#include "QF/dstring.h"
#include "QF/va.h"

#include "Dict.h"

@implementation Dict

-init
{
	[super initCount: 0 elementSize:sizeof (dict_t)
	description:NULL];
	return self;
}

-print
{
	int         i;
	dict_t     *d;

	for (i = 0; i < numElements; i++) {
		d =[self elementAt:i];
		printf ("%s : %s\n", d->key, d->value);
	}
	return self;
}

/*
===========
copyFromZone

JDC
===========
*/
-copy
{
	id          new;
	int         i;
	dict_t     *d;
	char       *old;

	new =[super copy];
	for (i = 0; i < numElements; i++) {
		d =[self elementAt:i];
		old = d->key;
		d->key = malloc (strlen (old) + 1);
		strcpy (d->key, old);

		old = d->value;
		d->value = malloc (strlen (old) + 1);
		strcpy (d->value, old);
	}

	return new;
}

-initFromFile:(FILE *) fp
{
	[self init];
	return[self parseBraceBlock:fp];
}

//===============================================
//
//  Dictionary pair functions
//
//===============================================

//
//  Write a { } block out to a FILE*
//
-writeBlockTo:(FILE *) fp
{
	int         max;
	int         i;
	dict_t     *d;

	fprintf (fp, "{\n");
	max =[super count];
	for (i = 0; i < max; i++) {
		d =[super elementAt:i];
		fprintf (fp, "\t{\"%s\"\t\"%s\"}\n", d->key, d->value);
	}
	fprintf (fp, "}\n");

	return self;
}

//
//  Write a single { } block out
//
-writeFile:(const char *) path
{
	FILE       *fp;

	fp = fopen (path, "w+t");
	if (fp != NULL) {
		printf ("Writing dictionary file %s.\n", path);
		fprintf (fp, "// QE_Project file %s\n", path);
		[self writeBlockTo:fp];
		fclose (fp);
	} else {
		printf ("Error writing %s!\n", path);
		return NULL;
	}

	return self;
}

//===============================================
//
//  Utility methods
//
//===============================================

//
//  Find a keyword in storage
//  Returns * to dict_t, otherwise NULL
//
-(dict_t *) findKeyword:(const char *) key
{
	int         max;
	int         i;
	dict_t     *d;

	max =[super count];
	for (i = 0; i < max; i++) {
		d =[super elementAt:i];
		if (!strcmp (d->key, key))
			return d;
	}

	return NULL;
}

//
//  Change a keyword's string
//
-changeStringFor:(const char *) key to:(const char *) value
{
	dict_t     *d;
	dict_t      newd;

	d =[self findKeyword:key];
	if (d != NULL) {
		free (d->value);
		d->value = malloc (strlen (value) + 1);
		strcpy (d->value, value);
	} else {
		newd.key = malloc (strlen (key) + 1);
		strcpy (newd.key, key);
		newd.value = malloc (strlen (value) + 1);
		strcpy (newd.value, value);
		[self addElement:&newd];
	}
	return self;
}

//
//  Search for keyword, return the string *
//
-(const char *) getStringFor:(const char *) name
{
	dict_t     *d;

	d =[self findKeyword:name];
	if (d != NULL)
		return d->value;

	return "";
}

//
//  Search for keyword, return the value
//
-(unsigned int) getValueFor:(const char *) name
{
	dict_t     *d;

	d =[self findKeyword:name];
	if (d != NULL)
		return atol (d->value);

	return 0;
}

//
//  Return # of units in keyword's value
//
-(int) getValueUnits:(const char *) key
{
	id          temp;
	int         count;

	temp =[self parseMultipleFrom:key];
	count =[temp count];
	[temp release];

	return count;
}

//
//  Convert List to string
//
-(char *) convertListToString:(id) list
{
	int         i;
	int         max;
	dstring_t  *tempstr;
	char       *s;

	max =[list count];
	tempstr = dstring_newstr ();
	for (i = 0; i < max; i++) {
		s =[list elementAt:i];
		dstring_appendstr (tempstr, s);
		dstring_appendstr (tempstr, "  ");
	}

	return dstring_freeze (tempstr);
}

//
// JDC: I wrote this to simplify removing vectors
//
-removeKeyword:(const char *) key
{
	dict_t     *d;

	d =[self findKeyword:key];
	if (d == NULL)
		return self;
	[self removeElementAt:d - (dict_t *) dataPtr];
	return self;
}

//
//  Delete string from keyword's value
//
-delString:(const char *) string fromValue:(const char *) key
{
	id          temp;
	int         count;
	int         i;
	char       *s;
	dict_t     *d;

	d =[self findKeyword:key];
	if (d == NULL)
		return NULL;
	temp =[self parseMultipleFrom:key];
	count =[temp count];
	for (i = 0; i < count; i++) {
		s =[temp elementAt:i];
		if (!strcmp (s, string)) {
			[temp removeElementAt:i];
			free (d->value);
			d->value =[self convertListToString:temp];
			[temp release];

			break;
		}
	}
	return self;
}

//
//  Add string to keyword's value
//
-addString:(const char *) string toValue:(const char *) key
{
	char       *newstr;
	dict_t     *d;

	d =[self findKeyword:key];
	if (d == NULL)
		return NULL;
	newstr = nva ("%s\t%s", d->value, string);
	free (d->value);
	d->value = newstr;

	return self;
}

//===============================================
//
//  Use these for multiple parameters in a keyword value
//
//===============================================
const char *searchStr;
char        item[4096];

-setupMultiple:(const char *) value
{
	searchStr = value;
	return self;
}

-(char *) getNextParameter
{
	char       *s;

	if (!searchStr)
		return NULL;
	strcpy (item, searchStr);
	s = FindWhitespcInBuffer (item);
	if (!*s)
		searchStr = NULL;
	else {
		*s = 0;
		searchStr = FindNonwhitespcInBuffer (s + 1);
	}
	return item;
}

//
//  Parses a keyvalue string & returns a Storage full of those items
//
-(id) parseMultipleFrom:(const char *) key
{
#define	ITEMSIZE	128
	id          stuff;
	char        string[ITEMSIZE];
	const char *s;

	s =[self getStringFor:key];
	if (s == NULL)
		return NULL;

	stuff =[[Storage alloc]
	initCount: 0 elementSize: ITEMSIZE description:NULL];

	[self setupMultiple:s];
	while ((s =[self getNextParameter])) {
		bzero (string, ITEMSIZE);
		strcpy (string, s);
		[stuff addElement:string];
	}

	return stuff;
}

//===============================================
//
//  Dictionary pair parsing
//
//===============================================

//
//  parse all keyword/value pairs within { } 's
//
-(id) parseBraceBlock:(FILE *) fp
{
	int         c;
	dict_t      pair;
	char        string[1024];

	c = FindBrace (fp);
	if (c == -1)
		return NULL;

	while ((c = FindBrace (fp)) != '}') {
		if (c == -1)
			return NULL;
//      c = FindNonwhitespc(fp);
//      if (c == -1)
//          return NULL;
//      CopyUntilWhitespc(fp,string);

// JDC: fixed to allow quoted keys
		c = FindNonwhitespc (fp);
		if (c == -1)
			return NULL;
		c = fgetc (fp);
		if (c == '\"')
			CopyUntilQuote (fp, string);
		else {
			ungetc (c, fp);
			CopyUntilWhitespc (fp, string);
		}

		pair.key = malloc (strlen (string) + 1);
		strcpy (pair.key, string);

		c = FindQuote (fp);
		CopyUntilQuote (fp, string);
		pair.value = malloc (strlen (string) + 1);
		strcpy (pair.value, string);

		[super addElement:&pair];
		c = FindBrace (fp);
	}

	return self;
}

@end
//===============================================
//
//  C routines for string parsing
//
//===============================================
	int
GetNextChar (FILE * fp)
{
	int         c;
	int         c2;

	c = getc (fp);
	if (c == EOF)
		return -1;
	if (c == '/')						// parse comments
	{
		c2 = getc (fp);
		if (c2 == '/') {
			while ((c2 = getc (fp)) != '\n');
			c = getc (fp);
		} else
			ungetc (c2, fp);
	}
	return c;
}

void
CopyUntilWhitespc (FILE * fp, char *buffer)
{
	int         count = 800;
	int         c;

	while (count--) {
		c = GetNextChar (fp);
		if (c == EOF)
			return;
		if (c <= ' ') {
			*buffer = 0;
			return;
		}
		*buffer++ = c;
	}
}

void
CopyUntilQuote (FILE * fp, char *buffer)
{
	int         count = 800;
	int         c;

	while (count--) {
		c = GetNextChar (fp);
		if (c == EOF)
			return;
		if (c == '\"') {
			*buffer = 0;
			return;
		}
		*buffer++ = c;
	}
}

int
FindBrace (FILE * fp)
{
	int         count = 800;
	int         c;

	while (count--) {
		c = GetNextChar (fp);
		if (c == EOF)
			return -1;
		if (c == '{' || c == '}')
			return c;
	}
	return -1;
}

int
FindQuote (FILE * fp)
{
	int         count = 800;
	int         c;

	while (count--) {
		c = GetNextChar (fp);
		if (c == EOF)
			return -1;
		if (c == '\"')
			return c;
	}
	return -1;
}

int
FindWhitespc (FILE * fp)
{
	int         count = 800;
	int         c;

	while (count--) {
		c = GetNextChar (fp);
		if (c == EOF)
			return -1;
		if (c <= ' ') {
			ungetc (c, fp);
			return c;
		}
	}
	return -1;
}

int
FindNonwhitespc (FILE * fp)
{
	int         count = 800;
	int         c;

	while (count--) {
		c = GetNextChar (fp);
		if (c == EOF)
			return -1;
		if (c > ' ') {
			ungetc (c, fp);
			return c;
		}
	}
	return -1;
}

char       *
FindWhitespcInBuffer (char *buffer)
{
	int         count = 1000;
	char       *b = buffer;

	while (count--)
		if (*b <= ' ')
			return b;
		else
			b++;
	return NULL;
}

char       *
FindNonwhitespcInBuffer (char *buffer)
{
	int         count = 1000;
	char       *b = buffer;

	while (count--)
		if (*b > ' ')
			return b;
		else
			b++;
	return NULL;
}
