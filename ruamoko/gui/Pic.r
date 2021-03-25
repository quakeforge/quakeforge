#include <draw.h>
#include <string.h>
#include <gui/Pic.h>

@implementation Pic
-(id)init
{
	pic_name = str_new ();
	return self;
}

-(void)dealloc
{
	str_free (pic_name);
	[super dealloc];
}

-(void)setPic:(string)pic
{
	str_copy (pic_name, pic);
}

-(void)draw
{
	Draw_Pic (xabs, yabs, Draw_CachePic (pic_name, 1));
}
@end

@implementation CenterPic
-(void)draw
{
	Draw_CenterPic (xabs, yabs, Draw_CachePic (pic_name, 1));
}
@end
