
extern	id	clipper_i;

@interface Clipper : Object
{
	int			num;
	vec3_t		pos[3];
	plane_t		plane;
}

- (BOOL)hide;
- XYClick: (NSPoint)pt;
- (BOOL)XYDrag: (NSPoint *)pt;
- ZClick: (NSPoint)pt;
- carve;
- flipNormal;
- (BOOL)getFace: (face_t *)pl;

- cameraDrawSelf;
- XYDrawSelf;
- ZDrawSelf;

@end

