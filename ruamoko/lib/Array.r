#include "Array.h"

@implementation Array

- (id) init
{
	self.count = self.size = 0;
	self.incr = 16;
	self.array = NIL;
	return self;
}

- (id) initWithIncrement: (integer) inc
{
	self.count = 0;
	self.size = self.incr = inc;
	self.array = (void[][]) obj_malloc (inc * @sizeof (void []));
	return self;
}

- (void) free
{
	local integer i;
	for (i = 0; i < self.count; i++)
		obj_free (self.array[i]);
	obj_free (self.array);
}

- (void []) getItemAt: (integer) index
{
	if (index == -1)
		index = self.count - 1;
	if (index < 0 || index >= self.count)
		return NIL;
	return self.array[index];
}

- (void) setItemAt: (integer) index item: (void []) item
{
	if (index == -1)
		index = self.count - 1;
	if (index < 0 || index >= self.count)
		return;
	self.array[index] = item;
}

- (void) addItem: (void []) item
{
	if (self.count == self.size) {
		self.size += self.incr;
		self.array = (void[][])obj_realloc (self.array,
											self.size * @sizeof (void []));
	}
	self.array[self.count++] = item;
}

- (void []) removeItemAt: (integer) index
{
	local integer i;
	local void [] item;

	if (index == -1)
		index = self.count -1;
	if (index < 0 || index >= self.count)
		return NIL;
	item = self.array[index];
	self.count--;
	for (i = index; i < self.count; i++)
		self.array[i] = self.array[i + 1];
	return item;
}

- (void []) insertItemAt: (integer) index item:(void []) item
{
	local integer i;
	if (index == -1)
		index = self.count -1;
	if (index < 0 || index >= self.count)
		return NIL;
	if (self.count == self.size) {
		self.size += self.incr;
		self.array = (void[][])obj_realloc (self.array,
											self.size * @sizeof (void []));
	}
	for (i = self.count; i > index; i--)
		self.array[i] = self.array[i - 1];
	self.array[index] = item;
	self.count++;
	return item;
}

@end
