#ifndef __ruamoko_gui_Size_h
#define __ruamoko_gui_Size_h

struct Size {
	integer width;
	integer height;
};

typedef struct Size Size;

@extern Size makeSize (integer width, integer height);
@extern Size addSize (Size a, Size b);
@extern Size subtractSize (Size a, Size b);

#endif //__ruamoko_gui_Size_h
