#include "editor/editbuffer.h"

@implementation EditBuffer
- init = #0;
- initWithFile: (string) filename = #0;
- (void) dealloc = #0;
- (unsigned) nextChar: (unsigned) charPtr = #0;
- (unsigned) prevChar: (unsigned) charPtr = #0;
- (unsigned) nextNonSpace: (unsigned) charPtr = #0;
- (unsigned) prevNonSpace: (unsigned) charPtr = #0;
- (unsigned) nextWord: (unsigned) wordPtr = #0;
- (unsigned) prevWord: (unsigned) wordPtr = #0;
- (unsigned) nextLine: (unsigned) linePtr = #0;
- (unsigned) prevLine: (unsigned) linePtr = #0;
- (unsigned) nextLine: (unsigned) linePtr : (unsigned) count = #0;
- (unsigned) prevLine: (unsigned) linePtr : (unsigned) count = #0;

- (unsigned) charPos: (unsigned) linePtr
				  at: (unsigned) target = #0;
- (unsigned) charPtr: (unsigned) linePtr
				  at: (unsigned) target = #0;

- (eb_sel_t) getWord: (unsigned) wordPtr = #0;
- (eb_sel_t) getLine: (unsigned) linePtr = #0;
- (unsigned) getBOL: (unsigned) linePtr = #0;
- (unsigned) getEOL: (unsigned) linePtr = #0;
- (unsigned) getBOT = #0;
- (unsigned) getEOT = #0;
- (string) readString: (eb_sel_t) selection = #0;

- (unsigned) countLines: (eb_sel_t) selection = #0;
- (eb_sel_t) search: (eb_sel_t) selection
				for: (string) str
		  direction: (int) dir = #0;
- (eb_sel_t) isearch: (eb_sel_t) selection
				 for: (string) str
		   direction: (int) dir = #0;
- (unsigned) formatLine: (unsigned) linePtr
				   from: (unsigned) xpos
				   into: (int *) buf
				  width: (unsigned) length
			  highlight: (eb_sel_t) selection
				 colors: (eb_color_t) colors = #0;

- (BOOL) modified = #0;
- (unsigned) textSize = #0;
- (int) saveFile: (string) fileName = #0;
@end
