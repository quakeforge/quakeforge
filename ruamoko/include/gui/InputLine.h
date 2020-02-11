#ifndef __ruamoko_gui_InputLine_h
#define __ruamoko_gui_InputLine_h

#include "View.h"

/**	\defgroup inputline Low level intputline interface.
	\ingroup gui

	Interface functions to the engine implementation.
*/
///@{

/**	Opaque handle to an inputline.

	\warning	Not a real pointer. Dereferencing leads to nasal-dragon
				infested lands.
*/
typedef struct _inputline_t *inputline_t;

/**	Create a new inputline.

	The inputline will be positioned at 0,0 with the cursor enabled.

	\param	lines	The number of lines of input history.
	\param	size	The maximum length of the input string.
	\param	prompt	The prompt to display.
	\return			The inputline handle.
*/
@extern inputline_t InputLine_Create (int lines, int size, int prompt);

/**	Set the visual location of the input line.

	The coordinates are defined by the display system (pixels for clients,
	character cells for servers).

	\param	il		The inputline handle.
	\param	x		The X coordinate of the upper-left corner of the inputline.
	\param	y		The Y coordinate of the upper-left corner of the inputline.
*/
@extern void InputLine_SetPos (inputline_t il, int x, int y);

/**	Turn the inputline's cursor on or off.

	\param	il		The inputline handle.
	\param	cursor	0 turns off the cursor, non-0 turns it on.
*/
@extern void InputLine_SetCursor (inputline_t il, int cursor);

typedef void (il_enterfunc)(string, void*);
/**	Set the callback function for when the enter key is pressed.

	\param	il		The inputline handle.
	\param	f		The callback function. The first parameter is the text
					of the input line and the second is \a data.
	\param	data	Pointer to a data block to be passed to the callback
					function.
*/
@extern @overload void InputLine_SetEnter (inputline_t il, il_enterfunc f, void *data);

/**	Set the callback method for when the enter key is pressed.

	The method will be called with a single string parameter representing the
	text of the inputline. eg:

		-enter: (string) text;

	\param	il		The inputline handle.
	\param	imp		The implementation of the method.
	\param	obj		The object receiving the message.
	\param	sel		The selector representing the message.
*/
@extern @overload void InputLine_SetEnter (inputline_t il, IMP imp, id obj, SEL sel);

/**	Set the visible width of the inputline.

	\param	il		The inputline handle.
	\param	width	The width of the inputline in character cells.
*/
@extern void InputLine_SetWidth (inputline_t il, int width);

/**	Destroy an inputline, freeing its resources.

	\param	il		The inputline handle.
*/
@extern void InputLine_Destroy (inputline_t il);

/**	Clear the inputline's text.

	\param	il		The inputline handle.
	\param	save	If true, the current text will be saved to the history.
*/
@extern void InputLine_Clear (inputline_t il, int save);

/**	Process a keystroke.

	\param	il		The inputline handle.
	\param	key		The Quake key code for the key press.
*/
@extern void InputLine_Process (inputline_t il, int key);

/**	Draw the inputline to the screen.

	Drawing is handled by the engine.
	\param	il		The inputline handle.
*/
@extern void InputLine_Draw (inputline_t il);

/**	Set the text of the inputline

	\param	il		The inputline handle.
	\param	str		The text to which the inputline will be set.
*/
@extern void InputLine_SetText (inputline_t il, string str);

/**	Retrieve the text of the inputline.

	\param	il		The inputline handle.
	\return			The current text of the intputline.
*/
@extern string InputLine_GetText (inputline_t il);
///@}

/**	\addtogroup gui */
///@{

/**	Class representation of the low-level inputline objects.
*/
@interface InputLine: View
{
	inputline_t	il;			///< The inputline handle.
}

/**	Initialize.

	\note	The size of the bounds parameter is interpreted differently to
			usual. The width is in character cells and the hight is used
			for the number of lines of history.
	\param	aRect	The bounds of the inputline.
	\param	char	The prompt character.
	\todo the size thing is stupid and broken.
*/
- (id) initWithBounds: (Rect)aRect promptCharacter: (int)char;

/**	Set the visible width of the inputline.

	\param	width	The visible width of the inputline.
	\todo the size thing is stupid and broken.
*/
- (void) setWidth: (int)width;

/**	Set up the XXX for when the enter key is pressed.

	The method will be called with a single string parameter representing the
	text of the inputline. eg:

		-enter: (string) text;

	\param	obj		The object receiving the message.
	\param	msg		The selector representing the message.

	\todo -(void) set[X]Action: (SEL)aSelector;
	\todo -(void) setTarget: (id)ahnObject;
*/
- (void) setEnter: obj message:(SEL) msg;

/**	Turn the inputline's cursor on or off.

	\param	cursor	0 turns off the cursor, non-0 turns it on.
*/
- (void) cursor: (BOOL)cursor;

/**	Process a keystroke.

	\param	key		The Quake key code for the key press.
*/
- (void) processInput: (int)key;

/**	Set the text of the inputline

	\param	text	The text to which the inputline will be set.
*/
- (id) setText: (string)text;

/**	Retrieve the text of the inputline.

	\return			The current text of the intputline.
*/
- (string) text;

@end

@interface InputLineBox: View
{
	InputLine *input_line;
}

/**	Initialize.

	\note	The size of the bounds parameter is interpreted differently to
			usual. The width is in character cells and the hight is used
			for the number of lines of history.
	\param	aRect	The bounds of the inputline.
	\param	char	The prompt character.
	\todo the size thing is stupid and broken.
*/
- (id) initWithBounds: (Rect)aRect promptCharacter: (int)char;

/**	Set the visible width of the inputline.

	\param	width	The visible width of the inputline.
	\todo the size thing is stupid and broken.
*/
- (void) setWidth: (int)width;

/**	Set up the target/action for when the enter key is pressed.

	The method will be called with a single string parameter representing the
	text of the inputline. eg:

		-enter: (string) text;

	\param	obj		The object receiving the message.
	\param	msg		The selector representing the message.

	\todo -(void) set[X]Action: (SEL)aSelector;
	\todo -(void) setTarget: (id)ahnObject;
*/
- (void) setEnter: obj message:(SEL) msg;

/**	Turn the inputline's cursor on or off.

	\param	cursor	0 turns off the cursor, non-0 turns it on.
*/
- (void) cursor: (BOOL)cursor;

/**	Process a keystroke.

	\param	key		The Quake key code for the key press.
*/
- (void) processInput: (int)key;

/**	Set the text of the inputline

	\param	text	The text to which the inputline will be set.
*/
- (id) setText: (string)text;

/**	Retrieve the text of the inputline.

	\return			The current text of the intputline.
*/
- (string) text;
@end

///@}

#endif //__ruamoko_gui_InputLine_h
