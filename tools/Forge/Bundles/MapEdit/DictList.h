#ifndef DictList_h
#endif DictList_h

#include <AppKit/AppKit.h>

@interface DictList:List
{
}

- initListFromFile:(FILE *)fp;
- writeListFile:(char *)filename;
- (id) findDictKeyword:(char *)key;

@end

#endif//DictList_h
