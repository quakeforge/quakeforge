typedef struct Point_s {
	int         x;
	int         y;
} Point;

typedef struct Extent_s {
	int         width;
	int         height;
} Extent;

typedef struct Rect_s {
	Point       offset;
	Extent      extent;
} Rect;

typedef struct Obj {
	struct Obj *w;
	struct Obj *a;
	struct Obj *sb;
	struct Obj *st;
} Obj;

Extent get_size(Obj *self);
Obj *set_rect(Obj *self, Rect r);
Obj *alloc();
Obj *retain(Obj *self);
void add(Obj *self, Obj *w);
Obj *create(Obj *self, int len, Point at);

Obj *init(Obj *self)
{
	Extent s = get_size (self);
	self.a = retain(alloc());
	self.w = set_rect (self, {nil, s});
	self.sb = create (self, s.height - 1, {s.width - 1, 1});
	return self;
}

Extent get_size(Obj *self) { return { 69, 42 }; }
Obj *set_rect(Obj *self, Rect r) { return nil; }
Obj *alloc() { return nil; };
Obj *retain(Obj *self) { return self; }
void add(Obj *self, Obj *w) {};
int x, l;
Obj *create(Obj *self, int len, Point at) { l = len; x = at.x; return nil; }

int main ()
{
	Obj o = {};
	init (&o);
	if (l != 41 || x != 68) {
		return 1;
	}
	return 0;
}
