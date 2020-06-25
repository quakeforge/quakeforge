typedef struct Extent_s {
	int width;
	int height;
} Extent;

extern Extent get_size (void);
extern int getlen(void);

int main (void)
{
	int x = get_size().width - getlen() - 1;
	return x != 29;
}

Extent get_size (void)
{
	return {38, 8};
}

int getlen (void)
{
	return 8;
}
