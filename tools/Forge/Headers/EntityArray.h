#import <Foundation/Foundation.h>
#import "mathlib.h"

extern	id	entity_classes_i;

@interface EntityArray: NSMutableArray
{
	id		nullclass;
	char	*source_path;
}

- initForSourceDirectory: (char *)path;
- (id)classForName: (char *)name;
- (void)scanDirectory;

@end

