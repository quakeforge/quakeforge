#include "QF/dstring.h"
#include "QF/script.h"
#include "QF/va.h"

#include "Dict.h"

@implementation Dict

- (id) init
{
	[super initCount: 0 elementSize: sizeof (dict_t)
	     description: NULL];
	return self;
}

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

/*
===========
copyFromZone

JDC
===========
*/
- (id) copy
{
	id          new;
	NSUInteger  i;
	dict_t      *d;
	char        *old;

	new = [super copy];
	for (i = 0; i < numElements; i++) {
		d = [self elementAt: i];
		old = d->key;
		d->key = malloc (strlen (old) + 1);
		strcpy (d->key, old);

		old = d->value;
		d->value = malloc (strlen (old) + 1);
		strcpy (d->value, old);
	}

	return new;
}

- (id) initFromFile: (FILE *)fp
{
	dstring_t   *text = dstring_newstr ();
	char        *str;
	size_t      read;
	const size_t readsize = 1024;
	script_t    *script;

	[self init];

	do {
		str = dstring_reservestr (text, readsize);
		read = fread (str, 1, readsize, fp);
		if (read)
			str[read] = 0;
	} while (read == readsize);

	script = Script_New ();
	Script_Start (script, "project file", text->str);

	self = [self parseBraceBlock: script];
	Script_Delete (script);
	return self;
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
	int     max;
	int     i;
	dict_t  *d;

	fprintf (fp, "{\n");
	max = [super count];
	for (i = 0; i < max; i++) {
		d = [super elementAt: i];
		fprintf (fp, "\t{\"%s\"\t\"%s\"}\n", d->key, d->value);
	}
	fprintf (fp, "}\n");

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
//  Find a keyword in storage
//  Returns * to dict_t, otherwise NULL
//
- (dict_t *) findKeyword: (const char *)key
{
	int     max;
	int     i;
	dict_t  *d;

	max = [super count];
	for (i = 0; i < max; i++) {
		d = [super elementAt: i];
		if (!strcmp (d->key, key))
			return d;
	}

	return NULL;
}

//
//  Change a keyword's string
//
- (id) changeStringFor: (const char *)key to: (const char *)value
{
	dict_t  *d;
	dict_t  newd;

	d = [self findKeyword: key];
	if (d != NULL) {
		free (d->value);
		d->value = malloc (strlen (value) + 1);
		strcpy (d->value, value);
	} else {
		newd.key = malloc (strlen (key) + 1);
		strcpy (newd.key, key);
		newd.value = malloc (strlen (value) + 1);
		strcpy (newd.value, value);
		[self addElement: &newd];
	}
	return self;
}

//
//  Search for keyword, return the string *
//
- (const char *) getStringFor: (const char *)name
{
	dict_t  *d;

	d = [self findKeyword: name];
	if (d != NULL)
		return d->value;
	return (char *) "";
}

//
//  Search for keyword, return the value
//
- (unsigned int) getValueFor: (const char *)name
{
	dict_t  *d;

	d = [self findKeyword: name];
	if (d != NULL)
		return atol (d->value);
	return 0;
}

//
//  Return # of units in keyword's value
//
- (int) getValueUnits: (const char *)key
{
	id      temp;
	int     count;

	temp = [self parseMultipleFrom: key];
	count = [temp count];
	[temp release];

	return count;
}

//
//  Convert List to string
//
- (char *) convertListToString: (id)list
{
	int         i;
	int         max;
	dstring_t   *tempstr;
	char        *s;

	max = [list count];
	tempstr = dstring_newstr ();
	for (i = 0; i < max; i++) {
		s = [list elementAt: i];
		dstring_appendstr (tempstr, s);
		dstring_appendstr (tempstr, "  ");
	}

	return dstring_freeze (tempstr);
}

//
// JDC: I wrote this to simplify removing vectors
//
- (id) removeKeyword: (const char *)key
{
	dict_t  *d;

	d = [self findKeyword: key];
	if (d == NULL)
		return self;
	[self removeElementAt: d - (dict_t *) dataPtr];
	return self;
}

//
//  Delete string from keyword's value
//
- (id) delString: (const char *)string fromValue: (const char *)key
{
	id      temp;
	int     count;
	int     i;
	char    *s;
	dict_t  *d;

	d = [self findKeyword: key];
	if (d == NULL)
		return NULL;
	temp = [self parseMultipleFrom: key];
	count = [temp count];
	for (i = 0; i < count; i++) {
		s = [temp elementAt: i];
		if (!strcmp (s, string)) {
			[temp removeElementAt: i];
			free (d->value);
			d->value = [self convertListToString: temp];
			[temp release];

			break;
		}
	}
	return self;
}

//
//  Add string to keyword's value
//
- (id) addString: (const char *)string toValue: (const char *)key
{
	char    *newstr;
	dict_t  *d;

	d = [self findKeyword: key];
	if (d == NULL)
		return NULL;
	newstr = nva ("%s\t%s", d->value, string);
	free (d->value);
	d->value = newstr;

	return self;
}

// ===============================================
//
//  Use these for multiple parameters in a keyword value
//
// ===============================================
const char  *searchStr;
char        item[4096];

- (id) setupMultiple: (const char *)value
{
	searchStr = value;
	return self;
}

- (char *) getNextParameter
{
	char  *s;

	if (!searchStr)
		return NULL;
	strcpy (item, searchStr);
	s = FindWhitespcInBuffer (item);
	if (!*s) {
		searchStr = NULL;
	} else {
		*s = 0;
		searchStr = FindNonwhitespcInBuffer (s + 1);
	}
	return item;
}

//
//  Parses a keyvalue string & returns a Storage full of those items
//
- (id) parseMultipleFrom: (const char *)key
{
#define ITEMSIZE 128
	id          stuff;
	char        string[ITEMSIZE];
	const char  *s;

	s = [self getStringFor: key];
	if (s == NULL)
		return NULL;
	stuff = [[Storage alloc]
	         initCount: 0 elementSize: ITEMSIZE description: NULL];

	[self setupMultiple: s];
	while ((s = [self getNextParameter])) {
		bzero (string, ITEMSIZE);
		strcpy (string, s);
		[stuff addElement: string];
	}

	return stuff;
}

// ===============================================
//
//  Dictionary pair parsing
//
// ===============================================

//
//  parse all keyword/value pairs within { } 's
//
- (id) parseBraceBlock: (script_t *)script
{
	dict_t  pair;

	if (!Script_GetToken (script, YES))
		return NULL;
	if (strcmp (Script_Token (script), "{"))
		return NULL;
	do {
		if (!Script_GetToken (script, YES))
			return NULL;
		if (!strcmp (Script_Token (script), "}"))
			break;
		if (strcmp (Script_Token (script), "{"))
			return NULL;

		if (!Script_GetToken (script, YES))
			return NULL;
		pair.key = strdup (Script_Token (script));

		if (!Script_GetToken (script, YES))
			return NULL;
		pair.value = strdup (Script_Token (script));

		if (!Script_GetToken (script, YES))
			return NULL;
		if (strcmp (Script_Token (script), "}"))
			return NULL;
		[super addElement: &pair];
	} while (1);

	return self;
}

@end

// ===============================================
//
//  C routines for string parsing
//
// ===============================================
int
GetNextChar (FILE * fp)
{
	int     c;
	int     c2;

	c = getc (fp);
	if (c == EOF)
		return -1;

	if (c == '/') { // parse comments
		c2 = getc (fp);
		if (c2 == '/') {
			while ((c2 = getc (fp)) != '\n')
				;

			c = getc (fp);
		} else {
			ungetc (c2, fp);
		}
	}
	return c;
}

void
CopyUntilWhitespc (FILE * fp, char *buffer)
{
	int     count = 800;
	int     c;

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
	int     count = 800;
	int     c;

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
	int     count = 800;
	int     c;

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
	int     count = 800;
	int     c;

	while (count--) {
		c = GetNextChar (fp);

		if (c == EOF)
			return -1;

		if (c == '\"')
			return c;
	}
	return -1;
}

char *
FindWhitespcInBuffer (char *buffer)
{
	int     count = 1000;
	char    *b = buffer;

	while (count--) {
		if (*b <= ' ')
			return b;
		else
			b++;
	}
	return NULL;
}

char *
FindNonwhitespcInBuffer (char *buffer)
{
	int     count = 1000;
	char    *b = buffer;

	while (count--) {
		if (*b > ' ')
			return b;
		else
			b++;
	}
	return NULL;
}
