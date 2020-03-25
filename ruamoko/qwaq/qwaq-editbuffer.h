#ifndef __qwaq_editbuffer_h
#define __qwaq_editbuffer_h

#ifdef __QFCC__
#include <Object.h>

//FIXME add unsigned to qfcc
#ifndef unsigned
#define unsigned int
#define umax 0x7fffffff
#endif

#endif//__QFCC__

typedef struct eb_sel_s {
	unsigned    start;
	unsigned    length;
} eb_sel_t;

typedef struct eb_color_s {
	int         normal;
	int         selected;
} eb_color_t;

#ifdef __QFCC__
@interface EditBuffer : Object
{
	struct edit_buffer_s *buffer;
}
-init;
-initWithFile: (string) filename;
- (unsigned) nextChar: (unsigned) charPtr;
- (unsigned) prevChar: (unsigned) charPtr;
- (unsigned) nextNonSpace: (unsigned) charPtr;
- (unsigned) prevNonSpace: (unsigned) charPtr;
- (unsigned) nextWord: (unsigned) wordPtr;
- (unsigned) prevWord: (unsigned) wordPtr;
- (unsigned) nextLine: (unsigned) linePtr;
- (unsigned) prevLine: (unsigned) linePtr;
- (unsigned) nextLine: (unsigned) linePtr : (unsigned) count;
- (unsigned) prevLine: (unsigned) linePtr : (unsigned) count;

- (unsigned) charPos: (unsigned) linePtr at: (unsigned) target;
- (unsigned) charPtr: (unsigned) linePtr at: (unsigned) target;

- (eb_sel_t) getWord: (unsigned) wordPtr;
- (eb_sel_t) getLine: (unsigned) linePtr;
- (unsigned) getBOL: (unsigned) linePtr;
- (unsigned) getEOL: (unsigned) linePtr;
- (unsigned) getBOT;
- (unsigned) getEOT;
- (string) readString: (eb_sel_t) selection;

- (unsigned) countLines: (eb_sel_t) selection;
- (eb_sel_t) search: (eb_sel_t) selection
				for: (string) str
		  direction: (int) dir;
- (eb_sel_t) isearch: (eb_sel_t) selection
				 for: (string) str
		   direction: (int) dir;
- (unsigned) formatLine: (unsigned) linePtr
				   from: (unsigned) xpos
				   into: (int *) buf
				  width: (unsigned) length
			  highlight: (eb_sel_t) selection
				 colors: (eb_color_t) colors;

- (BOOL) modified;
- (unsigned) textSize;
- (int) saveFile: (string) fileName;
@end
#else//__QFCC__

#include "QF/pr_obj.h"

typedef struct qwaq_editbuffer_s {
	pr_id_t     isa;
	pointer_t   buffer;
} qwaq_editbuffer_t;

#endif//!__QFCC__

#endif//__qwaq_editbuffer_h
