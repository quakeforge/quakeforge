#ifndef __ruamoko_gui_Size_h
#define __ruamoko_gui_Size_h

/**	\addtogroup gui */
///@{

struct Size {
	int width;
	int height;
};

typedef struct Size Size;

@extern Size makeSize (int width, int height);
@extern Size addSize (Size a, Size b);
@extern Size subtractSize (Size a, Size b);

///@}

#endif //__ruamoko_gui_Size_h
