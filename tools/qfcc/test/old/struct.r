struct _qfile_t {};
typedef struct _qfile_t *QFile;

@extern void Qclose (QFile file);
void (QFile file) Qclose = #0;

struct plitem_s {integer dummy;};
typedef struct plitem_s plitem_t;

@extern plitem_t PL_GetFromFile (QFile file);
@extern plitem_t PL_GetPropertyList (string str);

plitem_t PL_GetFromFile (QFile file) = #0;

typedef enum {
	NO = 0,     ///< the false value
	YES         ///< a true value
} BOOL;

@class Protocol;

@protocol Object
- (BOOL) conformsToProtocol: (Protocol *)aProtocol;
@end

@interface Object //<Object>
{
	Class isa;
	integer referenceCount;
}
- (BOOL) conformsToProtocol: (Protocol *)aProtocol;
@end

@class Array;

@interface AutoreleasePool: Object
{
	Array   *array;             ///< a list of objects awaiting release
}
+ (void) addObject: (id)anObject;
@end

@interface Protocol : Object
{
@private
	string protocol_name;
	struct obj_protocol_list *protocol_list;
	struct obj_method_description_list *instance_methods, *class_methods;
}
@end

void test_plist (void)
{
	plitem_t pl;

	pl = PL_GetPropertyList ("{}");
}
