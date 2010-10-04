#ifndef Dict_h
#define Dict_h

#include <AppKit/AppKit.h>

#include "Storage.h"

struct script_s;

@interface Dict: NSObject
{
	struct plitem_s *plist;
}

- (id) initFromFile: (FILE *)fp;

- (int) getValueUnits: (const char *)key;

- (struct plitem_s *) getArrayFor: (const char *)name;
- (const char *) getStringFor: (const char *)name;
- (unsigned int) getValueFor: (const char *)name;
- (id) changeStringFor: (const char *)key to: (const char *)value;

- (id) writeBlockTo: (FILE *)fp;
- (id) writeFile: (const char *)path;

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
