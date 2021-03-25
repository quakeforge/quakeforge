#ifndef __ruamoko_Protocol_h
#define __ruamoko_Protocol_h

#include <Object.h>

struct obj_method_description {
	string name;
	string types;
};

@interface Protocol : Object
{
@private
	string protocol_name;
	struct obj_protocol_list *protocol_list;
	struct obj_method_description_list *instance_methods, *class_methods;
}

- (string) name;
- (BOOL) conformsTo: (Protocol *)aProtocolObject;
- (struct obj_method_description *) descriptionForInstanceMethod: (SEL)aSel;
- (struct obj_method_description *) descriptionForClassMethod: (SEL)aSel;
@end

#endif//__ruamoko_Protocol_h
