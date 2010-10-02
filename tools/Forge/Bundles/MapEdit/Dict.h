#ifndef Dict_h
#define Dict_h

#include <AppKit/AppKit.h>

#include "Storage.h"

typedef  struct {
	char    *key;
	char    *value;
} dict_t;

struct script_s;

@interface Dict: Storage
{
}

- (id) initFromFile: (FILE *)fp;

- (id) parseMultipleFrom: (const char *)value;
- (int) getValueUnits: (const char *)key;
- (id) delString: (const char *)string fromValue: (const char *)key;

- (id) addString: (const char *)string toValue: (const char *)key;

- (char *) convertListToString: (id)list;
- (const char *) getStringFor: (const char *)name;
- (id) removeKeyword: (const char *)key;
- (unsigned int) getValueFor: (const char *)name;
- (id) changeStringFor: (const char *)key to: (const char *)value;

- (dict_t *) findKeyword: (const char *)key;

- (id) writeBlockTo: (FILE *)fp;
- (id) writeFile: (const char *)path;

// INTERNAL
- (id) init;
- (id) parseBraceBlock: (struct script_s *)script;

@end

int GetNextChar (FILE * fp);
void CopyUntilWhitespc (FILE * fp, char *buffer);
void CopyUntilQuote (FILE * fp, char *buffer);
int FindBrace (FILE * fp);
int FindQuote (FILE * fp);
int FindWhitespc (FILE * fp);
int FindNonwhitespc (FILE * fp);

char *FindWhitespcInBuffer (char *buffer);
char *FindNonwhitespcInBuffer (char *buffer);

#endif // Dict_h
