#ifndef DictList_h
#define DictList_h

#include <AppKit/AppKit.h>

@interface DictList:NSMutableArray
{
	NSMutableArray *array;
}

-initListFromFile:(FILE *) fp;
-writeListFile:(char *) filename;
-(id) findDictKeyword:(char *) key;

@end
#endif // DictList_h
