#include <Protocol.h>

struct obj_protocol_list {
	struct obj_protocol_list *next;
	int count;
	Protocol *list[1];
};

struct obj_method_description_list {
	int count;
	struct obj_method_description list[1];
};

@implementation Protocol
- (string) name
{
	return protocol_name;
}

- (BOOL) conformsTo: (Protocol *)aProtocolObject
{
	local int i;
	local struct obj_protocol_list *proto_list;

	if (aProtocolObject.protocol_name == protocol_name)
		return YES;

	for (proto_list = protocol_list; proto_list;
		 proto_list = proto_list.next) {
		for (i = 0; i < proto_list.count; i++) {
			if ([proto_list.list[i] conformsTo: aProtocolObject])
				return YES;
		}
	}
	return NO;
}

- (struct obj_method_description *) descriptionForInstanceMethod: (SEL)aSel
{
	local int i;
	local struct obj_protocol_list *proto_list;
	local string name = sel_get_name (aSel);
	local struct obj_method_description *result;

	for (i = 0; i < instance_methods.count; i++) {
		if (instance_methods.list[i].name == name)
			return &instance_methods.list[i];
	}

	for (proto_list = protocol_list; proto_list;
		 proto_list = proto_list.next) {
		for (i = 0; i < proto_list.count; i++) {
			if ((result = [proto_list.list[i]
						   descriptionForInstanceMethod: aSel]))
				return result;
		}
	}
	return nil;
}

- (struct obj_method_description *) descriptionForClassMethod: (SEL)aSel
{
	local int i;
	local struct obj_protocol_list *proto_list;
	local string name = sel_get_name (aSel);
	local struct obj_method_description *result;

	for (i = 0; i < class_methods.count; i++) {
		if (class_methods.list[i].name == name)
			return &class_methods.list[i];
	}

	for (proto_list = protocol_list; proto_list;
		 proto_list = proto_list.next) {
		for (i = 0; i < proto_list.count; i++) {
			if ((result = [proto_list.list[i]
						   descriptionForClassMethod: aSel]))
				return result;
		}
	}
	return nil;
}

@end
