#include "Array.h"

@implementation Array

- (id) init
{
	count = size = 0;
	incr = 16;
	array = NIL;
	return self;
}

- (id) initWithIncrement: (integer) inc
{
	count = 0;
	size = incr = inc;
	array = (void[][]) obj_malloc (inc * @sizeof (void []));
	return self;
}

- (void) free
{
	local integer i;
	for (i = 0; i < count; i++)
		obj_free (array[i]);
	obj_free (array);
}

- (void []) getItemAt: (integer) index
{
	if (index == -1)
		index = count - 1;
	if (index < 0 || index >= count)
		return NIL;
	return array[index];
}

- (void) setItemAt: (integer) index item: (void []) item
{
	if (index == -1)
		index = count - 1;
	if (index < 0 || index >= count)
		return;
	array[index] = item;
}

- (void) addItem: (void []) item
{
	if (count == size) {
		size += incr;
		array = (void[][])obj_realloc (array, size * @sizeof (void []));
	}
	array[count++] = item;
}

- (void) removeItem: (void []) item
{
	local integer i, n;

	for (i = 0; i < count; i++)
		if (array[i] == item) {
			count--;
			for (n = i; n < count; n++)
				array[n] = array[n + 1];
		}
	return;
}

- (void []) removeItemAt: (integer) index
{
	local integer i;
	local void [] item;

	if (index == -1)
		index = count -1;
	if (index < 0 || index >= count)
		return NIL;
	item = array[index];
	count--;
	for (i = index; i < count; i++)
		array[i] = array[i + 1];
	return item;
}

- (void []) insertItemAt: (integer) index item:(void []) item
{
	local integer i;
	if (index == -1)
		index = count -1;
	if (index < 0 || index >= count)
		return NIL;
	if (count == size) {
		size += incr;
		array = (void[][])obj_realloc (array, size * @sizeof (void []));
	}
	for (i = count; i > index; i--)
		array[i] = array[i - 1];
	array[index] = item;
	count++;
	return item;
}

- (integer) count
{
	return count;
}

@end
